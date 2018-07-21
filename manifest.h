#pragma once

#include <json-glib/json-glib.h>

enum manifest_signaturetype {
	OTA_SIGTYPE_INVALID, OTA_SIGTYPE_RSASHA256, OTA_SIGTYPE_RSASHA512
};

struct manifest_image {
	const gchar* name;
	unsigned version;
	GPtrArray* signatures;
};

struct manifest_signature {
	enum manifest_signaturetype type;
	const gchar* data;
};

struct manifest_manifest {
	unsigned serial;
	GPtrArray* images;
};

#define OTA_MANIFEST                 "manifest.json"
#define OTA_SIG                      "sig.json"

#define OTA_JSONFIELD_SERIAL         "serial"
#define MANIFEST_JSONFIELD_TIMESTAMP "timestamp"
#define OTA_JSONFIELD_IMAGES         "images"
#define OTA_JSONFIELD_IMAGE_NAME	 "name"
#define OTA_JSONFIELD_IMAGE_VERSION  "version"
#define OTA_JSONFIELD_SIGNATURES     "signatures"
#define OTA_JSONFIELD_SIGNATURE_DATA "data"
#define OTA_JSONFIELD_SIGNATURE_TYPE "type"

#define OTA_SIGNATURE_TYPE_RSASHA256 "rsa-sha256"
#define OTA_SIGNATURE_TYPE_RSASHA512 "rsa-sha512"

static const gchar* manifest_signaturetypestrings[] __attribute__((unused)) = {
		[OTA_SIGTYPE_RSASHA256 ] = OTA_SIGNATURE_TYPE_RSASHA256,
		[OTA_SIGTYPE_RSASHA512 ] = OTA_SIGNATURE_TYPE_RSASHA512 };

void manifest_signature_serialise(JsonBuilder* builder,
		struct manifest_signature* signature);
gboolean manifest_deserialise_into(struct manifest_manifest* manifest,
		const gchar* data, gsize len);
JsonBuilder* manifest_serialise(struct manifest_manifest* manifest);
struct manifest_manifest* manifest_deserialise(const gchar* data, gsize len);
struct manifest_image* manifest_image_new(void);
struct manifest_manifest* manifest_new(void);
void manifest_free(struct manifest_manifest* manifest);
