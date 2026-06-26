#include "restconf.h"

static int
data_get(nghttp2_session *session, struct http2_stream_data *stream_data)
{
	sr_data_t *data = NULL;
	int ret;

	if (sr_get_data(stream_data->session, stream_data->xpath, 0, 0, 0, &data))
		return NGHTTP2_ERR_CALLBACK_FAILURE;

	lyd_print_mem(&stream_data->output, data ? data->tree : NULL, stream_data->out_format, LYD_PRINT_SIBLINGS | LYD_PRINT_SHRINK);
	if (!stream_data->output) {
		sr_release_data(data);
		return NGHTTP2_ERR_CALLBACK_FAILURE;
	}

	ret = str_reply(session, stream_data, stream_data->output);
	sr_release_data(data);
	return ret;
}

static int
data_put(nghttp2_session *session, struct http2_stream_data *stream_data)
{
// 	const struct ly_ctx *ly_ctx;
// 	struct lyd_node *input;
// 	struct ly_in *in;
// 	struct lyd_node *parent = NULL;
// 	int parse_flags = LYD_PARSE_NO_STATE | LYD_PARSE_ONLY | LYD_PARSE_STORE_ONLY;
// 	int ret;

// 	ly_ctx = sr_acquire_context(sr_session_get_connection(stream_data->session));

// 	ret = lyd_parse_data(ly_ctx, NULL, in, stream_data->out_format, parse_flags, 0, data);

// release:
// 	sr_release_context(sr_session_get_connection(stream_data->session));
// 	return ret;
	return 0;
}

static void
error_sr_print(sr_session_ctx_t *sess)
{
	uint32_t i;
	const sr_error_info_t *err_info = NULL;

	if (!sr_session_get_error(sess, &err_info) && err_info) {
		for (i = 0; i < err_info->err_count; i++) {
			fprintf(stderr, "%d: %s\n", err_info->err[i].err_code, err_info->err[i].message);
		}
	}
}

static int
op_post(nghttp2_session *session, struct http2_stream_data *stream_data)
{
	const struct ly_ctx *ly_ctx;
	struct lyd_node *input;
	sr_data_t *output = NULL;
	struct ly_in *in = NULL;
	struct lyd_node *parent = NULL;
	int ret;

	ly_ctx = sr_acquire_context(sr_session_get_connection(stream_data->session));
	ret = lyd_new_path(NULL, ly_ctx, stream_data->xpath, NULL, 0, &parent);
	if (ret) {
		ret = status_reply(session, stream_data, "400");
		goto release;
	}

	if (stream_data->input) {
		ly_in_new_memory(stream_data->input, &in);
		ret =  lyd_parse_op(ly_ctx, parent, in, stream_data->out_format, LYD_TYPE_RPC_RESTCONF, LYD_PARSE_STRICT, &input, NULL);
		ly_in_free(in, 0);
		if (ret) {
			ret = status_reply(session, stream_data, "400");
			goto release;
		}
		lyd_free_all(input);
	}

	ret = sr_rpc_send_tree(stream_data->session, parent, 0, &output);
	lyd_free_all(parent);

	if (output) {
		lyd_print_mem(&stream_data->output, output->tree, stream_data->out_format, LYD_PRINT_SIBLINGS | LYD_PRINT_SHRINK);
		ret = str_reply(session, stream_data, stream_data->output);
		sr_release_data(output);
	} else if (ret) {
		error_sr_print(stream_data->session);
		ret = status_reply(session, stream_data, "400");
	} else
		ret = status_reply(session, stream_data, "200");

release:
	sr_release_context(sr_session_get_connection(stream_data->session));
	return ret;
}

