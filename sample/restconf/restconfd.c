#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <signal.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <ctype.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <err.h>
#include <string.h>
#include <errno.h>

#include <event.h>
#include <event2/event.h>
#include <event2/bufferevent.h>
#include <event2/listener.h>

#include "restconf.h"

static void
add_stream(struct http2_session_data *session_data,
           struct http2_stream_data *stream_data)
{
	stream_data->next = session_data->root.next;
	session_data->root.next = stream_data;
	stream_data->prev = &session_data->root;
	if (stream_data->next)
		stream_data->next->prev = stream_data;
}

static void
remove_stream(struct http2_session_data *session_data,
              struct http2_stream_data *stream_data)
{
	(void)session_data;

	stream_data->prev->next = stream_data->next;
	if (stream_data->next)
		stream_data->next->prev = stream_data->prev;

}

static struct http2_stream_data *
create_http2_stream_data(struct http2_session_data *session_data, int32_t stream_id)
{
	struct http2_stream_data *stream_data;

	stream_data = malloc(sizeof(struct http2_stream_data));
	stream_data->stream_id = stream_id;
	stream_data->session = NULL;
	stream_data->request_path = NULL;
	stream_data->xpath = NULL;
	stream_data->input = NULL;
	stream_data->input_length = 0;
	stream_data->output = NULL;
	stream_data->method = METHOD_UNKNOW;
	stream_data->out_format = LYD_JSON;
	stream_data->ds = SR_DS_RUNNING;

	add_stream(session_data, stream_data);
	return stream_data;
}

static void
delete_http2_stream_data(struct http2_stream_data *stream_data)
{
	if (stream_data->session)
		sr_session_stop(stream_data->session);

	free(stream_data->input);
	free(stream_data->request_path);
	free(stream_data->output);
	free(stream_data);
}

static struct http2_session_data *
create_http2_session_data(struct app_context *app_ctx,
                          int fd,
                          struct sockaddr *addr,
                          int addrlen)
{
	int rv;
	struct http2_session_data *session_data;
	char host[NI_MAXHOST];
	int val = 1;

	session_data = malloc(sizeof(struct http2_session_data));
	memset(session_data, 0, sizeof(struct http2_session_data));
	session_data->app_ctx = app_ctx;
	setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (char *)&val, sizeof(val));
	session_data->bev = bufferevent_socket_new(app_ctx->evbase, fd, BEV_OPT_CLOSE_ON_FREE | BEV_OPT_DEFER_CALLBACKS);
	bufferevent_enable(session_data->bev, EV_READ | EV_WRITE);
	rv = getnameinfo(addr, (socklen_t)addrlen, host, sizeof(host), NULL, 0, NI_NUMERICHOST);
	if (rv != 0)
		session_data->client_addr = strdup("(unknown)");
	else
		session_data->client_addr = strdup(host);

	return session_data;
}

static void
delete_http2_session_data(struct http2_session_data *session_data)
{
	struct http2_stream_data *stream_data;

	bufferevent_free(session_data->bev);
	nghttp2_session_del(session_data->session);
	for (stream_data = session_data->root.next; stream_data;) {
		struct http2_stream_data *next = stream_data->next;
		delete_http2_stream_data(stream_data);
		stream_data = next;
	}

	free(session_data->client_addr);
	free(session_data);
}

/* Serialize the frame and send (or buffer) the data to
   bufferevent. */
static int
session_send(struct http2_session_data *session_data)
{
	int rv;

	rv = nghttp2_session_send(session_data->session);
	if (rv != 0) {
		warnx("Fatal error: %s", nghttp2_strerror(rv));
		return -1;
	}

	return 0;
}

/* Read the data in the bufferevent and feed them into nghttp2 library
   function. Invocation of nghttp2_session_mem_recv2() may make
   additional pending frames, so call session_send() at the end of the
   function. */
