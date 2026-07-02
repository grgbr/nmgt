#define _GNU_SOURCE

#include <sysrepo.h>
#include <stdlib.h>
#include <assert.h>

#define cli_assert(...) assert(__VA_ARGS__)

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

#define cli_pref_err(_prefix, _format, ...) \
	fprintf(stderr, "%s: " _format "\n", _prefix, _format, ## __VA_ARGS__)

#define cli_err(_format, ...) \
	cli_pref_err(program_invocation_short_name, _format, ## __VA_ARGS__)

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
	cli_assert_ops((_work)->ops)

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
	cli_assert(context);

	cli_free(work);
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
	sr_data_t *        select;
};

#define cli_assert_context(_ctx) \
	cli_assert(_ctx); \
	cli_assert((_ctx)->conn); \
	cli_assert((_ctx)->sess)

int
cli_load_config(const struct cli_context * context,
                const char *               xpath,
                unsigned int               depth,
                sr_data_t **               data)
{
	int err;

	err = sr_get_data(context->sess,
	                  xpath,
	                  depth,
	                  0,
	                  SR_OPER_NO_STATE,
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

void
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
		cli_err("cannot schedule work.");
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

int
cli_init(struct cli_context * context)
{
	cli_assert(context);

	int err;

	/* Enable info logging at sysrepo level. */
	sr_log_stderr(SR_LL_INF);

	err = sr_connect(0, &context->conn);
	if (err != SR_ERR_OK)
		return err;

	err = sr_session_start(context->conn, SR_DS_RUNNING, &context->sess);
	if (err != SR_ERR_OK)
		goto disconnect;

	context->wkcnt = 0;
	cli_setup_node(&context->root, &cli_root_ops);
	context->select = NULL;

	return SR_ERR_OK;

disconnect:
	sr_disconnect(context->conn);

	return err;
}

static int
cli_visitn_destroy_node(const struct cli_node *  node,
                        enum cli_tree_walk_event event,
                        void *                   data)
{
	if (event == CLI_TREE_WALK_POST_EVT)
		cli_destroy_node(node);

	return CLI_TREE_WALK_CONT_RET;
}

void
cli_fini(struct cli_context * context)
{
	cli_assert_context(context);

	cli_unload(state->select);
	cli_release_workq(context);
	cli_node_tree_walk_safe(&context->root, cli_visitn_destroy_node);
	sr_session_stop(context->sess);
	sr_disconnect(context->conn);
}

/******************************************************************************
 * Node handling
 ******************************************************************************/

typedef int cli_node_parse_fn(const struct cli_node *,
                              int,
                              const char * const [],
                              struct cli_context *);

struct cli_node_ops {
	cli_node_parse_fn * parse;
};

#define cli_assert_node_ops(_ops) \
	cli_assert(_ops); \
	cli_assert((_ops)->parse)

struct cli_node {
	const struct cli_node_ops * ops;
	struct cli_node *           next;
	struct cli_node *           prev;
	struct cli_node *           child;
};

#define CLI_NODE_SETUP(_ops) \
	{ .ops = _ops, .next = NULL, .child = NULL }

enum cli_tree_walk_event {
	CLI_TREE_WALK_PRE_EVT,
	CLI_TREE_WALK_POST_EVT,
	CLI_TREE_WALK_EVT_NR
};

typedef int cli_node_tree_visit_fn(const struct cli_node *,
                                   enum cli_tree_walk_event,
                                   void *);

#define cli_assert_node(_node) \
	cli_assert(_node); \
	cli_assert_node_ops((_node)->ops)

#define cli_foreach_child(_node, _child) \
	for (_child = (_node)->child; _child; _child = (_child)->next)

#define cli_foreach_child_safe(_node, _child, _tmp) \
	for (_child = (_node)->child, _tmp = (_child)->next; \
	     _child; \
	     _child = _tmp, _tmp = (_tmp)->next)

static int
cli_node_tree_walk_recurs(struct cli_node *        root,
                          cli_node_tree_visit_fn * visit,
                          void *                   data)
{
	cli_assert_node(tree);
	cli_assert(visit);

	int ret;

	ret = visit(root, CLI_TREE_WALK_PRE_ORDER, data);
	if (ret == CLI_TREE_WALK_CONT_RET) {
		const struct cli_node * child;

		cli_foreach_child(root, child) {
			ret = cli_node_tree_walk_recurs(child, visit, data);
			if (ret < 0)
				return ret;
		}

		ret = visit(node, CLI_TREE_WALK_POST_ORDER, data);
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

	const struct cli_node * child;

	cli_foreach_child(tree, child) {
		ret = cli_node_tree_walk_recurs(child, visit, data);
		if (ret < 0)
			break;
	}

	return ret;
}

static int
cli_node_tree_walk_recurs_safe(struct cli_node *        root,
                               cli_node_tree_visit_fn * visit,
                               void *                   data)
{
	cli_assert_node(tree);
	cli_assert(visit);

	int ret;

	ret = visit(root, CLI_TREE_WALK_PRE_ORDER, data);
	if (ret == CLI_TREE_WALK_CONT_RET) {
		const struct cli_node * child;
		const struct cli_node * tmp;

		cli_foreach_child_safe(root, child, tmp) {
			ret = cli_node_tree_walk_recurs(child, visit, data);
			if (ret < 0)
				return ret;
		}

		ret = visit(node, CLI_TREE_WALK_POST_ORDER, data);
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

	const struct cli_node * child;
	const struct cli_node * tmp;

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

	return node->ops->parse(node, argc, argv, context);
}

static void
cli_setup_node(struct cli_node * node, const struct cli_node_ops * ops)
{
	cli_assert(node);
	cli_assert_node_ops(ops);

	node->ops = ops;
	node->next = NULL;
	node->child = NULL;
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

void
cli_destroy_node(struct cli_node * node)
{
	cli_assert_node(node);

	cli_free(node);
}

/******************************************************************************
 * `xpath' command parser.
 ******************************************************************************/

struct cli_xpath_work {
	struct cli_work super;
	char *          xpath;
	unsigned int    depth;
};

static int
cli_xpath_exec_work(const struct cli_work * work,
                    struct cli_context *    context)
{
	/* TODO: set prompt. */

	return cli_load_config(context, wk->xpath, wk->depth, &context->select);
}

static void
cli_xpath_release_work(const struct cli_work * work,
                       struct cli_context *    context)
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

	wk = cli_create_work(sizeof(*wk), &cli_xpath_work_ops);
	wk->xpath = xpath;
	wk->depth = depth;

	return cli_sched_work(context, &wk->super);
}

static int
cli_xpath_parse(const struct cli_node * node,
                int                     argc,
                const char * const      argv[],
                struct cli_context *      state)
{
	if ((argc == 2) && (!strcmp(argv[0], "xpath"))) {
		size_t len;

		len = strnlen(argv[1]);
		if (!len || len >= CLI_XPATH_MAX) {
			cli_pref_err("xpath", "'%s': XPATH too long.", argv[1]);
			return -EINVAL;
		}

		return cli_xpath_sched_work(argv[1], 1, context);
	}
	else
		return 0;
}

static const struct cli_node_ops cli_xpath_cmd_ops = {
	.parse = cli_xpath_parse
};

static struct cli_node cli_xpath_cmd = CLI_NODE_SETUP(&cli_xpath_cmd_ops);

int
cli_init_parse()
{

}

int
cli_parse(int argc, char * const argv[])
{
	assert(argc);
	assert(argv);
	assert(argv[0]);
	assert(argv[0][0]);
	assert(!isspace(argv[0][0]));

	return cli_parse_node((argv[0][0] != '/') ? cli_curr : &cli_root,
	                      argc,
	                      argv);
}
