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
static gchar* keysdir = NULL;
static struct manifest_manifest* manifest = NULL;
static guint currentversion = 0;
static gint64 manifestfetchedat;

static gboolean responsecallback(const struct teenyhttp_response* response,
		gpointer user_data) {
	const gchar* contenttype = user_data;
	return response->code == 200
			&& (strcmp(contenttype, response->contenttype) == 0);
}

static gboolean bytebuffercallback(guint8* data, gsize len, gpointer user_data) {
	GByteArray* buffer = user_data;
	g_byte_array_append(buffer, data, len);
	return TRUE;
}

struct checksigcntx {
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

	g_message("validating manifest with %s",
			manifest_signaturetypestrings[sig->type]);
	cntx->cont = crypto_verify(sig, cntx->keys, cntx->data, cntx->len);
	if (!cntx->cont) {
		g_message("sig check failed");
	}
}

static void updatemanifest() {
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

	gchar* pubkeypath = buildpath(keysdir, CRYPTO_KEYNAME_RSA_PUB);
	gchar* privkeypath = buildpath(keysdir, CRYPTO_KEYNAME_RSA_PRIV);
	struct crypto_keys* keys = crypto_readkeys(pubkeypath, privkeypath);
	g_free(pubkeypath);
	g_free(privkeypath);
	if (keys == NULL) {
		g_message("failed to load keys");
		goto err_loadkeys;
	}

	struct checksigcntx chksigcntx = { .data = manifestbuffer->data, .len =
			manifestbuffer->len, .keys = keys, .cont = TRUE };
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

	if (manifest != NULL)
		manifest_free(manifest);
	manifest = newmanifest;
	manifestfetchedat = g_get_real_time();

	err_manifestparse: //
	err_manifestsig: //
	err_loadkeys: //
	err_fetchmanifest: //
	g_byte_array_free(manifestbuffer, TRUE);
	err_parsesig: //
	err_fetchsig: //
	g_byte_array_free(sigbuffer, TRUE);
}

static void checkforupdate() {

}

static gboolean timeout(gpointer user_data) {
	updatemanifest();
	checkforupdate();
	return G_SOURCE_CONTINUE;
}

int main(int argc, char** argv) {
	int ret = 0;
	host = "thingy.jp";
	path = "/ota/spibeagle";

	GError* error = NULL;
	GOptionEntry entries[] = { ARGS_HOST, ARGS_PATH, ARGS_KEYDIR, { NULL } };
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

	teenyhttp_init();
	timeout(NULL);

	GMainLoop* mainloop = g_main_loop_new(NULL, FALSE);
	g_timeout_add_seconds(60 * 10, timeout, NULL);
	g_main_loop_run(mainloop);

	err_args: //
	return ret;
}

