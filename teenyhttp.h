#pragma once

#include <glib.h>

//#define TEENYHTTP_DEBUG

struct teenyhttp_response {
	unsigned code;
	gsize contentlen;
	const gchar* contenttype;
	// private, don't touch!
	gsize contentleft;
};

typedef gboolean (*teenyhttp_responsecallback)(
		const struct teenyhttp_response* response, gpointer user_data);
typedef gboolean (*teenyhttp_datacallback)(guint8* data, gsize len,
		gpointer user_data);

void teenyhttp_init(void);
gboolean teenyhttp_get(const gchar* host, const gchar* path,
		teenyhttp_responsecallback responsecallback,
		gpointer responsecallback_user_data,
		teenyhttp_datacallback datacallback, gpointer datacallback_user_data);
gboolean teenyhttp_get_simple(const gchar* host, const gchar* path,
		teenyhttp_datacallback datacallback, gpointer datacallback_user_data);
gboolean teenyhttp_datacallback_bytebuffer(guint8* data, gsize len,
		gpointer user_data);
