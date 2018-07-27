#include "utils.h"

gchar* buildpath(const gchar* dir, const gchar* file) {
	GString* pathgstr = g_string_new(dir);
	g_string_append_c(pathgstr, '/');
	g_string_append(pathgstr, file);
	return g_string_free(pathgstr, FALSE);
}

void teenyhttp_hexdump(guint8* payload, gsize len) {
	for (gsize i = 0; i < len; i += 8) {
		GString* hexstr = g_string_new(NULL);
		GString* asciistr = g_string_new(NULL);
		for (int j = 0; j < 8; j++) {
			if (i + j < len) {
				g_string_append_printf(hexstr, "%02x ", (unsigned) *payload);
				g_string_append_printf(asciistr, "%c",
				g_ascii_isgraph(*payload) ? *payload : ' ');
			} else {
				g_string_append_printf(hexstr, "   ");
				g_string_append_printf(asciistr, " ");
			}
			payload++;
		}
		gchar* hs = g_string_free(hexstr, FALSE);
		gchar* as = g_string_free(asciistr, FALSE);
		g_message("%08x %s[%s]", (unsigned ) i, hs, as);
		g_free(hs);
		g_free(as);
	}
}