static int
session_recv(struct http2_session_data *session_data)
{
	nghttp2_ssize readlen;
	struct evbuffer *input = bufferevent_get_input(session_data->bev);
	size_t datalen = evbuffer_get_length(input);
	unsigned char *data = evbuffer_pullup(input, -1);

	readlen = nghttp2_session_mem_recv2(session_data->session, data, datalen);
	if (readlen < 0) {
		warnx("Fatal error: %s", nghttp2_strerror((int)readlen));
		return -1;
	}

	if (evbuffer_drain(input, (size_t)readlen) != 0) {
		warnx("Fatal error: evbuffer_drain failed");
		return -1;
	}

	if (session_send(session_data) != 0)
		return -1;

	return 0;
}

static nghttp2_ssize
send_callback(nghttp2_session *session,
             const uint8_t *data, size_t length,
             int flags, void *user_data)
{
	struct http2_session_data *session_data = (struct http2_session_data *)user_data;
	struct bufferevent *bev = session_data->bev;
	(void)session;
	(void)flags;

	/* Avoid excessive buffering in server side. */
	if (evbuffer_get_length(bufferevent_get_output(session_data->bev)) >= OUTPUT_WOULDBLOCK_THRESHOLD)
		return NGHTTP2_ERR_WOULDBLOCK;

	bufferevent_write(bev, data, length);
	return (nghttp2_ssize)length;
}

/* Returns nonzero if the string |s| ends with the substring |sub| */
static int
ends_with(const char *s, const char *sub)
{
	size_t slen = strlen(s);
	size_t sublen = strlen(sub);

	if (slen < sublen)
		return 0;

	return memcmp(s + slen - sublen, sub, sublen) == 0;
}

/* Returns int value of hex string character |c| */
static uint8_t
hex_to_uint(uint8_t c)
{
	if ('0' <= c && c <= '9')
		return (uint8_t)(c - '0');

	if ('A' <= c && c <= 'F')
		return (uint8_t)(c - 'A' + 10);

	if ('a' <= c && c <= 'f')
		return (uint8_t)(c - 'a' + 10);

	return 0;
}

/* Decodes percent-encoded byte string |value| with length |valuelen|
   and returns the decoded byte string in allocated buffer. The return
   value is NULL terminated. The caller must free the returned
   string. */
static char *
percent_decode(const uint8_t *value, size_t valuelen)
{
	char *res;

	res = malloc(valuelen + 1);
	if (valuelen > 3) {
		size_t i, j;

		for (i = 0, j = 0; i < valuelen - 2;) {
			if (value[i] != '%' || !isxdigit(value[i + 1]) || !isxdigit(value[i + 2])) {
				res[j++] = (char)value[i++];
				continue;
			}
			res[j++] = (char)((hex_to_uint(value[i + 1]) << 4) + hex_to_uint(value[i + 2]));
			i += 3;
		}
		memcpy(&res[j], &value[i], 2);
		res[j + 2] = '\0';
	} else {
		memcpy(res, value, valuelen);
		res[valuelen] = '\0';
	}

	return res;
}

static nghttp2_ssize
strsend(nghttp2_session *session,
        int32_t stream_id, uint8_t *buf,
        size_t length, uint32_t *data_flags,
        nghttp2_data_source *source,
        void *user_data)
{
	char *data = source->ptr;
	ssize_t r;
	(void)session;
	(void)stream_id;
	(void)user_data;

	r = strlen(data);
	if (!r) {
		fprintf(stderr, "EOF\n");
		*data_flags |= NGHTTP2_DATA_FLAG_EOF;
		return 0;
	}

	if (r > length)
		r = length - 1;
	else
		*data_flags |= NGHTTP2_DATA_FLAG_EOF;

	memcpy(buf, data, r);
	source->ptr = &data[length];
	return (nghttp2_ssize)r;
}

#define ERROR_HTML "<html><head><title>404</title></head>" \
                   "<body><h1>404 Not Found</h1></body></html>"

