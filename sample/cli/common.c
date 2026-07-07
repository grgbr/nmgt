#include "common.h"
#include <sys/ioctl.h>

void *
cli_malloc(size_t size)
{
	cli_assert(size);

	void * data;

	data = malloc(size);
	if (!data)
		abort();

	return data;
}

unsigned int
cli_term_cols(const struct cli_context * context)
{
	cli_assert_context(context);

	struct winsize wsz;

#warning TODO: plug in a SIGWINCH signal handler instead
	if (context->isatty && !ioctl(STDOUT_FILENO, TIOCGWINSZ, &wsz))
		return (unsigned int)wsz.ws_col;
	else
		return 0;
}

/******************************************************************************
 * Base node handling
 ******************************************************************************/

#define cli_node_assert_ops(_ops) \
	cli_assert(_ops); \
	cli_assert((_ops)->parse); \
	cli_assert((_ops)->release); \

#define cli_node_assert(_node) \
	cli_assert(_node); \
	cli_node_assert_ops((_node)->ops)

#define CLI_NODE_SETUP(_node, _ops) \
	{ \
		.ops    = _ops, \
		.next   = NULL, \
		.prev   = &(_node), \
		.child  = NULL, \
		.parent = NULL \
	}

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

#warning Remove recursion
static int
cli_walk_node_recurs(struct cli_context * context,
                     struct cli_node *    node,
                     cli_node_visit_fn *  visit,
                     void *               data)
{
	cli_assert_context(context);
	cli_node_assert(node);
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
		cli_assert(ret <= 0);
	}

	return (ret >= 0) ? 0 : ret;
}

#warning Remove recursion
static int
cli_walk_node(struct cli_context * context,
              struct cli_node *    node,
              cli_node_visit_fn *  visit,
              void *               data)
{
	cli_assert_context(context);
	cli_node_assert(node);
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

#warning Remove recursion
static int
cli_walk_node_recurs_safe(struct cli_context * context,
                          struct cli_node *    node,
                          cli_node_visit_fn *  visit,
                          void *               data)
{
	cli_assert_context(context);
	cli_node_assert(node);
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
		cli_assert(ret <= 0);
	}

	return (ret >= 0) ? 0 : ret;
}

#warning Remove recursion
static int
cli_walk_node_safe(struct cli_context * context,
                   struct cli_node *    node,
                   cli_node_visit_fn *  visit,
                   void *               data)
{
	cli_assert_context(context);
	cli_node_assert(node);
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
	cli_node_assert(node);
	cli_node_assert(child);

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

	child->parent = node;
}

static void
cli_setup_node(struct cli_node * node, const struct cli_node_ops * ops)
{
	cli_assert(node);
	cli_node_assert_ops(ops);

	node->ops = ops;
	node->next = NULL;
	node->prev = node;
	node->child = NULL;
	node->parent = NULL;
}

static void
cli_release_node_null(struct cli_node * node, struct cli_context * context)
{
	cli_node_assert(node);
	cli_assert(context);
}

static struct cli_node *
cli_create_node(size_t size, const struct cli_node_ops * ops)
{
	cli_assert(size >= sizeof(struct cli_node));
	cli_node_assert_ops(ops);

	struct cli_node * node;

	node = cli_malloc(size);
	cli_setup_node(node, ops);

	return node;
}

static void
cli_destroy_node(struct cli_node * node)
{
	cli_node_assert(node);

	cli_free(node);
}

static void
cli_releasen_destroy_node(struct cli_node * node, struct cli_context * context)
{
	cli_node_assert(node);
	cli_assert(context);

	cli_destroy_node(node);
}


