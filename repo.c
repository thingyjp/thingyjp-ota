#define GETTEXT_PACKAGE "gtk20"
#include <json-glib/json-glib.h>
#include "jsonbuilderutils.h"
#include "jsonparserutils.h"
#include "crypto.h"
#include "manifest.h"
#include "args.h"
#include "utils.h"
#include "stamp.h"

static const enum manifest_signaturetype sigtypes[] = { OTA_SIGTYPE_RSASHA256,
		OTA_SIGTYPE_RSASHA512 };
static gchar* arg_repodir = NULL;
static gchar* keysdir = NULL;
static gchar* manifestpath;
static gchar* sigpath;

static gchar* buildsigkey(struct manifest_signature* sig) {
	GString* s = g_string_new(NULL);
	g_string_printf(s, "%s:%s", manifest_signaturetypestrings[sig->type],
			sig->data);
	return g_string_free(s, FALSE);
}

static void createsigimagetable_sig(gpointer data, gpointer user_data) {
	struct manifest_signature* sig = data;
	GHashTable* t = user_data;
	g_hash_table_add(t, buildsigkey(sig));
}

static void createsigimagetable_image(gpointer data, gpointer user_data) {
	struct manifest_image* image = data;
	GHashTable* t = user_data;
	g_ptr_array_foreach(image->signatures, createsigimagetable_sig, t);
}
static GHashTable* createsigimagetable(const struct manifest_manifest* manifest) {
	GHashTable* t = g_hash_table_new(g_str_hash, g_str_equal);

	g_ptr_array_foreach(manifest->images, createsigimagetable_image, t);
	return t;
}

static struct crypto_keys* repo_keys_load() {
	gchar* pubkeypath = buildpath(keysdir, CRYPTO_KEYNAME_RSA_PUB, NULL);
	gchar* privkeypath = buildpath(keysdir, CRYPTO_KEYNAME_RSA_PRIV, NULL);
	struct crypto_keys* keys = crypto_readkeys(pubkeypath, privkeypath);
	g_free(pubkeypath);
	g_free(privkeypath);
	return keys;
}

static void repo_image_list_printimage(gpointer data, gpointer user_data) {
	struct manifest_image* image = data;
	int* index = user_data;
	g_message("%d: uuid: %s, version: %d, enabled: %s", *index, image->uuid,
			image->version, image->enabled ? "yes" : "no");
	*index += 1;
}

static void repo_image_list() {
	struct manifest_manifest* manifest = manifest_load(manifestpath);
	int index = 0;
	g_message("have %d images", manifest->images->len);
	g_ptr_array_foreach(manifest->images, repo_image_list_printimage, &index);
	manifest_free(manifest);
}

