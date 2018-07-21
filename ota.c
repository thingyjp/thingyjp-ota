#define GETTEXT_PACKAGE "gtk20"
#include "ota.h"
#include "teenyhttp.h"
#include "jsonparserutils.h"
#include "crypto.h"
#include "manifest.h"

#define MANIFEST_CONTENTTYPE      "application/json"

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

static void updatemanifest() {
	GByteArray* manifestbuffer = g_byte_array_new();
	if (!teenyhttp_get("thingy.jp", "/ota/spibeagle/" OTA_MANIFEST,
			responsecallback, MANIFEST_CONTENTTYPE, bytebuffercallback,
			manifestbuffer)) {
		g_message("failed to fetch manifest");
		goto err_fetchmanifest;
	}

	err_fetchmanifest: //
	g_byte_array_free(manifestbuffer, TRUE);
}

int main(int argc, char** argv) {
	int ret = 0;
	gchar* host = "thingy.jp";
	gchar* directory = "/ota/spibeagle";

	GError* error = NULL;
	GOptionEntry entries[] = { { "host", 'h', 0, G_OPTION_ARG_STRING, &host,
			"OTA server host", NULL }, { "dir", 'd', 0, G_OPTION_ARG_STRING,
			&directory, "directory", NULL }, { NULL } };
	GOptionContext* optioncontext = g_option_context_new(NULL);
	g_option_context_add_main_entries(optioncontext, entries,
	GETTEXT_PACKAGE);
	if (!g_option_context_parse(optioncontext, &argc, &argv, &error)) {
		g_print("option parsing failed: %s\n", error->message);
		ret = 1;
		goto err_args;
	}

	teenyhttp_init();

	updatemanifest();

	err_args: //
	return ret;
}