int
error_reply(nghttp2_session *session, struct http2_stream_data *stream_data)
{
	int ret;
	ssize_t writelen;
	nghttp2_nv hdrs[] = {
		MAKE_NV(":status", "404")
	};
	nghttp2_data_provider2 data_prd = {
		.read_callback = strsend,
		.source.ptr = ERROR_HTML,
	};

	ret = nghttp2_submit_response2(session,
				       stream_data->stream_id,
				       hdrs,
				       ARRLEN(hdrs),
				       &data_prd);
	return ret ? NGHTTP2_ERR_CALLBACK_FAILURE : 0;
}

int
str_reply(nghttp2_session *session, struct http2_stream_data *stream_data, const char *data)
{
	int ret;
	ssize_t writelen;
	const char *app = stream_data->out_format == LYD_JSON ?
			  "application/yang-data+json" :
			  "application/yang-data+xml";
	char length[64];

	snprintf(length, sizeof(length), "%ld", strlen(data));
	nghttp2_nv hdrs[] = {
		MAKE_NV(":status", "200"),
		MAKE_NV("cache-control", "no-cache"),
		MAKE_NV("access-control-allow-origin", "*"),
		MAKE_NV_STR("content-type", app),
		MAKE_NV_STR("content-length", length)
	};
	nghttp2_data_provider2 data_prd = {
		.read_callback = strsend,
		.source.ptr = (char *)data,
	};

	ret = nghttp2_submit_response2(session,
				stream_data->stream_id,
				hdrs,
				ARRLEN(hdrs),
				stream_data->method == METHOD_HEAD ? NULL : &data_prd);
	return ret ? NGHTTP2_ERR_CALLBACK_FAILURE : 0;
}

int
status_reply(nghttp2_session *session, struct http2_stream_data *stream_data, const char *status)
{
	int ret;
	nghttp2_nv hdrs[] = {
		MAKE_NV_STR(":status", status),
		MAKE_NV("access-control-allow-origin", "*"),
	};

	ret = nghttp2_submit_response2(session,
				stream_data->stream_id,
				hdrs,
				ARRLEN(hdrs),
				NULL);
	return ret ? NGHTTP2_ERR_CALLBACK_FAILURE : 0;
}

int
options_reply(nghttp2_session *session, struct http2_stream_data *stream_data, const char *options)
{
	int ret;
	ssize_t writelen;
	const char *app = stream_data->out_format == LYD_JSON ?
			  "application/yang-data+json" :
			  "application/yang-data+xml";
	char length[64];

	nghttp2_nv hdrs[] = {
		MAKE_NV(":status", "204"),
		MAKE_NV("access-control-allow-origin", "*"),
		MAKE_NV_STR("allow", options),
	};

	ret = nghttp2_submit_response2(session,
				stream_data->stream_id,
				hdrs,
				ARRLEN(hdrs),
				NULL);
	return ret ? NGHTTP2_ERR_CALLBACK_FAILURE : 0;
}


/* nghttp2_on_header_callback: Called when nghttp2 library emits
   single header name/value pair. */
static int
on_header_callback(nghttp2_session *session,
                   const nghttp2_frame *frame, const uint8_t *name,
                   size_t namelen, const uint8_t *value,
                   size_t valuelen, uint8_t flags, void *user_data)
{
	struct http2_stream_data *stream_data;
	static const char *PATH = ":path";
	static const char *METHOD = ":method";
	static const char *ACCEPT = "accept";
	(void)flags;
	(void)user_data;

	switch (frame->hd.type) {
	case NGHTTP2_HEADERS:
		if (frame->headers.cat != NGHTTP2_HCAT_REQUEST)
			break;

		stream_data = nghttp2_session_get_stream_user_data(session, frame->hd.stream_id);
		if (!stream_data)
			break;

		if ((strcmp(PATH, name) == 0) && !stream_data->request_path) {
			size_t j;

			for (j = 0; j < valuelen && value[j] != '?'; ++j)
				;

			stream_data->request_path = percent_decode(value, j);
		} else if (strcmp(METHOD, name) == 0) {
			if (strcmp(value, "OPTIONS") == 0)
				stream_data->method = METHOD_OPTIONS;
			else if (strcmp(value, "HEAD") == 0)
				stream_data->method = METHOD_HEAD;
			else if (strcmp(value, "GET") == 0)
				stream_data->method = METHOD_GET;
			else if (strcmp(value, "POST") == 0)
				stream_data->method = METHOD_POST;
			else if (strcmp(value, "PUT") == 0)
				stream_data->method = METHOD_PUT;
			else if (strcmp(value, "PATCH") == 0)
				stream_data->method = METHOD_PATCH;
			else if (strcmp(value, "DELETE") == 0)
				stream_data->method = METHOD_DELETE;
			else
				stream_data->method = METHOD_UNKNOW;
		} else if (strcmp(ACCEPT, name) == 0) {
			if (strcmp(value, "application/yang-data+xml") == 0)
				stream_data->out_format = LYD_XML;
		}
		break;
	}
	return 0;
}

