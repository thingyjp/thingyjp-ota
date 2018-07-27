#pragma once

#include <glib.h>

gchar* buildpath(const gchar* dir, const gchar* file);
void teenyhttp_hexdump(guint8* payload, gsize len);