static void repo_updatemanifest(struct manifest_manifest* manifest,
		struct crypto_keys* keys) {
	manifest->serial++;
	manifest->timestamp = g_get_real_time() / 1000000;

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

static gboolean findbyversion(gconstpointer a, gconstpointer b) {
	return ((struct manifest_image*) a)->version == GPOINTER_TO_UINT(b);
}

static void repo_image_add(const gchar* imagepath, const gchar* stamp) {
	struct manifest_manifest* manifest = manifest_load(manifestpath);

	struct stamp_stamp* s = stamp_loadstamp(stamp);
	if (s == NULL) {
		g_message("failed to load stamp");
		return;
	}

	if (g_ptr_array_find_with_equal_func(manifest->images,
			GUINT_TO_POINTER(s->version), findbyversion, NULL)) {
		g_message("version %u already exists", s->version);
		goto err_verexists;
	}

	gchar* imagedata;
	gsize imagesz;
	if (!g_file_get_contents(imagepath, &imagedata, &imagesz, NULL)) {
		g_message("failed to read image data");
		goto err_readimage;
	}

	GHashTable* sigtable = createsigimagetable(manifest);

	struct crypto_keys* keys = repo_keys_load();
	struct manifest_image* image = manifest_image_new();
	for (int i = 0; i < G_N_ELEMENTS(sigtypes); i++) {
		struct manifest_signature* imagesig = crypto_sign(sigtypes[i], keys,
				(guint8*) imagedata, imagesz);
		g_ptr_array_add(image->signatures, imagesig);

		gchar* sigkey = buildsigkey(imagesig);
		if (g_hash_table_contains(sigtable, sigkey)) {
			g_message("image already exists");
			goto err_sigexists;
		}
	}

	image->uuid = s->uuid;
	image->version = s->version;
	image->size = imagesz;
	image->enabled = TRUE;
	g_ptr_array_add(manifest->images, image);

	GError* imagewriteerr = NULL;
	gchar* imageinrepo = buildpath(arg_repodir, image->uuid, NULL);
	if (!g_file_set_contents(imageinrepo, imagedata, imagesz, &imagewriteerr)) {
		g_message("failed to write image data; %s", imagewriteerr->message);
		goto err_writeimage;
	}

	repo_updatemanifest(manifest, keys);

	err_writeimage: //
	g_clear_error(&imagewriteerr);
	g_free(imageinrepo);
	err_sigexists: //
	g_free(imagedata);
	crypto_keys_free(keys);
	err_readimage: //
	err_verexists: //
	stamp_freestamp(s);
	manifest_free(manifest);

	return;
}

static void repo_image_update(guint index) {

}

static void repo_image_delete(guint index) {
	struct manifest_manifest* manifest = manifest_load(manifestpath);
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

#define IMAGEERR_NONE 0
#define IMAGEERR_MISSINGFILE 1

static void repo_verify_verifyimage(gpointer data, gpointer user_data) {
	struct manifest_image* image = data;
	int imageerr = IMAGEERR_NONE;
	g_message("checking image %s...", image->uuid);
	gchar* imagepath = buildpath(arg_repodir, image->uuid, NULL);
	gsize imagesz;
	gchar* imagedata;

	GError* err = NULL;
	if (!g_file_get_contents(imagepath, &imagedata, &imagesz, &err)) {
		g_message("failed to load image data; %s", err->message);
		imageerr = IMAGEERR_MISSINGFILE;
		goto err_loadimage;
	}

	err_loadimage: //
	g_clear_error(&err);
	g_free(imagepath);

	if (user_data != NULL && imageerr != IMAGEERR_NONE) {
		GHashTable* badimages = user_data;
		g_hash_table_insert(badimages, image, GINT_TO_POINTER(imageerr));
	}
}

static void repo_verify() {
	struct manifest_manifest* manifest = manifest_load(manifestpath);
	struct crypto_keys* keys = repo_keys_load();
	g_ptr_array_foreach(manifest->images, repo_verify_verifyimage, NULL);
	crypto_keys_free(keys);
	manifest_free(manifest);
}

static void repo_repair_removebadimage(gpointer key, gpointer value,
		gpointer user_data) {
	GPtrArray* images = user_data;
	g_ptr_array_remove(images, key);
}

static void countversions(gpointer data, gpointer user_data) {
	struct manifest_image* image = data;
	GHashTable* versioncounts = user_data;
	guint count = 0;

	gpointer key = GUINT_TO_POINTER(image->version);
	if (g_hash_table_contains(versioncounts, key)) {
		count = GPOINTER_TO_UINT(g_hash_table_lookup(versioncounts, key));
	}
	count++;
	g_hash_table_replace(versioncounts, key, GUINT_TO_POINTER(count));
}

static void cullversions(gpointer key, gpointer value, gpointer user_data) {
	guint count = GPOINTER_TO_UINT(value);
	if (count == 1)
		return;

	GPtrArray* images = user_data;
	guint index;
	while (g_ptr_array_find_with_equal_func(images, key, findbyversion, &index)) {
		g_ptr_array_remove_index(images, index);
	}
}

static gboolean findbyuuid(gconstpointer a, gconstpointer b) {
	const struct manifest_image* image = a;
	const gchar* uuid = b;
	return strcmp(image->uuid, uuid) == 0;
}

static void repo_repair() {
	struct manifest_manifest* manifest = manifest_load(manifestpath);
	struct crypto_keys* keys = repo_keys_load();

	guint lenbefore = manifest->images->len;

	// find any images in the repo that aren't listed in the manifest
	GDir* dir = g_dir_open(arg_repodir, 0, NULL);
	for (const gchar* filename = g_dir_read_name(dir); filename != NULL;
			filename = g_dir_read_name(dir)) {
		if (strcmp(filename, OTA_MANIFEST) == 0
				|| strcmp(filename, OTA_SIG) == 0)
			continue;
		if (!g_ptr_array_find_with_equal_func(manifest->images, filename,
				findbyuuid, NULL)) {
			g_message("deleting dangling image %s", filename);
			gchar* imagepath = buildpath(arg_repodir, filename, NULL);
			unlink(imagepath);
			g_free(imagepath);
		}
	}

	// count the instances of each version and remove any versions with duplicates
	GHashTable* versioncounts = g_hash_table_new(g_direct_hash, g_direct_equal);
	g_ptr_array_foreach(manifest->images, countversions, versioncounts);
	g_hash_table_foreach(versioncounts, cullversions, manifest->images);

	// find any images that have missing data or incorrect signatures
	GHashTable* badimages = g_hash_table_new(g_direct_hash, g_direct_equal);
	g_ptr_array_foreach(manifest->images, repo_verify_verifyimage, badimages);
	g_hash_table_foreach(badimages, repo_repair_removebadimage,
			manifest->images);

	if (manifest->images->len != lenbefore)
		repo_updatemanifest(manifest, keys);

	manifest_free(manifest);
	crypto_keys_free(keys);
}

int main(int argc, char** argv) {

	int ret = 0;

	gboolean action_list = FALSE;
	gboolean action_add = FALSE;
	gboolean action_update = FALSE;
	gboolean action_delete = FALSE;
	gboolean action_verify = FALSE;
	gboolean action_repair = FALSE;
	gchar* param_imagepath = NULL;
	gint param_imageindex = -1;
	gchar* param_stamp = NULL;
	gchar** param_imagetags = NULL;
	gchar* param_imageenabled = "true";

	GError* error = NULL;
	GOptionEntry entries[] = { ARGS_REPODIR, ARGS_KEYDIR,
//
			ARGS_ACTION_ADD, ARGS_ACTION_LIST,
			ARGS_ACTION_UPDATE, ARGS_ACTION_DELETE, ARGS_ACTION_VERIFY,
			ARGS_ACTION_REPAIR,
			//
			ARGS_PARAMETER_IMAGEPATH, ARGS_PARAMETER_IMAGEINDEX,
			ARGS_PARAMETER_IMAGESTAMP, ARGS_PARAMETER_IMAGETAGS,
			ARGS_PARAMETER_IMAGEENABLED,
			//
			{ NULL } };
	GOptionContext* optioncontext = g_option_context_new(NULL);
	g_option_context_add_main_entries(optioncontext, entries,
	GETTEXT_PACKAGE);
	g_option_context_set_description(optioncontext, "manage ota repo");

	if (!g_option_context_parse(optioncontext, &argc, &argv, &error)) {
		g_print("option parsing failed: %s\n", error->message);
		ret = 1;
		goto err_args;
	}

	if (arg_repodir == NULL) {
		g_message("you must pass a directory for the repo");
		goto err_args;
	}

	if (keysdir == NULL) {
		g_message("you must pass a directory containing the keys");
		goto err_args;
	}

	if (action_list + action_add + action_update + action_delete + action_verify
			+ action_repair != 1) {
		g_message("you must specify one action");
		goto err_args;
	}

	if (g_mkdir_with_parents(arg_repodir, 0700) > 0) {
		g_message("failed to create repo dir");
		goto err_createdir;
	}

	manifestpath = buildpath(arg_repodir, OTA_MANIFEST, NULL);
	sigpath = buildpath(arg_repodir, OTA_SIG, NULL);

	if (action_list)
		repo_image_list();
	else if (action_add) {
		if (param_imagepath == NULL) {
			g_message("you must pass the path of the image to add");
			goto err_args;
		}
		if (param_stamp == NULL) {
			g_message("you must pass the path of the image stamp file");
			goto err_args;
		}
		repo_image_add(param_imagepath, param_stamp);
	} else if (action_update) {
		repo_image_update(param_imageindex);
	} else if (action_delete) {
		if (param_imageindex < 0) {
			g_message("you must pass a valid image index");
			goto err_args;
		}
		repo_image_delete(param_imageindex);
	} else if (action_verify)
		repo_verify();
	else if (action_repair) {
		repo_repair();
	}

	err_createdir: //
	err_args: //

	return ret;
}
