#include "lyd_table.h"
#include "yang.h"

static int
cli_lyd_table_load(const struct cli_table *   table,
                   const struct cli_context * context,
                   void *                     data __cli_unused)
{
	cli_lyd_table_assert((const struct cli_lyd_table *)table);
	cli_assert_context(context);

	unsigned int           l;
	struct libscols_line * ln;

	cli_table_foreach_line(table, l, ln) {
		const struct lysc_node * scn;
		sr_data_t *              node;
		int                      err;
		struct libscols_cell *   cell;
		const char * const *     style = cli_get_style(context);
		const char *             color;

		scn = cli_table_line_get_userdata(ln);
		cli_assert(scn);

		err = cli_lyd_load_node_from_schema(context, scn, &node);
		cell = cli_table_line_get_cell(ln, 1);
		if (!err) {
			cli_table_cell_set_data(cell, cli_lyd_node_value(node));

			color = !cli_lyd_node_is_default(node)
			        ? cli_style_get_color(style,
			                              CLI_VALUE_STYLE_KIND)
			        : cli_style_get_color(style,
			                              CLI_DEFAULT_STYLE_KIND);
			cli_table_cell_set_color(cell, color);
		}
		else {
			cli_table_cell_set_data(cell, "??");


			color = cli_style_get_color(style,
			                            CLI_ERROR_STYLE_KIND);
			cli_table_cell_set_color(cell, color);
		}

		cli_lyd_unload(node);
	}

	return 0;
}

int
cli_lyd_table_init(struct cli_lyd_table *         table,
                   const struct lysc_node *       node,
                   cli_lyd_table_filter_node_fn * filter,
                   const struct cli_context *     context)
{
	cli_assert(table);
	cli_assert(node);
	cli_assert(filter);
	cli_assert_context(context);

	struct libscols_column * col;
	const struct lysc_node * child;
	int                      err;

	cli_table_init(&table->super, context, true, cli_lyd_table_load);

#if CLI_TABLE_COLUMN_MAX < 2
#error Invalid maximum number of table columns !
#endif
	cli_assert(sizeof("Attribute") <= CLI_TABLE_LABEL_MAX);
	col = cli_table_new_col(&table->super, "Attribute", 0.3, 0);

	cli_assert(sizeof("Value") <= CLI_TABLE_LABEL_MAX);
	cli_table_new_col(&table->super, "Value", 0.7, SCOLS_FL_WRAP);

	cli_lysc_foreach_child(node, child) {
		if ((child->nodetype == LYS_LEAF) && filter(child)) {
			struct libscols_line *        ln;
			struct libscols_cell *        cell;
			const struct lysc_node_leaf * leaf =
				(const struct lysc_node_leaf *)child;

			ln = cli_table_new_line(&table->super);
			cli_table_line_set_userdata(ln, (void *)leaf);

			cell = cli_table_line_get_cell(ln, 0);
			cli_table_cell_set_data(cell, leaf->name);
		}
	}

	if (!cli_table_get_line_count(&table->super)) {
		err = -ENOENT;
		goto fini;
	}

	cli_table_col_set_color(col, cli_style_get_color(cli_get_style(context),
	                                                 CLI_LABEL_STYLE_KIND));

	return 0;

fini:
	cli_table_fini(&table->super);

	return err;
}
