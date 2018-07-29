#include <gio/gio.h>
#include "teenyhttp.h"
#include "utils.h"

#define TEENYHTTP_READBUFFSZ (8 * 1024)
#define TEENYHTTP_HTTP_LINETERM "\r\n"

static GRegex* resultlineregexp;
static GRegex* headerregexp;

void teenyhttp_init() {
	resultlineregexp = g_regex_new("(HTTP\/(1.0|1.1)) ([1-5][0-9]{2}) (.*)", 0,
			0, NULL);
	headerregexp = g_regex_new("([a-z,A-Z,-]*): {0,1}(.*)", 0, 0, NULL);
}

static gboolean teenyhttp_munchstatusline(const gchar* resultline,
		struct teenyhttp_response* response) {
	gboolean ret = FALSE;
	GMatchInfo* matchinfo = NULL;
	if (g_regex_match(resultlineregexp, resultline, 0, &matchinfo)) {
		gchar* proto = g_match_info_fetch(matchinfo, 1);
		gchar* codestr = g_match_info_fetch(matchinfo, 3);
		gchar* message = g_match_info_fetch(matchinfo, 4);
		guint64 code = g_ascii_strtoull(codestr, NULL, 10);
		response->code = code;
#ifdef TEENYHTTP_DEBUG
		g_message("%s %u %s", proto, response->code, message);
#endif
		ret = TRUE;
	} else {
		g_message("bad status line");
	}
	g_match_info_free(matchinfo);
	return ret;
}

typedef gboolean (*teenyhttp_headerhandler)(const gchar* header,
		const gchar* value, struct teenyhttp_response* response);
struct teenyhttp_headerhandler_entry {
	const gchar* header;
	const teenyhttp_headerhandler handler;
};

static gboolean teenhttp_headerhandler_contenttype(const gchar* header,
		const gchar* value, struct teenyhttp_response* response) {
	response->contenttype = value;
	return TRUE;
}

static gboolean teenhttp_headerhandler_contentlength(const gchar* header,
		const gchar* value, struct teenyhttp_response* response) {
	guint64 contentlen = g_ascii_strtoull(value, NULL, 10);
	response->contentlen = contentlen;
	return TRUE;
}

static struct teenyhttp_headerhandler_entry headerhandlers[] = { { .header =
		"Content-Type", .handler = teenhttp_headerhandler_contenttype }, {
		.header = "Content-Length", .handler =
				teenhttp_headerhandler_contentlength } };

static void teenyhttp_munchheaders(const gchar* headers,
		struct teenyhttp_response* response) {
	GMatchInfo* matchinfo = NULL;
	g_regex_match(headerregexp, headers, G_REGEX_MATCH_NEWLINE_CRLF,
			&matchinfo);
	for (; g_match_info_matches(matchinfo);
			g_match_info_next(matchinfo, NULL)) {
		gchar* header = g_match_info_fetch(matchinfo, 1);
		gchar* value = g_match_info_fetch(matchinfo, 2);
#ifdef TEENYHTTP_DEBUG
		g_message("http header; header:%s, value:%s", header, value);
#endif
		for (int i = 0; i < G_N_ELEMENTS(headerhandlers); i++)
			if (strcmp(header, headerhandlers[i].header) == 0)
				headerhandlers[i].handler(header, value, response);
	}
	g_match_info_free(matchinfo);
}

static void teenyhttp_munchpayload(guint8* payload, gsize len,
		teenyhttp_datacallback callback, gpointer user_data) {
#ifdef TEENYHTTP_DEBUG
	teenyhttp_hexdump(payload, len);
#endif
	if (callback != NULL)
		callback(payload, len, user_data);
}

