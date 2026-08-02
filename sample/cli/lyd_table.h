#ifndef _CLI_LYD_TABLE_H
#define _CLI_LYD_TABLE_H

#include "table.h"

struct lysc_node;

struct cli_lyd_table {
	struct cli_table super;
};

#define cli_lyd_table_assert(_table) \
	cli_assert(_table); \
	cli_table_assert(&(_table)->super)

typedef bool cli_lyd_table_filter_node_fn(const struct lysc_node * node);

extern int
cli_lyd_table_init(struct cli_lyd_table *         table,
                   const struct lysc_node *       node,
                   cli_lyd_table_filter_node_fn * filter,
                   const struct cli_context *     context);

static inline void
cli_lyd_table_fini(struct cli_lyd_table * table)
{
	cli_lyd_table_assert(table);

	cli_table_fini(&table->super);
}

#endif /* _CLI_LYD_TABLE_H */
