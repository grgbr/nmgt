#define _GNU_SOURCE

#define CONFIG_CLI_ASSERT 1
#define CONFIG_CLI_LOG 1
#define CONFIG_CLI_LOG_LEVEL 5

#include <sysrepo.h>
#include <stdbool.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include <assert.h>

#if defined(CONFIG_CLI_ASSERT)
#define cli_assert(...) assert(__VA_ARGS__)
#else  /* !defined(CONFIG_CLI_ASSERT) */
#define cli_assert(...)
#endif /* defined(CONFIG_CLI_ASSERT) */

/*
 * Maximum size available to store a command line including the terminating NULL
 * byte.
 */
#define CLI_LINE_MAX (1024U)

/*
 * Maximum size available to store a xpath including the terminating NULL byte.
 */
#define CLI_XPATH_MAX (128U)

#define CLI_TREE_WALK_CONT_RET (0)
#define CLI_TREE_WALK_SKIP_RET (1)

enum cli_tree_walk_event {
	CLI_TREE_WALK_PRE_EVT,
	CLI_TREE_WALK_POST_EVT,
	CLI_TREE_WALK_EVT_NR
};

static void *
cli_malloc(size_t size)
{
	cli_assert(size);

	void * data;

	data = malloc(size);
	if (!data)
		abort();

	return data;
}

static void
cli_free(void * data)
{
	free(data);
}