gboolean teenyhttp_get(const gchar* host, const gchar* path,
		teenyhttp_responsecallback responsecallback,
		gpointer responsecallback_user_data,
		teenyhttp_datacallback datacallback, gpointer datacallback_user_data) {

	gboolean ret = FALSE;

#ifdef TEENYHTTP_DEBUG
	g_message("GET'ing %s from %s", path, host);
#endif

	GSocketClient* socketclient = g_socket_client_new();
	GSocketConnection* socketconnection = g_socket_client_connect_to_host(
			socketclient, host, 80, NULL, NULL);
	if (socketconnection == NULL) {
		g_message("failed to connect");
		goto err_connect;
	}

	GString* requeststr = g_string_new(NULL);
	g_string_append_printf(requeststr,
			"GET %s HTTP/1.1" TEENYHTTP_HTTP_LINETERM, path);
	g_string_append_printf(requeststr, "Host: %s"TEENYHTTP_HTTP_LINETERM, host);
	g_string_append_printf(requeststr, TEENYHTTP_HTTP_LINETERM);
	gchar* request = g_string_free(requeststr, FALSE);
	gsize requestlen = strlen(request);
	gssize wrote = g_output_stream_write(
			g_io_stream_get_output_stream(G_IO_STREAM(socketconnection)),
			request, requestlen, NULL, NULL);
	if (wrote != requestlen) {
		g_message("failed to write %d bytes", (int ) requestlen);
		goto err_sendreq;
	}

	guint8* readbuff = g_malloc(TEENYHTTP_READBUFFSZ);
	gssize read = g_input_stream_read(
			g_io_stream_get_input_stream(G_IO_STREAM(socketconnection)),
			readbuff,
			TEENYHTTP_READBUFFSZ, NULL, NULL);

	gchar* preambleend = g_strstr_len(readbuff, read,
	TEENYHTTP_HTTP_LINETERM""TEENYHTTP_HTTP_LINETERM);
	if (preambleend == NULL)
		g_message("didn't find end of preamble block");
	preambleend += 2;
	*preambleend = '\0';

	gchar* resultline = readbuff;
	size_t preamblelen = preambleend - resultline;

	gchar* resultlineend = g_strstr_len(readbuff, preamblelen,
	TEENYHTTP_HTTP_LINETERM);
	if (resultlineend == NULL) {
		g_message("didn't find end of result line");
		goto err_read;
	}
	*(resultlineend) = '\0';

	gchar* headers = resultlineend + 2;
	guint8* payload = preambleend + 2;

	struct teenyhttp_response response;
	memset(&response, 0, sizeof(response));

	teenyhttp_munchstatusline(readbuff, &response);
	teenyhttp_munchheaders(headers, &response);

	if (responsecallback != NULL
			&& !responsecallback(&response, responsecallback_user_data))
		goto err_read;

	if (response.contentlen > 0) {
		gssize payloadlen = read - ((guint8*) (preambleend + 2) - readbuff);
		g_assert(payloadlen >= 0);
		response.contentleft = response.contentlen - payloadlen;
		teenyhttp_munchpayload(payload, payloadlen, datacallback,
				datacallback_user_data);
		while (response.contentleft > 0) {
			g_message("%u left", response.contentleft);
			gsize want = MIN(TEENYHTTP_READBUFFSZ, response.contentleft);
			read = g_input_stream_read(
					g_io_stream_get_input_stream(G_IO_STREAM(socketconnection)),
					readbuff, want, NULL, NULL);

			if (read < 0) {
				g_message("failed to read %d", read);
				goto err_read;
			}
			if (read > 0) {
				teenyhttp_munchpayload(readbuff, read, datacallback,
						datacallback_user_data);
				response.contentleft -= read;
			}
		}
	}

	ret = TRUE;

	err_read: //
	g_free(readbuff);
	err_sendreq: //
	g_free(request);
	err_connect: //
	g_object_unref(socketconnection);
	return ret;
}

static gboolean teenyhttp_defaultresponsecallback(
		const struct teenyhttp_response* response, gpointer user_data) {
	const gchar* contenttype = user_data;
	return response->code == 200;
}

gboolean teenyhttp_get_simple(const gchar* host, const gchar* path,
		teenyhttp_datacallback datacallback, gpointer datacallback_user_data) {
	return teenyhttp_get(host, path, teenyhttp_defaultresponsecallback, NULL,
			datacallback, datacallback_user_data);
}
