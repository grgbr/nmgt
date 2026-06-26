#ifndef _RESCONF_H_
#define _RESCONF_H_
#include <sysrepo.h>

#define NGHTTP2_NO_SSIZE_T
#include <nghttp2/nghttp2.h>

#define OUTPUT_WOULDBLOCK_THRESHOLD (1 << 16)

#define ARRLEN(x) (sizeof(x) / sizeof(x[0]))

#define MAKE_NV(NAME, VALUE) \
	{ \
		(uint8_t *)NAME,   (uint8_t *)VALUE,     sizeof(NAME) - 1, \
		sizeof(VALUE) - 1, NGHTTP2_NV_FLAG_NONE, \
	}

#define MAKE_NV_STR(NAME, VALUE) \
	{ \
		(uint8_t *)NAME,   (uint8_t *)VALUE,     sizeof(NAME) - 1, \
		strlen(VALUE), NGHTTP2_NV_FLAG_NONE, \
	}

struct app_context {
	struct event_base *evbase;
	sr_conn_ctx_t *connection;
};

enum method {
	METHOD_UNKNOW,
	METHOD_OPTIONS,
	METHOD_HEAD,
	METHOD_GET,
	METHOD_POST,
	METHOD_PUT,
	METHOD_PATCH,
	METHOD_DELETE,
};

enum resource {
	RES_ROOT,
	RES_DATA,
	RES_OP,
	RES_YANG,
};

struct http2_stream_data {
	struct http2_stream_data *prev, *next;
	sr_session_ctx_t         *session;
	char                     *request_path;
	char                     *xpath;
	int32_t                   stream_id;
	enum method               method;
	enum resource             resource;
	LYD_FORMAT                out_format;
	char                     *output;
	char                     *input;
	size_t                    input_length;
	sr_datastore_t            ds;
};

struct http2_session_data {
	struct http2_stream_data root;
	struct bufferevent *bev;
	struct app_context *app_ctx;
	nghttp2_session *session;
	char *client_addr;
};

int
restconf_request(nghttp2_session *session,
                struct http2_session_data *session_data,
                struct http2_stream_data *stream_data);

int
error_reply(nghttp2_session *session, struct http2_stream_data *stream_data);

int
str_reply(nghttp2_session *session, struct http2_stream_data *stream_data, const char *data);

int
status_reply(nghttp2_session *session, struct http2_stream_data *stream_data, const char *status);

int
options_reply(nghttp2_session *session, struct http2_stream_data *stream_data, const char *options);

#endif /* _RESCONF_H_ */