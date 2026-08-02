#ifndef _CLI_BUILD_H
#define _CLI_BUILD_H

#include "dir.h"

struct cli_dir;
struct cli_context;
struct lysc_node;
enum cli_walk_event;

struct cli_tree_build {
	struct cli_dir * parent;
};

#define CLI_TREE_BUILD_INIT(_root) \
	{ .parent = _root }

static inline void
cli_build_setup(struct cli_tree_build * builder, struct cli_dir * directory)
{
	cli_assert(builder);
	cli_dir_assert(directory);

	builder->parent = directory;
}

extern int
cli_build_tree_dir(struct cli_context *     context,
                   const struct lysc_node * node,
                   enum cli_walk_event      event,
                   void *                   data);

#endif /* _CLI_BUILD_H */