#define cli_log(_format, ...) \
	fprintf(stderr, \
	        "%s: " _format "\n", \
	        program_invocation_short_name, \
	        ## __VA_ARGS__)

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
 * Work handling
 ******************************************************************************/

struct cli_work;
struct cli_context;

typedef int cli_work_exec_fn(const struct cli_work *, struct cli_context *);

typedef void cli_work_release_fn(struct cli_work *, struct cli_context *);

struct cli_work_ops {
	cli_work_exec_fn *    exec;
	cli_work_release_fn * release;
};

#define cli_assert_work_ops(_ops) \
	cli_assert(_ops); \
	cli_assert((_ops)->exec); \
	cli_assert((_ops)->release)

struct cli_work {
	const struct cli_work_ops * ops;
};

#define cli_assert_work(_work) \
	cli_assert(_work); \
	cli_assert_work_ops((_work)->ops)

static struct cli_work *
cli_create_work(size_t size, const struct cli_work_ops * ops)
{
	cli_assert(size >= sizeof(struct cli_work));
	cli_assert_work_ops(ops);

	struct cli_work * wk;

	wk = cli_malloc(size);
	wk->ops = ops;

	return wk;
}

static void
cli_release_work(struct cli_work * work, struct cli_context * context)
{
	cli_assert_work(work);
	cli_assert(context);

	work->ops->release(work, context);
}

static void
cli_destroy_work(struct cli_work * work)
{
	cli_assert_work(work);

	cli_free(work);
}

/******************************************************************************
 * Node handling
 ******************************************************************************/

struct cli_node;

typedef int cli_node_parse_fn(const struct cli_node *,
                              int,
                              const char * const [],
                              struct cli_context *);

typedef void cli_node_release_fn(struct cli_node *, struct cli_context *);

struct cli_node_ops {
	cli_node_parse_fn *   parse;
	cli_node_release_fn * release;
};

#define cli_assert_node_ops(_ops) \
	cli_assert(_ops); \
	cli_assert((_ops)->parse); \
	cli_assert((_ops)->release); \

struct cli_node {
	const struct cli_node_ops * ops;
	struct cli_node *           next;
	struct cli_node *           prev;
	struct cli_node *           child;
};

#define CLI_NODE_SETUP(_node, _ops) \
	{ .ops = _ops, .next = NULL, .prev = &(_node), .child = NULL }

typedef int cli_node_tree_visit_fn(struct cli_node *,
                                   enum cli_tree_walk_event,
                                   void *);

#define cli_assert_node(_node) \
	cli_assert(_node); \
	cli_assert_node_ops((_node)->ops)

#define cli_foreach_child(_node, _child) \
	for (_child = (_node)->child; _child; _child = (_child)->next)

#define cli_foreach_child_safe(_node, _child, _tmp) \
	for (_child = (_node)->child; \
	     _child && (_tmp = (_child)->next); \
	     _child = _tmp, _tmp = (_child)->next)

static int
cli_node_tree_walk_recurs(struct cli_node *        node,
                          cli_node_tree_visit_fn * visit,
                          void *                   data)
{
	cli_assert_node(node);
	cli_assert(visit);

	int ret;

	ret = visit(node, CLI_TREE_WALK_PRE_EVT, data);
	if (ret == CLI_TREE_WALK_CONT_RET) {
		struct cli_node * child;

		cli_foreach_child(node, child) {
			ret = cli_node_tree_walk_recurs(child, visit, data);
			if (ret < 0)
				return ret;
		}

		ret = visit(node, CLI_TREE_WALK_POST_EVT, data);
	}

	return (ret != CLI_TREE_WALK_SKIP_RET) ? ret : CLI_TREE_WALK_CONT_RET;
}

static int
cli_node_tree_walk(struct cli_node *        tree,
                   cli_node_tree_visit_fn * visit,
                   void *                   data)
{
	cli_assert_node(tree);
	cli_assert(visit);

	struct cli_node * child;
	int               ret = 0;

	cli_foreach_child(tree, child) {
		ret = cli_node_tree_walk_recurs(child, visit, data);
		if (ret < 0)
			break;
	}

	return ret;
}

static int
cli_node_tree_walk_recurs_safe(struct cli_node *        node,
                               cli_node_tree_visit_fn * visit,
                               void *                   data)
{
	cli_assert_node(node);
	cli_assert(visit);

	int ret;

	ret = visit(node, CLI_TREE_WALK_PRE_EVT, data);
	if (ret == CLI_TREE_WALK_CONT_RET) {
		struct cli_node * child;
		struct cli_node * tmp;

		cli_foreach_child_safe(node, child, tmp) {
			ret = cli_node_tree_walk_recurs(child, visit, data);
			if (ret < 0)
				return ret;
		}

		ret = visit(node, CLI_TREE_WALK_POST_EVT, data);
	}

	return (ret != CLI_TREE_WALK_SKIP_RET) ? ret : CLI_TREE_WALK_CONT_RET;
}

static int
cli_node_tree_walk_safe(struct cli_node *        tree,
                        cli_node_tree_visit_fn * visit,
                        void *                   data)
{
	cli_assert_node(tree);
	cli_assert(visit);

	struct cli_node * child;
	struct cli_node * tmp;
	int               ret = 0;

	cli_foreach_child_safe(tree, child, tmp) {
		ret = cli_node_tree_walk_recurs_safe(child, visit, data);
		if (ret < 0)
			break;
	}

	return ret;
}

void
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

#define CLI_WORK_NR (128U)

struct cli_context {
	sr_conn_ctx_t *    conn;
	sr_session_ctx_t * sess;
	unsigned int       wkcnt;
	struct cli_work *  wkq[CLI_WORK_NR];
	struct cli_node    root;
	struct ly_out *    lyout;
	sr_data_t *        select;
};

#define cli_assert_context(_ctx) \
	cli_assert(_ctx); \
	cli_assert((_ctx)->conn); \
	cli_assert((_ctx)->sess); \
	cli_assert((_ctx)->lyout)

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

static void
cli_register_cmd(struct cli_context * context, struct cli_node * command)
{
	cli_node_add_child(&context->root, command);
}

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

	err = sr_connect(0, &context->conn);
	if (err != SR_ERR_OK) {
		cli_log("cannot open repo connection: %s",
		        sr_strerror(err));
		return err;
	}

	err = sr_session_start(context->conn, SR_DS_RUNNING, &context->sess);
	if (err != SR_ERR_OK) {
		cli_log("cannot start repo session: %s",
		        sr_strerror(err));
		goto disconnect;
	}

	err = ly_out_new_file(stdout, &context->lyout);
	if (err != LY_SUCCESS) {
		cli_log("cannot open yang output printer: %s",
		        ly_strerr(err));
		err = SR_ERR_LY;
		goto stop;
	}

	context->wkcnt = 0;
	cli_setup_node(&context->root, &cli_root_ops);
	context->select = NULL;

	return SR_ERR_OK;

stop:
	sr_session_stop(context->sess);
disconnect:
	sr_disconnect(context->conn);

	return err;
}

static int
cli_visitn_destroy_node(struct cli_node *        node,
                        enum cli_tree_walk_event event,
                        void *                   data)
{
	if (event == CLI_TREE_WALK_POST_EVT)
		cli_release_node(node, data);

	return CLI_TREE_WALK_CONT_RET;
}

static void
cli_fini_context(struct cli_context * context)
{
	cli_assert_context(context);

	cli_unload(context->select);
	cli_release_workq(context);
	cli_node_tree_walk_safe(&context->root,
	                        cli_visitn_destroy_node,
	                        context);
	ly_out_free(context->lyout, NULL, 0);
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
cli_schema_exec_work(const struct cli_work * work,
                     struct cli_context *    context)
{
	int ret;

	if (!context->select) {
		ret = cli_load_config(context, "/*", 0, &context->select);
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
		                     LYS_OUT_TREE, //LYS_OUT_YANG_COMPILED,
		                     0,
		                     0);
	}
#else
	ret = lys_print_module(context->lyout,
	                       context->select->tree->schema->module,
	                       LYS_OUT_TREE,
	                       0,
	                       0);
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
cli_init(struct cli_context * context)
{
	int ret;

	ret = cli_init_context(context);
	if (ret)
		return ret;

	cli_register_cmd(context, &cli_xpath_cmd);
	cli_register_cmd(context, &cli_schema_cmd);

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
