#include "yang.h"
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include <assert.h>

/*
 * Maximum size available to store a command line including the terminating NULL
 * byte.
 */
#define CLI_LINE_MAX (1024U)

/*
 * Maximum size available to store a xpath including the terminating NULL byte.
 */
#define CLI_XPATH_MAX (128U)

/******************************************************************************
 * Utilities
 ******************************************************************************/

#if defined(CONFIG_CLI_ASSERT)

bool
cli_ischr_valid(int chr)
{
	return isalnum(chr) || ispunct(chr) || isblank(chr) || (chr == '\n');
}

void
cli_assert_args(int argc, const char * const argv[])
{
	cli_assert(argc);
	cli_assert(argv);

	unsigned int a;
	size_t       len;

	for (a = 0, len = 0; a < argc; a++) {
		cli_assert(argv[a]);

		size_t       alen = strnlen(argv[a], CLI_LINE_MAX);
		unsigned int c;

		len += alen;
		cli_assert(len < CLI_LINE_MAX);

		for (c = 0; c < alen; c++)
			cli_assert(cli_ischr_valid(argv[a][c]));
	}

	cli_assert(argv[argc] == NULL);
}

#else  /* !defined(CONFIG_CLI_ASSERT) */

static inline void
cli_assert_args(int argc, const char * const argv[])
{
}

#endif /* defined(CONFIG_CLI_ASSERT) */

/******************************************************************************
 * Node handling
 ******************************************************************************/

#define cli_assert_node_ops(_ops) \
	cli_assert(_ops); \
	cli_assert((_ops)->parse); \
	cli_assert((_ops)->release); \

#define cli_assert_node(_node) \
	cli_assert(_node); \
	cli_assert_node_ops((_node)->ops)

#define CLI_NODE_SETUP(_node, _ops) \
	{ .ops = _ops, .next = NULL, .prev = &(_node), .child = NULL }

#define cli_foreach_child(_node, _child) \
	for (_child = (_node)->child; _child; _child = (_child)->next)

#define cli_foreach_child_safe(_node, _child, _tmp) \
	for (_child = (_node)->child; \
	     _child && (_tmp = (_child)->next); \
	     _child = _tmp, _tmp = (_child)->next)

typedef int cli_node_visit_fn(struct cli_context *,
                              struct cli_node *,
                              enum cli_walk_event,
                              void *);

static int
cli_walk_node_recurs(struct cli_context * context,
                     struct cli_node *    node,
                     cli_node_visit_fn *  visit,
                     void *               data)
{
	cli_assert_context(context);
	cli_assert_node(node);
	cli_assert(visit);

	int ret;

	ret = visit(context, node, CLI_WALK_PRE_EVT, data);
	if (ret == CLI_WALK_CONT_RET) {
		struct cli_node * child;

		cli_foreach_child(node, child) {
			ret = cli_walk_node_recurs(context, child, visit, data);
			if (ret < 0)
				return ret;
		}

		ret = visit(context, node, CLI_WALK_POST_EVT, data);
	}

	cli_assert(ret <= 0);
	return ret;
}

static int
cli_walk_node(struct cli_context * context,
              struct cli_node *    node,
              cli_node_visit_fn *  visit,
              void *               data)
{
	cli_assert_context(context);
	cli_assert_node(node);
	cli_assert(visit);

	struct cli_node * child;
	int               ret = 0;

	cli_foreach_child(node, child) {
		ret = cli_walk_node_recurs(context, child, visit, data);
		if (ret < 0)
			return ret;
	}

	cli_assert(!ret);
	return 0;
}

static int
cli_walk_node_recurs_safe(struct cli_context * context,
                          struct cli_node *    node,
                          cli_node_visit_fn *  visit,
                          void *               data)
{
	cli_assert_context(context);
	cli_assert_node(node);
	cli_assert(visit);

	int ret;

	ret = visit(context, node, CLI_WALK_PRE_EVT, data);
	if (ret == CLI_WALK_CONT_RET) {
		struct cli_node * child;
		struct cli_node * tmp;

		cli_foreach_child_safe(node, child, tmp) {
			ret = cli_walk_node_recurs_safe(context,
			                                child,
			                                visit,
			                                data);
			if (ret < 0)
				return ret;
		}

		ret = visit(context, node, CLI_WALK_POST_EVT, data);
	}

	cli_assert(ret <= 0);
	return ret;
}

