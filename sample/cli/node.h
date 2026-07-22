#ifndef _CLI_NODE_H
#define _CLI_NODE_H

#include "common.h"
#include <stdbool.h>

struct cli_node {
	struct cli_node * next;
	struct cli_node * prev;
	struct cli_node * child;
	struct cli_node * parent;
};

#define cli_node_assert(_node) \
	cli_assert(_node)

#define cli_node_foreach_sibling(_head, _sib) \
	for (_sib = _head; _sib; _sib = (_sib)->next)

#define cli_node_foreach_sibling_safe(_head, _sib, _tmp) \
	for (_sib = _head; \
	     (_sib) && (_tmp = (_sib)->next, 1); \
	     _sib = (_tmp))

#define cli_node_foreach_child(_node, _child) \
	cli_node_foreach_sibling((_node)->child, _child)

#define cli_node_foreach_child_safe(_node, _child, _tmp) \
	cli_node_foreach_sibling_safe((_node)->child, _child, _tmp)

static inline bool
cli_node_has_child(const struct cli_node * node)
{
	cli_node_assert(node);

	return !!node->child;
}

#define CLI_WALK_CONT_RET (0)
#define CLI_WALK_SKIP_RET (1)

enum cli_walk_event {
	CLI_WALK_PRE_EVT,
	CLI_WALK_POST_EVT,
	CLI_WALK_EVT_NR
};

typedef int cli_node_visit_fn(struct cli_node *, enum cli_walk_event, void *);

extern int
cli_node_walk_siblings(struct cli_node *   head,
                       cli_node_visit_fn * visit,
                       void *              data);

static inline int
cli_node_walk(struct cli_node * node, cli_node_visit_fn * visit, void * data)
{
	cli_node_assert(node);
	cli_assert(visit);

	return cli_node_walk_siblings(node->child, visit, data);
}

extern int
cli_node_walk_siblings_safe(struct cli_node *   head,
                            cli_node_visit_fn * visit,
                            void *              data);

static inline int
cli_node_walk_safe(struct cli_node *   node,
                   cli_node_visit_fn * visit,
                   void *              data)
{
	cli_node_assert(node);
	cli_assert(visit);

	return cli_node_walk_siblings_safe(node->child, visit, data);
}

extern void
cli_node_add_sibling(struct cli_node ** head, struct cli_node * sibling);

static inline void
cli_node_add_child(struct cli_node * node, struct cli_node * child)
{
	cli_node_assert(node);
	cli_node_assert(child);

	cli_node_add_sibling(&node->child, child);
}

extern void
cli_node_setup(struct cli_node * node);

extern struct cli_node *
cli_node_create(size_t size);

extern void
cli_node_destroy(struct cli_node * node);

#endif /* _CLI_NODE_H */
