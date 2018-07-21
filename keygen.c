#define GETTEXT_PACKAGE "gtk20"
#include "crypto.h"
#include "args.h"

int main(int argc, char** argv) {
	int ret = 0;

	gchar* keysdir = NULL;
	GError* error = NULL;
	GOptionEntry entries[] = { ARGS_KEYDIR, { NULL } };
	GOptionContext* optioncontext = g_option_context_new(NULL);
	g_option_context_add_main_entries(optioncontext, entries,
	GETTEXT_PACKAGE);
	if (!g_option_context_parse(optioncontext, &argc, &argv, &error)) {
		g_print("option parsing failed: %s\n", error->message);
		ret = 1;
		goto err_args;
	}

	if (keysdir == NULL) {
		g_message("you must pass a directory to populate with keys");
		goto err_args;
	}

	if (g_mkdir_with_parents(keysdir, 0700)) {
		g_message("failed to create keys directory");
		goto err_mkdir;
	}

	GString* pubkeypathstr = g_string_new(keysdir);
	g_string_append_printf(pubkeypathstr, "/%s", CRYPTO_KEYNAME_RSA_PUB);
	gchar* pubkeypath = g_string_free(pubkeypathstr, FALSE);

	GString* privkeypathstr = g_string_new(keysdir);
	g_string_append_printf(privkeypathstr, "/%s", CRYPTO_KEYNAME_RSA_PRIV);
	gchar* privkeypath = g_string_free(privkeypathstr, FALSE);

	struct crypto_keys keys;
	crypto_keygen(&keys);
	crypto_writekeys(&keys, pubkeypath, privkeypath);
	err_mkdir: //
	err_args: //
	return ret;
}