static int
cli_walk_node_safe(struct cli_context * context,
                   struct cli_node *    node,
                   cli_node_visit_fn *  visit,
                   void *               data)
{
	cli_assert_context(context);
	cli_assert_node(node);
	cli_assert(visit);

	struct cli_node * child;
	struct cli_node * tmp;
	int               ret = 0;

	cli_foreach_child_safe(node, child, tmp) {
		ret = cli_walk_node_recurs_safe(context, child, visit, data);
		if (ret < 0)
			return ret;
	}

	cli_assert(!ret);
	return 0;
}

static void
cli_node_add_child(struct cli_node * node, struct cli_node * child)
{
	cli_assert_node(node);
	cli_assert_node(child);

	struct cli_node * head = node->child;

	if (head) {
		struct cli_node * tail = head->prev;

		child->prev = tail;
		tail->next = child;
		head->prev = child;
	}
	else {
		child->prev = child;
		node->child = child;
	}
}

static inline int
cli_parse_node(const struct cli_node * node,
               int                     argc,
               const char * const      argv[],
               struct cli_context *    context)
{
	cli_assert_node(node);
	cli_assert_args(argc, argv);
	cli_assert(context);

	return node->ops->parse(node, argc, argv, context);
}

static void
cli_setup_node(struct cli_node * node, const struct cli_node_ops * ops)
{
	cli_assert(node);
	cli_assert_node_ops(ops);

	node->ops = ops;
	node->next = NULL;
	node->prev = node;
	node->child = NULL;
}

static void
cli_release_node_null(struct cli_node * node, struct cli_context * context)
{
	cli_assert_node(node);
	cli_assert(context);
}

static inline void
cli_release_node(struct cli_node * node, struct cli_context * context)
{
	cli_assert_node(node);
	cli_assert(context);

	node->ops->release(node, context);
}

static struct cli_node *
cli_create_node(size_t size, const struct cli_node_ops * ops)
{
	cli_assert(size >= sizeof(struct cli_node));
	cli_assert_node_ops(ops);

	struct cli_node * node;

	node = cli_malloc(size);
	cli_setup_node(node, ops);

	return node;
}

static void
cli_destroy_node(struct cli_node * node)
{
	cli_assert_node(node);

	cli_free(node);
}

/******************************************************************************
 * Overall cli context handling
 ******************************************************************************/

static int
cli_load_config(const struct cli_context * context,
                const char *               xpath,
                unsigned int               depth,
                sr_data_t **               data)
{
	cli_assert_context(context);
	cli_assert(xpath);
	cli_assert(strnlen(xpath, CLI_XPATH_MAX) < CLI_XPATH_MAX);
	cli_assert(data);

	int err;

	err = sr_get_data(context->sess,
	                  xpath,
	                  depth,
	                  0,
	                  SR_OPER_DEFAULT,
	                  data);
	if (err != SR_ERR_OK)
		return err;

	if (!*data)
		return SR_ERR_NOT_FOUND;

	if (!(*data)->tree) {
		sr_release_data(*data);
		*data = NULL;
		return SR_ERR_NOT_FOUND;
	}

	return SR_ERR_OK;
}

static void
cli_unload(sr_data_t * data)
{
	/* data may be NULL here. */
	sr_release_data(data);
}

static int
cli_sched_work(struct cli_context * context, struct cli_work * work)
{
	cli_assert_context(context);
	cli_assert_work(work);

	if (context->wkcnt >= CLI_WORK_NR) {
		cli_log("cannot schedule work.");
		return -EBUSY;
	}

	context->wkq[context->wkcnt++] = work;

	return 0;
}

static void
cli_release_workq(struct cli_context * context)
{
	cli_assert_context(context);

	unsigned int w;

	for (w = 0; w < context->wkcnt; w++)
		cli_release_work(context->wkq[w], context);

	context->wkcnt = 0;
}

static int
cli_exec_workq(struct cli_context * context)
{
	cli_assert_context(context);

	unsigned int w;
	int          ret;

	for (w = 0, ret = 0; (w < context->wkcnt) && !ret; w++) {
		struct cli_work * wk = context->wkq[w];

		ret = wk->ops->exec(wk, context);
	}

	cli_release_workq(context);

	return ret;
}