static int
on_begin_headers_callback(nghttp2_session *session,
                          const nghttp2_frame *frame,
                          void *user_data)
{
	struct http2_session_data *session_data = (struct http2_session_data *)user_data;
	struct http2_stream_data *stream_data;

	if (frame->hd.type != NGHTTP2_HEADERS ||
	    frame->headers.cat != NGHTTP2_HCAT_REQUEST)
		return 0;

	stream_data = create_http2_stream_data(session_data, frame->hd.stream_id);
	nghttp2_session_set_stream_user_data(session, frame->hd.stream_id, stream_data);
	return 0;
}

/* Minimum check for directory traversal. Returns nonzero if it is safe. */
static int
check_path(const char *path)
{
	/* We don't like '\' in url. */
	return path[0] && path[0] == '/' && strchr(path, '\\') == NULL &&
	       strstr(path, "/../") == NULL && strstr(path, "/./") == NULL &&
	       !ends_with(path, "/..") && !ends_with(path, "/.");
}

#define WELL_KNOWN "<XRD xmlns='http://docs.oasis-open.org/ns/xri/xrd-1.0'>\n" \
                   "\t<Link rel='restconf' href='/restconf'/>\n" \
                   "</XRD>\n"

static int
on_request_recv(nghttp2_session *session,
                struct http2_session_data *session_data,
                struct http2_stream_data *stream_data)
{
	nghttp2_nv hdrs[] = {MAKE_NV(":status", "200")};
	char *rel_path;
	int ret;

	if (!stream_data->request_path) {
		if (error_reply(session, stream_data) != 0)
			return NGHTTP2_ERR_CALLBACK_FAILURE;

		return 0;
	}

	if (!check_path(stream_data->request_path)) {
		if (error_reply(session, stream_data) != 0)
			return NGHTTP2_ERR_CALLBACK_FAILURE;

		return 0;
	}

	if ((stream_data->method == METHOD_GET) &&
	    (strcmp(stream_data->request_path, "/.well-known/host-meta") == 0)) {
		/* case RFC8040 1.5 RESTCONF Extensibility */
		static const nghttp2_nv xml_hdrs[] = {
			MAKE_NV(":status", "200"),
			MAKE_NV("content-type", "application/xrd+xml")
		};
		nghttp2_data_provider2 data_prd = {
			.read_callback = strsend,
			.source.ptr = WELL_KNOWN,
		};

		ret = nghttp2_submit_response2(session,
					       stream_data->stream_id,
					       xml_hdrs,
					       ARRLEN(xml_hdrs),
					       &data_prd);
		return ret ? NGHTTP2_ERR_CALLBACK_FAILURE : 0;

	}

	if (strncmp(stream_data->request_path, "/restconf", 9) == 0)
		return restconf_request(session, session_data, stream_data);

	if (error_reply(session, stream_data) != 0)
		return NGHTTP2_ERR_CALLBACK_FAILURE;

	return 0;
}

