#pragma once

#include <glib.h>

gchar* buildpath(const gchar* dir, ...);
void teenyhttp_hexdump(guint8* payload, gsize len);
