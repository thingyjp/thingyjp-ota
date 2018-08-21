#define GETTEXT_PACKAGE "gtk20"
#include <unistd.h>
#include <sys/reboot.h>
#include <thingymcconfig/client_glib.h>
#include <thingymcconfig/logging.h>
#include <teenynet/http.h>
#include "ota.h"
#include "args.h"
#include "jsonparserutils.h"
#include "crypto.h"
#include "manifest.h"
#include "utils.h"
#include "mtd.h"
#include "stamp.h"

static gchar* host;
static gchar* path;
static struct crypto_keys* keys;
static struct manifest_manifest* manifest = NULL;
static guint currentversion = 0;
static gint64 manifestfetchedat;
static struct manifest_image* targetimage = NULL;
static gboolean waitingtoreboot = FALSE;
static gboolean dryrun = FALSE;
static gboolean force = FALSE;
static gchar** mtds = NULL;
static guint timeoutsource = 0;
static ThingyMcConfigClient* client;
static gboolean connectivitystate = FALSE;

static gboolean responsecallback(const struct teenyhttp_response* response,
		gpointer user_data) {
	const gchar* contenttype = user_data;
	return response->code == 200
			&& (strcmp(contenttype, response->contenttype) == 0);
}

static GHashTable* munchbootargs() {
	GHashTable* table = g_hash_table_new_full(g_str_hash, g_str_equal, g_free,
			g_free);
	// use the uboot supplied boot args as uboot fills this in
	// even if the kernel ignores it
	const gchar* fdtnode = "/sys/firmware/devicetree/base/chosen/bootargs";
	gsize bootargssz;
	gchar* bootargs;
	if (!g_file_get_contents(fdtnode, &bootargs, &bootargssz, NULL)) {
		g_message("failed to read bootargs from dt");
		goto err_readbootargs;
	}

	GRegex* keyregexp = g_regex_new("(ota\.)([a-z]*)=([0-9,a-f,x]*)", 0, 0,
	NULL);
	g_assert(keyregexp != NULL);
	GMatchInfo* match;
	if (g_regex_match_full(keyregexp, bootargs, bootargssz, 0, 0, &match,
	NULL)) {
		while (g_match_info_matches(match)) {
			gchar* key = g_match_info_fetch(match, 2);
			gchar* value = g_match_info_fetch(match, 3);
			g_message("ota.%s=%s", key, value);
			g_hash_table_insert(table, key, value);
			g_match_info_next(match, NULL);
		}
	}
	g_match_info_free(match);
	g_regex_unref(keyregexp);
	err_readbootargs: //
	return table;
}

static void onendtoendconnectionsuccess() {
	if (!connectivitystate) {
		connectivitystate = TRUE;
		thingymcconfig_client_sendconnectivitystate(client, connectivitystate);
	}
}

static void updatemanifest() {
	if (targetimage != NULL) {
		g_message("target image selected, not updating manifest");
		return;
	}

	gchar* sigpath = buildpath(path, OTA_SIG, NULL);
	GByteArray* sigbuffer = g_byte_array_new();
	if (!teenyhttp_get(host, sigpath, responsecallback,
	MANIFEST_CONTENTTYPE, teenyhttp_datacallback_bytebuffer, sigbuffer)) {
		g_message("failed to fetch sig");
		goto err_fetchsig;
	}

	GPtrArray* sigs = manifest_signatures_deserialise((gchar*) sigbuffer->data,
			sigbuffer->len);
	if (sigs == NULL) {
		g_message("failed to parse signatures or no usable signatures");
		goto err_parsesig;
	}

	gchar* manifestpath = buildpath(path, OTA_MANIFEST, NULL);
	GByteArray* manifestbuffer = g_byte_array_new();
	if (!teenyhttp_get(host, manifestpath, responsecallback,
	MANIFEST_CONTENTTYPE, teenyhttp_datacallback_bytebuffer, manifestbuffer)) {
		g_message("failed to fetch manifest");
		goto err_fetchmanifest;
	}

	struct crypto_checksigcntx chksigcntx = { .what = "manifest", .data =
			manifestbuffer->data, .len = manifestbuffer->len, .keys = keys,
			.cont = TRUE };
	g_ptr_array_foreach(sigs, crypto_checksig, &chksigcntx);
	if (!chksigcntx.cont) {
		g_message("manifest sig check failed");
		goto err_manifestsig;
	}

	struct manifest_manifest* newmanifest = manifest_deserialise(
			(gchar*) manifestbuffer->data, manifestbuffer->len);
	if (newmanifest == NULL) {
		g_message("failed to parse manifest");
		goto err_manifestparse;
	}

	if (manifest != NULL) {
		if (newmanifest->serial <= manifest->serial) {
			g_message(
					"new manifest is older or the same version as the current one, ignoring");
			manifest_free(newmanifest);
			goto out;
		} else
			manifest_free(manifest);
	}
	manifest = newmanifest;
	manifestfetchedat = g_get_real_time();
	onendtoendconnectionsuccess();

	out: //
	err_manifestparse: //
	err_manifestsig: //
	err_fetchmanifest: //
	g_free(manifestpath);
	g_byte_array_free(manifestbuffer, TRUE);
	err_parsesig: //
	err_fetchsig: //
	g_free(sigpath);
	g_byte_array_free(sigbuffer, TRUE);
}