static int
cli_parse_root(const struct cli_node * node,
               int                     argc,
               const char * const      argv[],
               struct cli_context *    context)
{
	struct cli_node * child;
	int               ret;

	cli_foreach_child(node, child) {
		ret = cli_parse_node(child, argc, argv, context);
		if (ret)
			break;
	}

	if (ret < 0)
		return ret;

	if (ret != argc) {
		cli_log("'%s': invalid command.", argv[0]);
		return -EINVAL;
	}

	return 0;
}

static const struct cli_node_ops cli_root_ops = {
	.parse   = cli_parse_root,
	.release = cli_release_node_null
};

static int
cli_parse(struct cli_context * context, int argc, const char * const argv[])
{
	cli_assert_context(context);

	return cli_parse_node(&context->root, argc, argv, context);
}

#if defined(CONFIG_CLI_LOG)

static sr_log_level_t cli_log_lvl;

static void
cli_sr_log_cb(sr_log_level_t level, const char * message)
{
	if (level <= cli_log_lvl) {
		const char * lvl;

		switch (level) {
		case SR_LL_ERR:
			lvl = "err";
			break;
		case SR_LL_WRN:
			lvl = "warn";
			break;
		case SR_LL_INF:
			lvl = "info";
			break;
		case SR_LL_VRB:
			lvl = "verb";
			break;
		case SR_LL_DBG:
			lvl = "dbg";
			break;
		default:
			cli_assert(0);
		}

		cli_log("{%*.*s} repo: %s", 4, 4, lvl, message);
	}
}

static void
cli_setup_log(sr_log_level_t level)
{
	/* Disable all internal sysrepo / libyang logging messages... */
	sr_log_stderr(SR_LL_NONE);

	/* ... and setup our own logging callback instead. */
	switch (level) {
	case SR_LL_NONE:
		/* Disable sysrepo / libyang logging entirely. */
		sr_log_set_cb(NULL);
		break;

	case SR_LL_ERR:
	case SR_LL_WRN:
	case SR_LL_INF:
	case SR_LL_VRB:
	case SR_LL_DBG:
		/*
		 * Install our logging callback. It will always be called for
		 * every messages, regardless of any log level.
		 */
		cli_log_lvl = level;
		sr_log_set_cb(cli_sr_log_cb);
		break;

	default:
		cli_assert(0);
	}
}

#else  /* !defined(CONFIG_CLI_LOG) */

static void
cli_setup_log(sr_log_level_t level)
{
	/* Disable sysrepo / libyang logging entirely. */
	sr_log_set_cb(NULL);
}

#endif /* defined(CONFIG_CLI_LOG) */

static int
cli_init_context(struct cli_context * context)
{
	cli_assert(context);

	int err;

	cli_setup_log(CONFIG_CLI_LOG_LEVEL);

	err = sr_connect(SR_CONN_DEFAULT, &context->conn);
	if (err != SR_ERR_OK) {
		cli_log("cannot open repo connection: %s",
		        sr_strerror(err));
		return err;
	}

	err = sr_session_start(context->conn, SR_DS_OPERATIONAL, &context->sess);
	if (err != SR_ERR_OK) {
		cli_log("cannot start repo session: %s",
		        sr_strerror(err));
		goto disconnect;
	}

	context->lyctx = sr_session_acquire_context(context->sess);
	assert(context->lyctx);

	err = ly_out_new_file(stdout, &context->lyout);
	if (err != LY_SUCCESS) {
		cli_log("cannot open yang output printer: %s",
		        ly_strerr(err));
		err = SR_ERR_LY;
		goto release;
	}

	context->wkcnt = 0;
	cli_setup_node(&context->root, &cli_root_ops);
	context->select = NULL;
	context->isatty = !!isatty(STDOUT_FILENO);

	return SR_ERR_OK;

release:
	sr_session_release_context(context->sess);
	sr_session_stop(context->sess);
disconnect:
	sr_disconnect(context->conn);

	return err;
}

