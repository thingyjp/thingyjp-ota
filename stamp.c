#define GETTEXT_PACKAGE "gtk20"
#include <json-glib/json-glib.h>
#include "args.h"
#include "jsonbuilderutils.h"
#include "stamp.h"
#include "manifest.h"
#include "ota.h"
#include "utils.h"

static void stamp_findversion(gpointer data, gpointer user_data) {
	struct manifest_image* image = data;
	gint* param_imageversion = user_data;
	*param_imageversion = MAX(image->version + 1, *param_imageversion);
}

int main(int argc, char** argv) {
	int ret = 0;
	gchar* arg_rootdir = NULL;
	gchar* arg_configdir = OTA_CONFIGDIR_DEFAULT;
	gchar* arg_repodir = NULL;
	gint param_imageversion = -1;
	gchar* arg_repouuid = NULL;

	GError* error = NULL;
	GOptionEntry entries[] = { ARGS_ROOTDIR, ARGS_CONFIGDIR, ARGS_REPODIR,
	ARGS_PARAMETER_IMAGEVERSION, ARGS_REPOUUID, { NULL } };
	GOptionContext* optioncontext = g_option_context_new(NULL);
	g_option_context_add_main_entries(optioncontext, entries, GETTEXT_PACKAGE);
	g_option_context_set_description(optioncontext, "stamp image");

	if (!g_option_context_parse(optioncontext, &argc, &argv, &error)) {
		g_print("option parsing failed: %s\n", error->message);
		ret = 1;
		goto err_args;
	}

	if (arg_rootdir == NULL) {
		g_message("you must specify the root directory");
		ret = 1;
		goto err_args;
	}

	// check we either have a repouuid to use or a repo to get it from
	if (arg_repouuid == NULL && arg_repodir == NULL) {
		g_message(
				"you must specify the repo uuid or the directory of the repo");
		goto err_args;
	}

	// check we either have a version to use or a repo to work it out from
	if (param_imageversion == -1 && arg_repodir == NULL) {
		g_message("you must specify the repodir to use auto-versioning");
		goto err_args;
	}

	if (param_imageversion == -1 || arg_repouuid == NULL) {
		gchar* manifestpath = buildpath(arg_repodir, OTA_MANIFEST, NULL);
		struct manifest_manifest* manifest = manifest_load(manifestpath);
		if (manifest == NULL)
			goto err_loadmanifest;

		// get the repouuid from the manifest
		if (arg_repouuid == NULL) {
			arg_repouuid = g_strdup(manifest->uuid);
			g_message("using uuid %s from manifest", arg_repouuid);
		}

		// find the newest image in the repo and increment
		if (param_imageversion == -1) {
			param_imageversion = 0;
			g_ptr_array_foreach(manifest->images, stamp_findversion,
					&param_imageversion);
			g_message("using version %d", param_imageversion);
		}

		manifest_free(manifest);
	}

	GString* pathgstr = g_string_new(NULL);
	g_string_printf(pathgstr, "%s/%s/%s", arg_rootdir, arg_configdir,
	STAMPFILE);
	gchar* path = g_string_free(pathgstr, FALSE);
	g_message("writing stamp to %s", path);

	JsonBuilder* builder = json_builder_new();
	json_builder_begin_object(builder);
	JSONBUILDER_ADD_STRING(builder, STAMP_JSONFIELD_UUID,
			g_uuid_string_random());
	JSONBUILDER_ADD_STRING(builder, STAMP_JSONFIELD_REPOUUID, arg_repouuid);
	JSONBUILDER_ADD_INT(builder, STAMP_JSONFIELD_VERSION, param_imageversion);
	json_builder_end_object(builder);
	jsonbuilder_writetofile(builder, TRUE, path);

	err_loadmanifest: //
	err_args: //
	return ret;
}