static int
on_frame_recv_callback(nghttp2_session *session,
                       const nghttp2_frame *frame,
		       void *user_data)
{
	struct http2_session_data *session_data = (struct http2_session_data *)user_data;
	struct http2_stream_data *stream_data;

	switch (frame->hd.type) {
	case NGHTTP2_DATA:
	case NGHTTP2_HEADERS:
		/* Check that the client request has finished */
		if (frame->hd.flags & NGHTTP2_FLAG_END_STREAM) {
			stream_data = nghttp2_session_get_stream_user_data(session, frame->hd.stream_id);
			/* For DATA and HEADERS frame, this callback may be called after
			 * on_stream_close_callback. Check that stream still alive. */
			if (!stream_data)
				return 0;

			return on_request_recv(session, session_data, stream_data);
		}

		break;
	default:
		break;
	}

	return 0;
}

static int
on_stream_close_callback(nghttp2_session *session, int32_t stream_id,
                         uint32_t error_code, void *user_data)
{
	struct http2_session_data *session_data = (struct http2_session_data *)user_data;
	struct http2_stream_data *stream_data;
	(void)error_code;

	stream_data = nghttp2_session_get_stream_user_data(session, stream_id);
	if (!stream_data)
		return 0;

	remove_stream(session_data, stream_data);
	delete_http2_stream_data(stream_data);
	return 0;
}

static int
on_data_chunk_recv_callback(nghttp2_session *session, uint8_t flags,
	int32_t stream_id, const uint8_t *data, size_t len, void *user_data)
{
	struct http2_session_data *session_data = (struct http2_session_data *)user_data;
	struct http2_stream_data *stream_data;

	stream_data = nghttp2_session_get_stream_user_data(session, stream_id);
	if (!stream_data)
		return 0;

	stream_data->input = realloc(stream_data->input, stream_data->input_length + len);
	if (!stream_data->input) {
		delete_http2_stream_data(stream_data);
		return 0;
	}

	memcpy(stream_data->input + stream_data->input_length, data, len);
	stream_data->input_length += len;
	return 0;
}

static void
initialize_nghttp2_session(struct http2_session_data *session_data)
{
	nghttp2_session_callbacks *callbacks;

	nghttp2_session_callbacks_new(&callbacks);
	nghttp2_session_callbacks_set_send_callback2(callbacks, send_callback);
	nghttp2_session_callbacks_set_on_frame_recv_callback(callbacks, on_frame_recv_callback);
	nghttp2_session_callbacks_set_on_stream_close_callback( callbacks, on_stream_close_callback);
	nghttp2_session_callbacks_set_on_header_callback(callbacks, on_header_callback);
	nghttp2_session_callbacks_set_on_begin_headers_callback(callbacks, on_begin_headers_callback);
	nghttp2_session_callbacks_set_on_data_chunk_recv_callback(callbacks, on_data_chunk_recv_callback);
	nghttp2_session_server_new(&session_data->session, callbacks, session_data);
	nghttp2_session_callbacks_del(callbacks);
}

/* Send HTTP/2 client connection header, which includes 24 bytes
   magic octets and SETTINGS frame */
static int
send_server_connection_header(struct http2_session_data *session_data)
{
	nghttp2_settings_entry iv[1] = {
		{NGHTTP2_SETTINGS_MAX_CONCURRENT_STREAMS, 100}
	};
	int rv;

	rv = nghttp2_submit_settings(session_data->session, NGHTTP2_FLAG_NONE, iv, ARRLEN(iv));
	if (rv != 0) {
		warnx("Fatal error: %s", nghttp2_strerror(rv));
		return -1;
	}

	return 0;
}

/* readcb for bufferevent after client connection header was
   checked. */
static void
readcb(struct bufferevent *bev, void *ptr)
{
	struct http2_session_data *session_data = (struct http2_session_data *)ptr;
	(void)bev;

	if (session_recv(session_data) != 0)
		delete_http2_session_data(session_data);
}