static int
cli_visitn_destroy_node(struct cli_context * context,
                        struct cli_node *    node,
                        enum cli_walk_event  event,
                        void *               data)
{
	if (event == CLI_WALK_POST_EVT)
		cli_release_node(node, context);

	return CLI_WALK_CONT_RET;
}

static void
cli_fini_context(struct cli_context * context)
{
	cli_assert_context(context);

	cli_unload(context->select);
	cli_release_workq(context);
	cli_walk_node_safe(context,
	                   &context->root,
	                   cli_visitn_destroy_node,
	                   NULL);
	ly_out_free(context->lyout, NULL, 0);
	sr_session_release_context(context->sess);
	sr_session_stop(context->sess);
	sr_disconnect(context->conn);
}

/******************************************************************************
 * `xpath' command parser.
 ******************************************************************************/

struct cli_xpath_work {
	struct cli_work super;
	const char *    xpath;
	unsigned int    depth;
};

static int
cli_xpath_exec_work(const struct cli_work * work,
                    struct cli_context *    context)
{
	const struct cli_xpath_work * wk = (const struct cli_xpath_work *)work;
	int                           ret;

	ret = cli_load_config(context, wk->xpath, wk->depth, &context->select);
	if (ret != SR_ERR_OK)
		cli_log("xpath: '%s': %s.", wk->xpath, sr_strerror(ret));

#warning TODO: set prompt

	return ret;
}

static void
cli_xpath_release_work(struct cli_work * work, struct cli_context * context)
{
	cli_destroy_work(work);
}

static const struct cli_work_ops cli_xpath_work_ops = {
	.exec    = cli_xpath_exec_work,
	.release = cli_xpath_release_work
};

static int
cli_xpath_sched_work(const char *         xpath,
                     unsigned int         depth,
                     struct cli_context * context)
{
	struct cli_xpath_work * wk;

	wk = (struct cli_xpath_work *)cli_create_work(sizeof(*wk),
	                                              &cli_xpath_work_ops);
	wk->xpath = xpath;
	wk->depth = depth;

	return cli_sched_work(context, &wk->super);
}

static int
cli_xpath_parse_cmd(const struct cli_node * node,
                    int                     argc,
                    const char * const      argv[],
                    struct cli_context *    context)
{
	cli_assert_node(node);
	cli_assert_args(argc, argv);
	cli_assert_context(context);

	if (strcmp(argv[0], "xpath"))
		return 0;

	if (argc == 2) {
		size_t len = strnlen(argv[1], CLI_XPATH_MAX);

		if (len && (len < CLI_XPATH_MAX)) {
			int ret;

			ret = cli_xpath_sched_work(argv[1], 1, context);
			if (ret)
				return ret;

			return 2;
		}

		cli_log("xpath: '%s': XPATH too long.", argv[1]);
	}
	else
		cli_log("xpath: missing argument.");

	return -EINVAL;
}

static const struct cli_node_ops cli_xpath_cmd_ops = {
	.parse   = cli_xpath_parse_cmd,
	.release = cli_release_node_null,
};

static struct cli_node cli_xpath_cmd = CLI_NODE_SETUP(cli_xpath_cmd,
                                                      &cli_xpath_cmd_ops);

/******************************************************************************
 * `schema' command parser.
 ******************************************************************************/

static int
cli_show_module_yang(struct cli_context * context,
                     const char *         module)
{
	const struct lys_module * mod;
	int                       ret;

	mod = cli_lys_find_module(context, module);
	if (!mod) {
		cli_log("'%s': module not found.", module);
		return SR_ERR_NOT_FOUND;
	}

	ret = cli_lys_print_module_yang(context, mod, 0);
	if (ret != LY_SUCCESS) {
		cli_log("'%s': cannot show module YANG: %s.",
		        module,
		        ly_strerr(ret));
		return SR_ERR_LY;
	}

	return SR_ERR_OK;
}

static int
cli_show_module_diag(struct cli_context * context, const char * module)
{
	const struct lys_module * mod;
	int                       ret;

	mod = cli_lys_find_module(context, module);
	if (!mod) {
		cli_log("'%s': invalid module.", module);
		return SR_ERR_NOT_FOUND;
	}

	ret = cli_lys_print_module_diag(context, mod);
	if (ret != LY_SUCCESS) {
		cli_log("'%s': cannot show module tree diagram: %s.",
		        module,
		        ly_strerr(ret));
		return SR_ERR_LY;
	}

	return SR_ERR_OK;
}

