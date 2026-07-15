#include "node.h"

static int
cli_node_walk_recurs(struct cli_node *   node,
                     cli_node_visit_fn * visit,
                     void *              data)
{
	cli_node_assert(node);
	cli_assert(visit);

	int ret;

	ret = visit(node, CLI_WALK_PRE_EVT, data);
	if (ret == CLI_WALK_CONT_RET) {
		struct cli_node * child;

		cli_node_foreach_child(node, child) {
			ret = cli_node_walk_recurs(child, visit, data);
			if (ret < 0)
				return ret;
		}

		ret = visit(node, CLI_WALK_POST_EVT, data);
		cli_assert(ret <= 0);
	}

	return (ret >= 0) ? 0 : ret;
}

int
cli_node_walk_siblings(struct cli_node *   head,
                       cli_node_visit_fn * visit,
                       void *              data)
{
	cli_assert(!head || !head->prev->next);
	cli_assert(visit);

	struct cli_node * sib;
	int               ret = 0;

	cli_node_foreach_sibling(head, sib) {
		ret = cli_node_walk_recurs(sib, visit, data);
		if (ret < 0)
			return ret;
	}

	cli_assert(!ret);

	return 0;
}

static int
cli_node_walk_recurs_safe(struct cli_node *   node,
                          cli_node_visit_fn * visit,
                          void *              data)
{
#warning Remove recursion
	cli_node_assert(node);
	cli_assert(visit);

	int ret;

	ret = visit(node, CLI_WALK_PRE_EVT, data);
	if (ret == CLI_WALK_CONT_RET) {
		struct cli_node * child;
		struct cli_node * tmp;

		cli_node_foreach_child_safe(node, child, tmp) {
			ret = cli_node_walk_recurs_safe(child, visit, data);
			if (ret < 0)
				return ret;
		}

		ret = visit(node, CLI_WALK_POST_EVT, data);
		cli_assert(ret <= 0);
	}

	return (ret >= 0) ? 0 : ret;
}

int
cli_node_walk_siblings_safe(struct cli_node *   head,
                            cli_node_visit_fn * visit,
                            void *              data)
{
	cli_assert(!head || !head->prev->next);
	cli_assert(visit);

	struct cli_node * sib;
	struct cli_node * tmp;
	int               ret = 0;

	cli_node_foreach_sibling_safe(head, sib, tmp) {
		ret = cli_node_walk_recurs_safe(sib, visit, data);
		if (ret < 0)
			return ret;
	}

	cli_assert(!ret);

	return 0;
}

void
cli_node_add_sibling(struct cli_node ** head, struct cli_node * sibling)
{
	cli_assert(head);
	cli_assert(!(*head) || !(*head)->prev->next);
	cli_node_assert(sibling);
	cli_assert(!sibling->next);
	cli_assert(sibling->prev == sibling);
	cli_assert(!sibling->parent);

	struct cli_node * hd = *head;

	if (hd) {
		struct cli_node * tail = hd->prev;

		sibling->prev = tail;
		tail->next = sibling;
		hd->prev = sibling;
	}
	else
		*head = sibling;

	sibling->parent = cli_containerof(head, struct cli_node, child);
}

void
cli_node_setup(struct cli_node * node)
{
	cli_assert(node);

	node->next = NULL;
	node->prev = node;
	node->child = NULL;
	node->parent = NULL;
}

struct cli_node *
cli_node_create(size_t size)
{
	cli_assert(size >= sizeof(struct cli_node));

	struct cli_node * node;

	node = cli_malloc(size);
	cli_assert(node);
	cli_node_setup(node);

	return node;
}

void
cli_node_destroy(struct cli_node * node)
{
	cli_free(node);
}