static int
resconf_parse_datastore(struct http2_stream_data *stream_data)
{
	if (strncmp(stream_data->xpath, "/ietf-datastores:startup", 24) == 0) {
		stream_data->xpath += 24;
		stream_data->ds = SR_DS_STARTUP;
	} else if (strncmp(stream_data->xpath, "/ietf-datastores:running", 24) == 0) {
		stream_data->xpath += 24;
		stream_data->ds = SR_DS_RUNNING;
	} else if (strncmp(stream_data->xpath, "/ietf-datastores:candidate", 26) == 0) {
		stream_data->xpath += 26;
		stream_data->ds = SR_DS_CANDIDATE;
	} else if (strncmp(stream_data->xpath, "/ietf-datastores:operational", 28) == 0) {
		stream_data->xpath += 28;
		stream_data->ds = SR_DS_OPERATIONAL;
	} else if (strncmp(stream_data->xpath, "/ietf-datastores:factory-default", 32) == 0) {
		stream_data->xpath += 32;
		stream_data->ds = SR_DS_FACTORY_DEFAULT;
	} else
		return -1;

	return 0;
}

static int
resconf_parse_resource(struct http2_stream_data *stream_data)
{
	if (!strlen(stream_data->xpath)) {
		stream_data->resource = RES_ROOT;
	} else if (strncmp(stream_data->xpath, "/data", 5) == 0) {
		stream_data->resource = RES_DATA;
		stream_data->xpath += 5;
	} else if (strncmp(stream_data->xpath, "/operations", 11) == 0) {
		stream_data->resource = RES_OP;
		stream_data->xpath += 11;
	} else if (strncmp(stream_data->xpath, "/yang-library-version", 21) == 0) {
		stream_data->resource = RES_YANG;
		stream_data->xpath += 21;
	} else if (strncmp(stream_data->xpath, "/ds", 3) == 0) {
		stream_data->resource = RES_DATA;
		stream_data->xpath += 3;
		return resconf_parse_datastore(stream_data);
	} else
		return -1;

	return 0;
}

static int
get_yang_version(nghttp2_session *session,
                 struct http2_session_data *session_data,
                 struct http2_stream_data *stream_data)
{
	static const char xpath[] = "/sysrepo:sysrepo-modules/*[name='ietf-yang-library']";
	sr_data_t *data;
	struct ly_set *set;
	int ret;

	if (sr_get_module_info(session_data->app_ctx->connection, &data))
		goto err;

	if (lyd_find_xpath(data->tree, xpath, &set))
		goto err;

	ret = lyd_print_mem(&stream_data->output, set->objs[0], stream_data->out_format, LYD_PRINT_SIBLINGS | LYD_PRINT_SHRINK);
	ly_set_free(set, NULL);
	if (ret)
		goto err;

	return str_reply(session, stream_data, stream_data->output);

err:
	return NGHTTP2_ERR_CALLBACK_FAILURE;
}

int
restconf_request(nghttp2_session *session,
                 struct http2_session_data *session_data,
                 struct http2_stream_data *stream_data)
{
	stream_data->xpath = stream_data->request_path + 9;
	if (resconf_parse_resource(stream_data))
		goto error;

	if ((stream_data->method == METHOD_GET) &&
	    (stream_data->resource == RES_YANG))
		return get_yang_version(session, session_data, stream_data);


	fprintf(stderr, "XPATH %s %d %d\n", stream_data->xpath, stream_data->resource, stream_data->method);

	if (sr_session_start(session_data->app_ctx->connection, stream_data->ds, &stream_data->session))
	 	fprintf(stderr, "sr_session_start error\n");

	switch (stream_data->resource) {
	case RES_DATA:
		switch (stream_data->method) {
		case METHOD_HEAD:
		case METHOD_GET:
			return data_get(session, stream_data);
		case METHOD_PUT:
			return data_put(session, stream_data);
		case METHOD_OPTIONS:
			return options_reply(session, stream_data, "OPTIONS, HEAD, GET, PUT");
		}
		break;
	case RES_OP:
		switch (stream_data->method) {
		case METHOD_OPTIONS:
			return options_reply(session, stream_data, "OPTIONS, POST");
		case METHOD_POST:
			return op_post(session, stream_data);
		}
	}


error:
	if (error_reply(session, stream_data) != 0)
		return NGHTTP2_ERR_CALLBACK_FAILURE;

	return 0;
}