/* writecb for bufferevent. To greaceful shutdown after sending or
   receiving GOAWAY, we check the some conditions on the nghttp2
   library and output buffer of bufferevent. If it indicates we have
   no business to this session, tear down the connection. If the
   connection is not going to shutdown, we call session_send() to
   process pending data in the output buffer. This is necessary
   because we have a threshold on the buffer size to avoid too much
   buffering. See send_callback(). */
static void
writecb(struct bufferevent *bev, void *ptr)
{
	struct http2_session_data *session_data = (struct http2_session_data *)ptr;
	if (evbuffer_get_length(bufferevent_get_output(bev)) > 0) {
		return;
	}

	if (nghttp2_session_want_read(session_data->session) == 0 &&
	    nghttp2_session_want_write(session_data->session) == 0) {
		delete_http2_session_data(session_data);
		return;
	}

	if (session_send(session_data) != 0) {
		delete_http2_session_data(session_data);
		return;
	}
}

/* eventcb for bufferevent */
static void
eventcb(struct bufferevent *bev, short events, void *ptr)
{
	struct http2_session_data *session_data = (struct http2_session_data *)ptr;

	if (events & BEV_EVENT_EOF) {
		fprintf(stderr, "%s EOF\n", session_data->client_addr);
	} else if (events & BEV_EVENT_ERROR) {
		fprintf(stderr, "%s network error\n", session_data->client_addr);
	} else if (events & BEV_EVENT_TIMEOUT) {
		fprintf(stderr, "%s timeout\n", session_data->client_addr);
	}
	delete_http2_session_data(session_data);
}

/* callback for evconnlistener */
static void
acceptcb(struct evconnlistener *listener,
         int fd,
         struct sockaddr *addr,
         int addrlen,
         void *arg)
{
	struct app_context *app_ctx = (struct app_context *)arg;
	struct http2_session_data *session_data;
	(void)listener;

	session_data = create_http2_session_data(app_ctx, fd, addr, addrlen);
	fprintf(stderr, "accept %s\n", session_data->client_addr);
	initialize_nghttp2_session(session_data);
	if (send_server_connection_header(session_data) != 0 ||
		session_send(session_data) != 0) {
		delete_http2_session_data(session_data);
		return;
	}

	bufferevent_setcb(session_data->bev, readcb, writecb, eventcb, session_data);
}

static void
start_listen(struct event_base *evbase,
             const char *service,
             struct app_context *app_ctx)
{
	int rv;
	struct addrinfo hints;
	struct addrinfo *res, *rp;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	rv = getaddrinfo(NULL, service, &hints, &res);
	if (rv != 0)
		errx(1, "Could not resolve server address");

	for (rp = res; rp; rp = rp->ai_next) {
		struct evconnlistener *listener;

		listener = evconnlistener_new_bind(evbase, acceptcb, app_ctx, LEV_OPT_CLOSE_ON_FREE | LEV_OPT_REUSEABLE, 16, rp->ai_addr, (int)rp->ai_addrlen);
		if (listener) {
			freeaddrinfo(res);
			return;
		}
	}
	errx(1, "Could not start listener");
}

static void
initialize_app_context(struct app_context *app_ctx, struct event_base *evbase)
{
	memset(app_ctx, 0, sizeof(struct app_context));
	app_ctx->evbase = evbase;
	sr_connect(0, &app_ctx->connection);
}

static void
run(const char *service)
{
	struct app_context app_ctx;
	struct event_base *evbase;

	evbase = event_base_new();
	initialize_app_context(&app_ctx, evbase);
	start_listen(evbase, service, &app_ctx);

	event_base_loop(evbase, 0);

	event_base_free(evbase);
}

int
main(int argc, char **argv)
{
	struct sigaction act;

	if (argc < 2) {
		fprintf(stderr, "Usage: libevent-server PORT\n");
		exit(EXIT_FAILURE);
	}

	memset(&act, 0, sizeof(struct sigaction));
	act.sa_handler = SIG_IGN;
	sigaction(SIGPIPE, &act, NULL);

	run(argv[1]);
	return 0;
}
