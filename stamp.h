#pragma once

#include "jsonparserutils.h"

#define STAMPFILE				 "stamp.json"
#define STAMP_JSONFIELD_UUID	 "uuid"
#define STAMP_JSONFIELD_REPOUUID "repouuid"
#define STAMP_JSONFIELD_VERSION  "version"

struct stamp_stamp {
	gchar* uuid;
	gchar* repouuid;
	guint version;
};

static struct stamp_stamp* stamp_loadstamp(const gchar*) __attribute__((unused));
static struct stamp_stamp* stamp_loadstamp(const gchar* stamp) {
	struct stamp_stamp* s = NULL;
	JsonParser* stampparser = json_parser_new();
	if (!json_parser_load_from_file(stampparser, stamp, NULL)) {
		g_message("failed to load stamp");
		goto err_load;
	}

	JsonObject* stamproot = JSON_NODE_GET_OBJECT(
			json_parser_get_root(stampparser));
	if (stamproot == NULL) {
		goto err_parse;
	}

	const gchar* uuid = JSON_OBJECT_GET_MEMBER_STRING(stamproot,
			STAMP_JSONFIELD_UUID);
	const gchar* repouuid = JSON_OBJECT_GET_MEMBER_STRING(stamproot,
			STAMP_JSONFIELD_REPOUUID);
	int version = JSON_OBJECT_GET_MEMBER_INT(stamproot,
			STAMP_JSONFIELD_VERSION);

	if (uuid == NULL) {
		g_message("failed to get uuid from stamp");
		goto err_parse;
	}

	if (repouuid == NULL) {
		g_message("failed to get repouuid from stamp");
		goto err_parse;
	}

	if (version == -1) {
		g_message("failed to get version from stamp");
		goto err_parse;
	}

	s = g_malloc0(sizeof(*s));
	s->uuid = g_strdup(uuid);
	s->repouuid = g_strdup(repouuid);
	s->version = version;
	err_load: //
	err_parse: //
	g_object_unref(stampparser);
	return s;
}

static void stamp_freestamp(struct stamp_stamp*) __attribute__((unused));
static void stamp_freestamp(struct stamp_stamp* stamp) {
	g_free(stamp->uuid);
	g_free(stamp->repouuid);
	g_free(stamp);
}
