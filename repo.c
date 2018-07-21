#define GETTEXT_PACKAGE "gtk20"
#include <json-glib/json-glib.h>
#include "jsonbuilderutils.h"
#include "crypto.h"
#include "manifest.h"
#include "args.h"

static const enum manifest_signaturetype sigtypes[] = { OTA_SIGTYPE_RSASHA256,
		OTA_SIGTYPE_RSASHA512 };
static gchar* repodir = NULL;
static gchar* keysdir = NULL;
static gchar* manifestpath;
static gchar* sigpath;

static gchar* buildpath(const gchar* dir, const gchar* file) {
	GString* pathgstr = g_string_new(dir);
	g_string_append_c(pathgstr, '/');
	g_string_append(pathgstr, file);
	return g_string_free(pathgstr, FALSE);
}

static struct manifest_manifest* repo_manifest_load() {
	struct manifest_manifest* manifest = manifest_new();
	gchar* existingmanifest;
	gsize existingmanifestsz;
	if (g_file_get_contents(manifestpath, &existingmanifest,
			&existingmanifestsz, NULL)) {
		if (!manifest_deserialise_into(manifest, existingmanifest,
				existingmanifestsz)) {
			g_message("existing manifest is corrupt");
		}
		g_free(existingmanifest);
	}
	return manifest;
}

static struct crypto_keys* repo_keys_load() {
	GString* pubkeypathstr = g_string_new(keysdir);
	g_string_append_printf(pubkeypathstr, "/%s", CRYPTO_KEYNAME_RSA_PUB);
	gchar* pubkeypath = g_string_free(pubkeypathstr, FALSE);
	GString* privkeypathstr = g_string_new(keysdir);
	g_string_append_printf(privkeypathstr, "/%s", CRYPTO_KEYNAME_RSA_PRIV);
	gchar* privkeypath = g_string_free(privkeypathstr, FALSE);
	struct crypto_keys* keys = crypto_readkeys(pubkeypath, privkeypath);
	return keys;
}

static void repo_image_list_printimage(gpointer data, gpointer user_data) {
	struct manifest_image* image = data;
	g_message("%s", image->name);
}

static void repo_image_list() {
	struct manifest_manifest* manifest = repo_manifest_load();
	g_message("have %d images", manifest->images->len);
	g_ptr_array_foreach(manifest->images, repo_image_list_printimage, NULL);
	manifest_free(manifest);
}

static void repo_updatemanifest(struct manifest_manifest* manifest,
		struct crypto_keys* keys) {
	JsonBuilder* builder = manifest_serialise(manifest);
	gsize manifestjsonlen;
	gchar* manifestjson = jsonbuilder_freetostring(builder, &manifestjsonlen,
	TRUE);

	JsonBuilder* sigbuilder = json_builder_new();
	json_builder_begin_array(sigbuilder);
	for (int i = 0; i < G_N_ELEMENTS(sigtypes); i++) {
		struct manifest_signature* sig = crypto_sign(sigtypes[i], keys,
				(guint8*) manifestjson, manifestjsonlen);
		manifest_signature_serialise(sigbuilder, sig);
	}
	json_builder_end_array(sigbuilder);

	g_file_set_contents(manifestpath, manifestjson, manifestjsonlen, NULL);
	jsonbuilder_writetofile(sigbuilder, TRUE, sigpath);
}

static void repo_image_add(const gchar* imagepath) {
	struct manifest_manifest* manifest = repo_manifest_load();
	struct crypto_keys* keys = repo_keys_load();

	struct manifest_image* image = manifest_image_new();
	g_ptr_array_add(manifest->images, image);

	gchar* imagedata;
	gsize imagesz;
	g_file_get_contents(imagepath, &imagedata, &imagesz, NULL);

	for (int i = 0; i < G_N_ELEMENTS(sigtypes); i++) {
		struct manifest_signature* imagesig = crypto_sign(sigtypes[i], keys,
				imagedata, imagesz);
		g_ptr_array_add(image->signatures, imagesig);
	}

	repo_updatemanifest(manifest, keys);
}

static void repo_image_delete(guint index) {
	struct manifest_manifest* manifest = repo_manifest_load();
	struct crypto_keys* keys = repo_keys_load();

	if (index >= manifest->images->len) {
		g_message("bad image index");
		goto err_badindex;
	}

	g_ptr_array_remove_index(manifest->images, index);

	repo_updatemanifest(manifest, keys);
	err_badindex: //
	return;
}

static void repo_verify_verifyimage(gpointer data, gpointer user_data) {
	struct manifest_image* image = data;
	g_message("checking image %s...", image->name);
	gchar* imagepath = buildpath(repodir, image->name);
	gsize imagesz;
	gchar* imagedata;
	GError* err = NULL;
	if (!g_file_get_contents(imagepath, &imagedata, &imagesz, &err)) {
		g_message("failed to load image data; %s", err->message);
		goto err_loadimage;
	}

	err_loadimage: //
	g_free(imagepath);
}

static void repo_verify() {
	struct manifest_manifest* manifest = repo_manifest_load();
	struct crypto_keys* keys = repo_keys_load();
	g_ptr_array_foreach(manifest->images, repo_verify_verifyimage, NULL);
	manifest_free(manifest);
}

int main(int argc, char** argv) {

	int ret = 0;

	gboolean action_list = FALSE;
	gboolean action_add = FALSE;
	gboolean action_update = FALSE;
	gboolean action_delete = FALSE;
	gboolean action_verify = FALSE;
	gchar* param_imagepath = NULL;
	gint param_imageindex = -1;

	GError* error = NULL;
	GOptionEntry entries[] = { ARGS_REPODIR, ARGS_KEYDIR,
	ARGS_ACTION_ADD, ARGS_ACTION_LIST, ARGS_ACTION_UPDATE, ARGS_ACTION_DELETE,
	ARGS_ACTION_VERIFY, ARGS_PARAMETER_IMAGEPATH, ARGS_PARAMETER_IMAGEINDEX, {
	NULL } };
	GOptionContext* optioncontext = g_option_context_new(NULL);
	g_option_context_add_main_entries(optioncontext, entries,
	GETTEXT_PACKAGE);
	if (!g_option_context_parse(optioncontext, &argc, &argv, &error)) {
		g_print("option parsing failed: %s\n", error->message);
		ret = 1;
		goto err_args;
	}

	if (repodir == NULL) {
		g_message("you must pass a directory for the repo");
		goto err_args;
	}

	if (keysdir == NULL) {
		g_message("you must pass a directory containing the keys");
		goto err_args;
	}

	if (action_list + action_add + action_update + action_delete + action_verify
			!= 1) {
		g_message("you must specify one action");
		goto err_args;
	}

	if (g_mkdir_with_parents(repodir, 0700) > 0) {
		g_message("failed to create repo dir");
		goto err_createdir;
	}

	manifestpath = buildpath(repodir, OTA_MANIFEST);
	sigpath = buildpath(repodir, OTA_SIG);

	if (action_list)
		repo_image_list();
	else if (action_add) {
		if (param_imagepath == NULL) {
			g_message("you must pass the path of the image to add");
			goto err_args;
		}
		repo_image_add(param_imagepath);
	} else if (action_delete) {
		if (param_imageindex < 0) {
			g_message("you must pass a valid image index");
			goto err_args;
		}
		repo_image_delete(param_imageindex);
	} else if (action_verify)
		repo_verify();

	err_createdir: //
	err_args: //

	return ret;
}