static int
cli_show_node_yang(struct cli_context *     context,
                   const struct lysc_node * subtree,
                   const char *             xpath)
{
	const struct lysc_node * node;
	int                      ret;

	node = cli_lysc_find_node(context, subtree, xpath);
	if (!node) {
		cli_log("'%s': XPATH node not found.", xpath);
		return SR_ERR_NOT_FOUND;
	}

	ret = cli_lysc_print_node_yang(context, node, 0);
	if (ret != LY_SUCCESS) {
		cli_log("'%s': cannot show XPATH node YANG: %s.",
		        xpath,
		        ly_strerr(ret));
		return SR_ERR_LY;
	}

	return SR_ERR_OK;
}

static int
cli_show_nodeset_yang(struct cli_context *     context,
                      const struct lysc_node * subtree,
                      const char *             xpath)
{
	struct ly_set * nodes;

	nodes = cli_lysc_find_nodeset(context, subtree, xpath);
	if (nodes) {
		cli_assert(nodes->count);

		int ret;

		ret = cli_lysc_print_nodeset_yang(context, nodes, 0);
		ly_set_free(nodes, NULL);

		if (ret == LY_SUCCESS)
			return SR_ERR_OK;

		cli_log("'%s': cannot show XPATH nodes YANG: %s.",
		        xpath,
		        ly_strerr(ret));
		return SR_ERR_LY;
	}
	else {
		cli_log("'%s': XPATH nodes not found.", xpath);
		return SR_ERR_NOT_FOUND;
	}
}

static int
cli_show_node_diag(struct cli_context *     context,
                   const struct lysc_node * subtree,
                   const char *             xpath)
{
	const struct lysc_node * node;
	int                      ret;

	node = cli_lysc_find_node(context, subtree, xpath);
	if (!node) {
		cli_log("'%s': XPATH node not found.", xpath);
		return SR_ERR_NOT_FOUND;
	}

	ret = cli_lysc_print_node_diag(context, node);
	if (ret != LY_SUCCESS) {
		cli_log("'%s': cannot show XPATH node tree diagram: %s.",
		        xpath,
		        ly_strerr(ret));
		return SR_ERR_LY;
	}

	return SR_ERR_OK;
}

static int
cli_show_nodeset_diag(struct cli_context *     context,
                      const struct lysc_node * subtree,
                      const char *             xpath)
{
	struct ly_set * nodes;

	nodes = cli_lysc_find_nodeset(context, subtree, xpath);
	if (nodes) {
		cli_assert(nodes->count);

		int ret;

		ret = cli_lysc_print_nodeset_diag(context, nodes);
		ly_set_free(nodes, NULL);

		if (ret == LY_SUCCESS)
			return SR_ERR_OK;

		cli_log("'%s': cannot show XPATH nodes tree diagram: %s.",
		        xpath,
		        ly_strerr(ret));
		return SR_ERR_LY;
	}
	else {
		cli_log("'%s': XPATH nodes not found.", xpath);
		return SR_ERR_NOT_FOUND;
	}
}

static int
cli_schema_exec_work(const struct cli_work * work,
                     struct cli_context *    context)
{
	int ret;

	//cli_show_node_yang(context, NULL, "/oven:oven-state");

	//cli_show_nodeset_yang(context, NULL, "/oven:*");
	// first unprefixed top-level container
	//cli_show_nodeset_yang(context, NULL, "/oven");
	//cli_show_nodeset_yang(context, NULL, "oven");

	//cli_show_node_diag(context, NULL, "/oven:oven-state");