static void ota_image_findcandidate(gpointer data, gpointer user_data) {
	struct manifest_image* image = data;
	GPtrArray* candidates = user_data;
	g_message("checking image %s", image->uuid);
	if (!image->enabled) {
		g_message("image isn't enabled");
		return;
	} else if (!force && image->version <= currentversion) {
		g_message("image version %d isn't higher than %d", image->version,
				currentversion);
		return;
	}
	g_ptr_array_add(candidates, image);
}

static gint ota_image_score(gconstpointer a, gconstpointer b) {
	const struct manifest_image* left = *((struct manifest_image**) a);
	const struct manifest_image* right = *((struct manifest_image**) b);
	return left->version - right->version;
}

static void ota_checkimages() {
	if (manifest == NULL || targetimage != NULL)
		return;

	g_message("looking for update..");

	if (manifest->images->len == 0) {
		g_message("manifest contains no images");
		return;
	}

	GPtrArray* candidates = g_ptr_array_new();
	g_ptr_array_foreach(manifest->images, ota_image_findcandidate, candidates);
	if (candidates->len > 0) {
		g_message("have %d candidates", candidates->len);
		g_ptr_array_sort(candidates, ota_image_score);
		targetimage = g_ptr_array_index(candidates, candidates->len - 1);
		g_message("scheduled update to image %s(%u)", targetimage->uuid,
				targetimage->version);
	}
	g_ptr_array_free(candidates, TRUE);
}

static const gchar* ota_findpassive() {
	GHashTable* bootargs = munchbootargs();
	gchar* mtd = mtds[0];
	if (!g_hash_table_contains(bootargs, "part")) {
		g_message(
				"failed to find image offset in bootargs, first mtd will be used");
		goto err_nopartkey;
	}

	gchar* partkey = g_hash_table_lookup(bootargs, "part");
	guint64 offset = g_ascii_strtoull(partkey, NULL, 16);
	gchar* activepart = mtd_foroffset(offset);
	if (activepart == NULL) {
		g_message(
				"failed to find partition for offset %u, first mtd will be used",
				(unsigned ) offset);
		goto err_badoffset;
	}
	g_message("active partition is %s", activepart);

	gchar* passive = NULL;
	for (gchar** part = mtds; *part != NULL; part++) {
		if (strcmp(*part, activepart) != 0) {
			passive = *part;
			break;
		}
	}
	g_assert(passive != NULL);

	g_message("selected %s as passive partition", passive);
	mtd = passive;

	err_nopassive: //
	err_badoffset: //
	err_nopartkey: //
	g_hash_table_unref(bootargs);
	return mtd;
}

static void ota_tryupdate() {
	if (targetimage == NULL)
		return;

	gchar* imagepath = buildpath(path, targetimage->uuid, NULL);
	GByteArray* imagebuffer = g_byte_array_new();
	teenyhttp_get_simple(host, imagepath, teenyhttp_datacallback_bytebuffer,
			imagebuffer);

	if (imagebuffer->len != targetimage->size) {
		g_message(
				"downloaded image size doesn't match manifest %u vs %" G_GSIZE_FORMAT,
				imagebuffer->len, targetimage->size);
		goto err_imagelen;
	}

	struct crypto_checksigcntx cntx = { .what = "image", .data =
			imagebuffer->data, .len = imagebuffer->len, .keys = keys, .cont =
	TRUE };
	g_ptr_array_foreach(targetimage->signatures, crypto_checksig, &cntx);
	if (!cntx.cont) {
		g_message("image signature verification failed");
		goto err_imagesig;
	}

	if (!dryrun) {
		const gchar* mtd = ota_findpassive();
		g_message("erasing passive partition...");
		if (!mtd_erase(mtd))
			goto err_mtderase;

		g_message("installing image...");
		if (!mtd_writeimage(mtd, imagebuffer->data, imagebuffer->len))
			goto err_mtdwrite;

		g_message("scheduling reboot...");
		waitingtoreboot = TRUE;
		reboot(RB_AUTOBOOT);
	}

	err_mtdwrite: //
	err_mtderase: //
	err_imagelen: //
	err_imagesig: //
	g_byte_array_free(imagebuffer, TRUE);
}

