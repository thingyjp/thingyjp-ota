#define GETTEXT_PACKAGE "gtk20"
#include "ota.h"
#include "args.h"
#include "teenyhttp.h"
#include "jsonparserutils.h"
#include "crypto.h"
#include "manifest.h"
#include "utils.h"

static gchar* host;
static gchar* path;
static struct crypto_keys* keys;
static struct manifest_manifest* manifest = NULL;
static guint currentversion = 0;
static gint64 manifestfetchedat;
static struct manifest_image* targetimage = NULL;
static gboolean waitingtoreboot = FALSE;

static gboolean responsecallback(const struct teenyhttp_response* response,
		gpointer user_data) {
	const gchar* contenttype = user_data;
	return response->code == 200
			&& (strcmp(contenttype, response->contenttype) == 0);
}

static gboolean bytebuffercallback(guint8* data, gsize len, gpointer user_data) {
	GByteArray* buffer = user_data;
	g_byte_array_append(buffer, data, len);
	g_message("have %u bytes", buffer->len);
	return TRUE;
}

struct checksigcntx {
	const gchar* what;
	guint8* data;
	gsize len;
	struct crypto_keys* keys;
	gboolean cont;
};

static void checksig(gpointer data, gpointer user_data) {
	struct manifest_signature* sig = data;
	struct checksigcntx* cntx = user_data;

	if (!cntx->cont)
		return;

	g_message("validating %s with %s", cntx->what,
			manifest_signaturetypestrings[sig->type]);
	cntx->cont = crypto_verify(sig, cntx->keys, cntx->data, cntx->len);
	if (!cntx->cont) {
		g_message("sig check failed");
	}
}

static void updatemanifest() {
	if (targetimage != NULL) {
		g_message("target image selected, not updating manifest");
		return;
	}

	GByteArray* sigbuffer = g_byte_array_new();
	if (!teenyhttp_get(host, "/ota/spibeagle/" OTA_SIG, responsecallback,
	MANIFEST_CONTENTTYPE, bytebuffercallback, sigbuffer)) {
		g_message("failed to fetch sig");
		goto err_fetchsig;
	}

	GPtrArray* sigs = manifest_signatures_deserialise(sigbuffer->data,
			sigbuffer->len);
	if (sigs == NULL) {
		g_message("failed to parse signatures or no usable signatures");
		goto err_parsesig;
	}

	GByteArray* manifestbuffer = g_byte_array_new();
	if (!teenyhttp_get(host, "/ota/spibeagle/" OTA_MANIFEST, responsecallback,
	MANIFEST_CONTENTTYPE, bytebuffercallback, manifestbuffer)) {
		g_message("failed to fetch manifest");
		goto err_fetchmanifest;
	}

	struct checksigcntx chksigcntx = { .what = "manifest", .data =
			manifestbuffer->data, .len = manifestbuffer->len, .keys = keys,
			.cont = TRUE };
	g_ptr_array_foreach(sigs, checksig, &chksigcntx);
	if (!chksigcntx.cont) {
		g_message("manifest sig check failed");
		goto err_manifestsig;
	}

	struct manifest_manifest* newmanifest = manifest_deserialise(
			manifestbuffer->data, manifestbuffer->len);
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

	out: //
	err_manifestparse: //
	err_manifestsig: //
	err_fetchmanifest: //
	g_byte_array_free(manifestbuffer, TRUE);
	err_parsesig: //
	err_fetchsig: //
	g_byte_array_free(sigbuffer, TRUE);
}

static void ota_image_findcandidate(gpointer data, gpointer user_data) {
	struct manifest_image* image = data;
	GPtrArray* candidates = user_data;
	g_message("checking image %s", image->uuid);
	if (!image->enabled) {
		g_message("image isn't enabled");
		return;
	} else if (image->version <= currentversion) {
		g_message("image version %d isn't higher than %d", image->version,
				currentversion);
		return;
	}
	g_ptr_array_add(candidates, image);
}

static gint ota_image_score(gconstpointer a, gconstpointer b) {
	const struct manifest_image* left = a;
	const struct manifest_image* right = b;
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
		g_message("scheduled update to image %s", targetimage->uuid);
	}
	g_ptr_array_free(candidates, TRUE);
}

static void ota_tryupdate() {
	if (targetimage == NULL)
		return;

	gchar* imagepath = buildpath(path, targetimage->uuid);
	GByteArray* imagebuffer = g_byte_array_new();
	teenyhttp_get_simple(host, imagepath, bytebuffercallback, imagebuffer);

	if (imagebuffer->len != targetimage->size) {
		g_message("downloaded image size doesn't match manifest %u vs %u",
				imagebuffer->len, targetimage->size);
		goto err_imagelen;
	}

	struct checksigcntx cntx = { .what = "image", .data = imagebuffer->data,
			.len = imagebuffer->len, .keys = keys, .cont = TRUE };
	g_ptr_array_foreach(targetimage->signatures, checksig, &cntx);
	if (!cntx.cont) {
		g_message("image signature verification failed");
		goto err_imagesig;
	}

	g_message("erasing passive partition...");

	g_message("installing image...");

	g_message("scheduling reboot...");

	waitingtoreboot = TRUE;

	err_imagelen: //
	err_imagesig: //
	g_byte_array_free(imagebuffer, TRUE);
}

static gboolean timeout(gpointer user_data) {
	updatemanifest();
	ota_checkimages();
	ota_tryupdate();
	return G_SOURCE_CONTINUE;
}

int main(int argc, char** argv) {
	int ret = 0;
	host = "thingy.jp";
	path = "/ota/spibeagle";
	gchar* keysdir = NULL;
	gchar** mtds = NULL;
	gboolean dryrun = FALSE;

	GError* error = NULL;
	GOptionEntry entries[] = { ARGS_HOST, ARGS_PATH, ARGS_KEYDIR, ARGS_MTD,
			ARGS_DRYRUN, {
	NULL } };
	GOptionContext* optioncontext = g_option_context_new(NULL);
	g_option_context_add_main_entries(optioncontext, entries,
	GETTEXT_PACKAGE);
	if (!g_option_context_parse(optioncontext, &argc, &argv, &error)) {
		g_print("option parsing failed: %s\n", error->message);
		ret = 1;
		goto err_args;
	}

	if (keysdir == NULL) {
		g_message("you must pass the path of the keys directory");
		goto err_args;
	}

	gchar* pubkeypath = buildpath(keysdir, CRYPTO_KEYNAME_RSA_PUB);
	keys = crypto_readkeys(pubkeypath, NULL);
	g_free(pubkeypath);
	if (keys == NULL) {
		g_message("failed to load keys");
		goto err_loadkeys;
	}

	teenyhttp_init();
	timeout(NULL);

	GMainLoop* mainloop = g_main_loop_new(NULL, FALSE);
	g_timeout_add_seconds(60 * 10, timeout, NULL);
	g_main_loop_run(mainloop);

	err_loadkeys: //
	err_args: //
	return ret;
}