	//cli_show_nodeset_diag(context, NULL, "/oven:*");
	// first unprefixed top-level container
	//cli_show_nodeset_diag(context, NULL, "/oven");
	//cli_show_nodeset_diag(context, NULL, "oven");

#if 0
	if (!context->select) {
		ret = cli_load_config(context, "/oven:oven-state/temperature", 0, &context->select);
		if (ret != SR_ERR_OK) {
			cli_log("schema: cannot load data: %s.",
			        sr_strerror(ret));
			return ret;
		}
#warning TODO: set prompt
	}

#warning FIXME (implemented / internal / out format)
#if 1
	const struct lyd_node * node;
	LY_LIST_FOR(context->select->tree, node) {
		ret = lys_print_node(context->lyout,
		                     node->schema,
		                     LYS_OUT_TREE,
		                     0,
		                     0/* LYS_PRINT_NO_SUBSTMT */);
	}
#else
	const struct lyd_node * node;
	LY_LIST_FOR(context->select->tree, node) {
		ret = lys_print_module(context->lyout,
		                       node->schema->module,
		                       LYS_OUT_TREE,
		                       0,
		                       0/* LYS_PRINT_NO_SUBSTMT */);
	}
#endif
#endif
	if (ret != LY_SUCCESS) {
		cli_log("schema: cannot display: %s.", ly_strerr(ret));
		return SR_ERR_LY;
	}

	return SR_ERR_OK;
}

static void
cli_schema_release_work(struct cli_work * work, struct cli_context * context)
{
	cli_destroy_work(work);
}

static const struct cli_work_ops cli_schema_work_ops = {
	.exec    = cli_schema_exec_work,
	.release = cli_schema_release_work
};

static int
cli_schema_sched_work(struct cli_context * context)
{
	struct cli_work * wk;

	wk = cli_create_work(sizeof(*wk), &cli_schema_work_ops);

	return cli_sched_work(context, wk);
}

static int
cli_schema_parse_cmd(const struct cli_node * node,
                     int                     argc,
                     const char * const      argv[],
                     struct cli_context *    context)
{
	cli_assert_node(node);
	cli_assert_args(argc, argv);
	cli_assert_context(context);

	if (strcmp(argv[0], "schema"))
		return 0;

	if (argc == 1) {
		cli_schema_sched_work(context);

		return 1;
	}
	else
		cli_log("schema: too many argument.");

	return -EINVAL;
}

static const struct cli_node_ops cli_schema_cmd_ops = {
	.parse =   cli_schema_parse_cmd,
	.release = cli_release_node_null
};

static struct cli_node cli_schema_cmd = CLI_NODE_SETUP(cli_schema_cmd,
                                                       &cli_schema_cmd_ops);

/******************************************************************************
 * Top-level logic
 ******************************************************************************/

static int
cli_generate_cmd(struct cli_context *     context,
                 const struct lysc_node * node,
                 enum cli_walk_event      event,
                 void *                   data)
{

	switch (node->nodetype) {
	case LYS_CONTAINER:
		/*
		cmd = cli_create_node(sizeof(struct cli_node),
		                const struct cli_node_ops * ops);
		cli_node_add_child(parent, cmd);
		return CLI_WALK_SKIP_RET;
		*/

	default:
		{
			const char * xpath;

			xpath = cli_lysc_xpath(node);
			cli_log("'%s': %s support not implemented !",
			        xpath,
			        cli_ly_nodetype_str(node->nodetype));
			/* cli_free(xpath); */
			assert(0);
		}
	}
}

static int
cli_init(struct cli_context * context)
{
	int                       ret;
	unsigned int              m;
	const struct lys_module * mod;

	ret = cli_init_context(context);
	if (ret)
		return ret;

	cli_node_add_child(&context->root, &cli_xpath_cmd);
	cli_node_add_child(&context->root, &cli_schema_cmd);

	cli_lys_foreach_module(context, m, mod) {
		ret = cli_lys_walk_module(context,
		                          mod,
		                          cli_generate_cmd,
		                          NULL);
		if (ret)
			return ret;
	}

	return 0;
}

static void
cli_fini(struct cli_context * context)
{
	cli_fini_context(context);
}

int
main(int argc, const char * const argv[])
{
	struct cli_context ctx;
	int                ret;

	if (argc < 2) {
		cli_log("invalid arguments.");
		return EXIT_FAILURE;
	}

	ret = cli_init(&ctx);
	if (ret)
		goto fini;

	ret = cli_parse(&ctx, argc - 1, &argv[1]);
	if (ret)
		goto fini;

	ret = cli_exec_workq(&ctx);

fini:
	cli_fini(&ctx);

	return (!ret) ? EXIT_SUCCESS : EXIT_FAILURE;
}
