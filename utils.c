#include "utils.h"

gchar* buildpath(const gchar* dir, ...) {
	GString* pathgstr = g_string_new(dir);

	va_list parts;
	va_start(parts, dir);

	for (gchar* p = va_arg(parts, gchar*); p != NULL;
			p = va_arg(parts, gchar*)) {
		g_string_append_c(pathgstr, '/');
		g_string_append(pathgstr, p);
	}
	va_end(parts);

	return g_string_free(pathgstr, FALSE);
}
