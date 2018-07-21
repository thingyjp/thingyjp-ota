#include "manifest.h"
#include "jsonparserutils.h"
#include "jsonbuilderutils.h"

struct manifest_signature* manifest_signature_new() {
	struct manifest_signature* signature = g_malloc0(sizeof(*signature));
	return signature;
}

void manifest_signature_free(struct manifest_signature* signature) {
	g_free(signature);
}

static void manifest_signature_free_gdestroynotify(gpointer data) {
	manifest_signature_free((struct manifest_signature*) data);
}

struct manifest_image* manifest_image_new() {
	struct manifest_image* manifest_image = g_malloc0(sizeof(*manifest_image));
	manifest_image->signatures = g_ptr_array_new_with_free_func(
			manifest_signature_free_gdestroynotify);
	return manifest_image;
}

static void manifest_image_free(struct manifest_image* manifest_image) {
	if (manifest_image->name != NULL)
		g_free(manifest_image->name);
	g_ptr_array_free(manifest_image->signatures, TRUE);
	g_free(manifest_image);
}

static void manifest_image_free_gdestroynotify(gpointer data) {
	manifest_image_free((struct manifest_image*) data);
}

static void manifest_signature_deserialise(JsonArray *array, guint index,
		JsonNode *element_node, gpointer user_data) {
	GPtrArray* signatures = user_data;

	JsonObject* rootobj = JSON_NODE_GET_OBJECT(element_node);
	if (rootobj == NULL)
		return;

	const gchar* type = JSON_OBJECT_GET_MEMBER_STRING(rootobj,
			OTA_JSONFIELD_SIGNATURE_TYPE);
	const gchar* data = JSON_OBJECT_GET_MEMBER_STRING(rootobj,
			OTA_JSONFIELD_SIGNATURE_DATA);

	if (type == NULL || data == NULL) {
		g_message("incomplete or invalid signature");
		return;
	}

	enum manifest_signaturetype sigtype = OTA_SIGTYPE_INVALID;
	for (int i = 0; i < G_N_ELEMENTS(manifest_signaturetypestrings); i++) {
		const gchar* typestr = manifest_signaturetypestrings[i];
		if (typestr != NULL && strcmp(type, typestr) == 0) {
			sigtype = i;
			break;
		}
	}

	if (sigtype == OTA_SIGTYPE_INVALID) {
		g_message("invalid or unknown signature type: %s", type);
		return;
	}

	struct manifest_signature* signature = manifest_signature_new();
	signature->type = sigtype;
	g_ptr_array_add(signatures, signature);
}

void manifest_signature_serialise(JsonBuilder* builder,
		struct manifest_signature* signature) {
	json_builder_begin_object(builder);
	JSONBUILDER_ADD_STRING(builder, OTA_JSONFIELD_SIGNATURE_TYPE,
			manifest_signaturetypestrings[signature->type]);
	JSONBUILDER_ADD_STRING(builder, OTA_JSONFIELD_SIGNATURE_DATA,
			signature->data);
	json_builder_end_object(builder);
}

static void manifest_signature_serialise_gfunc(gpointer data,
		gpointer user_data) {
	manifest_signature_serialise((JsonBuilder*) user_data,
			(struct manifest_signature*) data);
}

static void manifest_image_serialise(gpointer data, gpointer user_data) {
	struct manifest_image* image = data;
	JsonBuilder* builder = user_data;

	json_builder_begin_object(builder);
	JSONBUILDER_ADD_STRING(builder, OTA_JSONFIELD_IMAGE_NAME, "xx");
	JSONBUILDER_ADD_INT(builder, OTA_JSONFIELD_IMAGE_VERSION, image->version);
	JSONBUILDER_START_ARRAY(builder, OTA_JSONFIELD_SIGNATURES);
	g_ptr_array_foreach(image->signatures, manifest_signature_serialise_gfunc,
			builder);
	json_builder_end_array(builder);

	json_builder_end_object(builder);
}

static void manifest_image_deserialise(JsonArray *array, guint index,
		JsonNode *element_node, gpointer user_data) {
	GPtrArray* manifest_images = user_data;
	struct manifest_image* manifest_image = manifest_image_new();

	gchar* name;
	JsonObject* imageobj = JSON_NODE_GET_OBJECT(element_node);
	if (imageobj != NULL) {
		name = JSON_OBJECT_GET_MEMBER_STRING(imageobj,
				OTA_JSONFIELD_IMAGE_NAME);
		JsonArray* signatures = JSON_OBJECT_GET_MEMBER_ARRAY(imageobj,
				OTA_JSONFIELD_SIGNATURES);
		if (name == NULL || signatures == NULL) {
			g_message("incomplete or invalid image");
			goto err_parse;
		}
		json_array_foreach_element(signatures, manifest_signature_deserialise,
				manifest_image->signatures);
	} else {
		g_message("image element isn't an object");
		goto err_parse;
	}

	if (manifest_image->signatures->len == 0) {
		g_message("image has no usable signatures");
		goto err_parse;
	}

	manifest_image->name = g_strdup(name);

	g_ptr_array_add(manifest_images, manifest_image);
	return;

	err_parse: //
	manifest_image_free(manifest_image);
	return;
}

gboolean manifest_deserialise_into(struct manifest_manifest* manifest,
		const gchar* data, gsize len) {
	gboolean ret = FALSE;
	JsonParser* parser = json_parser_new();

	if (json_parser_load_from_data(parser, data, len, NULL)) {
		JsonNode* root = json_parser_get_root(parser);
		JsonObject* rootobj = JSON_NODE_GET_OBJECT(root);
		if (rootobj != NULL) {
			JsonArray* images = JSON_OBJECT_GET_MEMBER_ARRAY(rootobj,
					OTA_JSONFIELD_IMAGES);
			if (images != NULL) {
				json_array_foreach_element(images, manifest_image_deserialise,
						manifest->images);
			} else {
				g_message("no images field or field isn't an array");
				goto err_parse;
			}
		} else {
			g_message("root node should be an object");
			goto err_parse;
		}
	} else
		g_message("failed to parse manifest");

	ret = TRUE;

	err_parse: //
	g_object_unref(parser);
	return ret;
}

JsonBuilder* manifest_serialise(struct manifest_manifest* manifest) {
	JsonBuilder* builder = json_builder_new();
	json_builder_begin_object(builder);

	JSONBUILDER_ADD_INT(builder, OTA_JSONFIELD_SERIAL, manifest->serial);
	JSONBUILDER_ADD_INT(builder, MANIFEST_JSONFIELD_TIMESTAMP, 0);
	JSONBUILDER_START_ARRAY(builder, OTA_JSONFIELD_IMAGES);
	g_ptr_array_foreach(manifest->images, manifest_image_serialise, builder);
	json_builder_end_array(builder);

	json_builder_end_object(builder);
	return builder;
}

struct manifest_manifest* manifest_deserialise(const gchar* data, gsize len) {
	struct manifest_manifest* manifest = manifest_new();
	if (manifest_deserialise_into(manifest, data, len))
		return manifest;
	else {
		manifest_free(manifest);
		return NULL;
	}
}

struct manifest_manifest* manifest_new() {
	struct manifest_manifest* manifest = g_malloc0(sizeof(*manifest));
	manifest->images = g_ptr_array_new_with_free_func(
			manifest_image_free_gdestroynotify);
	return manifest;
}

void manifest_free(struct manifest_manifest* manifest) {
	g_ptr_array_free(manifest->images, TRUE);
	g_free(manifest);
}