static gboolean timeout(gpointer user_data) {
	updatemanifest();
	ota_checkimages();
	ota_tryupdate();
	return waitingtoreboot ? G_SOURCE_REMOVE : G_SOURCE_CONTINUE;
}

static void ota_daemon_connected(void) {
	thingymcconfig_client_sendappstate(client);
}

static void ota_supplicant_connected(void) {
	timeout(NULL);
	timeoutsource = g_timeout_add_seconds(60 * 10, timeout, NULL);
}

static void ota_supplicant_disconnected(void) {
	if (timeoutsource != 0) {
		g_source_remove(timeoutsource);
		timeoutsource = 0;
	}
}

static void ota_daemon_disconnected(void) {

}

int main(int argc, char** argv) {
	int ret = 0;
	host = "thingy.jp";
	path = "/ota/spibeagle";
	gchar* arg_configdir = OTA_CONFIGDIR_DEFAULT;
	gchar* logfile = NULL;

	GError* error = NULL;
	GOptionEntry entries[] = { ARGS_HOST, ARGS_PATH, ARGS_CONFIGDIR, ARGS_MTD,
	ARGS_DRYRUN, ARGS_FORCE, ARGS_LOG, { NULL } };
	GOptionContext* optioncontext = g_option_context_new(NULL);
	g_option_context_add_main_entries(optioncontext, entries,
	GETTEXT_PACKAGE);
	if (!g_option_context_parse(optioncontext, &argc, &argv, &error)) {
		g_print("option parsing failed: %s\n", error->message);
		ret = 1;
		goto err_args;
	}

	logging_init(logfile);

	if (!dryrun) {
		int nummtds = mtds != NULL ? g_strv_length(mtds) : 0;
		if (nummtds < 2) {
			g_message("you must specify at least two mtd partitions");
			goto err_args;
		} else if (nummtds > 2) {
			//TODO some message about not using more than two mtds here
		}

		if (!mtd_init(mtds))
			goto err_mtdinit;
	}

	gchar* pubkeypath = buildpath(arg_configdir, OTA_CONFIGDIR_SUBDIR_KEYS,
	CRYPTO_KEYNAME_RSA_PUB, NULL);
	keys = crypto_readkeys(pubkeypath, NULL);
	g_free(pubkeypath);
	if (keys == NULL) {
		g_message("failed to load keys");
		goto err_loadkeys;
	}

	gchar* stamppath = buildpath(arg_configdir, STAMPFILE, NULL);
	struct stamp_stamp* stamp = stamp_loadstamp(stamppath);
	if (stamp == NULL)
		goto err_loadstamp;
	currentversion = stamp->version;
	stamp_freestamp(stamp);

	teenyhttp_init();

	client = thingymcconfig_client_new("ota");
	g_signal_connect(client, THINGYMCCONFIG_DETAILEDSIGNAL_DAEMON_CONNECTED,
			ota_daemon_connected, NULL);
	g_signal_connect(client, THINGYMCCONFIG_DETAILEDSIGNAL_SUPPLICANT_CONNECTED,
			ota_supplicant_connected, NULL);
	g_signal_connect(client,
			THINGYMCCONFIG_DETAILEDSIGNAL_SUPPLICANT_DISCONNECTED,
			ota_supplicant_disconnected, NULL);
	g_signal_connect(client, THINGYMCCONFIG_DETAILEDSIGNAL_DAEMON_DISCONNECTED,
			ota_daemon_disconnected, NULL);
	thingymcconfig_client_lazyconnect(client);

	GMainLoop* mainloop = g_main_loop_new(NULL, FALSE);

	g_main_loop_run(mainloop);

	thingymcconfig_client_free(client);

	err_loadstamp: //
	err_loadkeys: //
	err_mtdinit: //
	err_args: //
	return ret;
}
