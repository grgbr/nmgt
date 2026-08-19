#include "lyd_table.h"
#include "yang.h"

static void
cli_lyd_table_set_node_cell(struct libscols_cell *  cell,
                            const char * const *    style,
                            const struct lyd_node * node)
{
	cli_assert(cell);
	cli_assert(node);

	const char * color;

	cli_table_cell_set_data(cell, cli_lyd_value(node));

	color = !cli_lyd_is_default(node)
	        ? cli_style_get_color(style, CLI_VALUE_STYLE_KIND)
	        : cli_style_get_color(style, CLI_DEFAULT_STYLE_KIND);

	cli_table_cell_set_color(cell, color);
}

static void
cli_lyd_table_set_err_cell(struct libscols_cell *  cell,
                           const char * const *    style)
{
	cli_assert(cell);

	cli_table_cell_set_data(cell, "??");

	cli_table_cell_set_color(cell,
	                         cli_style_get_color(style,
	                                             CLI_ERROR_STYLE_KIND));
}

static int
cli_lyd_table_load(struct cli_table *         table,
                   const struct cli_context * context,
                   void *                     data)
{
	cli_lyd_table_assert((const struct cli_lyd_table *)table);
	cli_assert_context(context);
	cli_lyd_assert_flags((sr_get_oper_flag_t)data);

	sr_data_t *            cont;
	const char * const *   style = cli_get_style(context);
	int                    err;
	unsigned int           l;
	struct libscols_line * ln;

	err = cli_lyd_load_data(context,
	                        cli_table_get_userdata(table),
	                        2,
	                        (sr_get_oper_flag_t)data,
	                        &cont);
	if (!err) {
		cli_table_foreach_line(table, l, ln) {
			const struct lyd_node * leaf;
			struct libscols_cell *  cell =
				cli_table_line_get_cell(ln, 1);

			err = cli_lyd_find(cont->tree,
			                   cli_table_line_get_userdata(ln),
			                   (struct lyd_node **)&leaf);
			if (!err)
				cli_lyd_table_set_node_cell(cell, style, leaf);
			else
				cli_lyd_table_set_err_cell(cell, style);
		}

		cli_lyd_unload_data(cont);
	}
	else {
		cli_table_foreach_line(table, l, ln)
			cli_lyd_table_set_err_cell(
				cli_table_line_get_cell(ln, 1),
				style);
	}

	return 0;
}

static void
_cli_lyd_table_fini(struct cli_table * table)
{
	cli_lyd_table_assert((const struct cli_lyd_table *)table);

	cli_free(cli_table_get_userdata(table));
}

static const struct cli_table_ops cli_lyd_table_ops = {
	.load = cli_lyd_table_load,
	.fini = _cli_lyd_table_fini
};

int
cli_lyd_table_init(struct cli_lyd_table *             table,
                   const struct lysc_node_container * container,
                   cli_lyd_table_filter_node_fn *     filter,
                   const struct cli_context *         context)
{
	cli_assert(table);
	cli_assert(container);
	cli_assert(filter);
	cli_assert_context(context);

	struct libscols_column * col;
	const struct lysc_node * child;
	int                      err;

	cli_table_init(&table->super, context, true, &cli_lyd_table_ops);

	cli_assert(sizeof("Attribute") <= CLI_TABLE_LABEL_MAX);
	col = cli_table_new_col(&table->super, "Attribute", 0.3, 0);

	cli_assert(sizeof("Value") <= CLI_TABLE_LABEL_MAX);
	cli_table_new_col(&table->super, "Value", 0.7, SCOLS_FL_WRAP);

	cli_lysc_foreach_child((const struct lysc_node *)container, child) {
		if ((child->nodetype == LYS_LEAF) && filter(child)) {
			struct libscols_line *        ln;
			struct libscols_cell *        cell;
			const struct lysc_node_leaf * leaf =
				(const struct lysc_node_leaf *)child;

			ln = cli_table_new_line(&table->super);
			cli_table_line_set_userdata(ln, (void *)leaf->name);

			cell = cli_table_line_get_cell(ln, 0);
			cli_table_cell_set_data(cell, leaf->name);
		}
	}

	if (!cli_table_get_line_count(&table->super)) {
		err = -ENOENT;
		goto fini;
	}

	cli_table_set_userdata(&table->super,
	                       cli_lysc_node_xpath((const struct lysc_node *)
	                                           container));
	cli_table_col_set_color(col,
	                        cli_style_get_color(cli_get_style(context),
	                                            CLI_LABEL_STYLE_KIND));

	return 0;

fini:
	cli_table_fini(&table->super);

	return err;
}

static void
cli_lyd_table_load_list_entry(const struct cli_table * table,
                              const char * const *     style,
                              const struct lyd_node *  entry)
{
	cli_lyd_table_assert((const struct cli_lyd_table *)table);
	cli_assert(entry);

	struct libscols_line *   ln = cli_table_new_line(table);
	unsigned int             c;
	struct libscols_column * col;

	cli_assert(ln);

	cli_table_foreach_col(table, c, col) {
		struct libscols_cell *   cell = cli_table_line_get_cell(ln, c);
		const struct lyd_node *  leaf;
		int                      err;

		cli_assert(cell);

		err = cli_lyd_find(entry,
		                   cli_table_col_get_userdata(col),
		                   (struct lyd_node **)&leaf);
		if (!err)
			cli_lyd_table_set_node_cell(cell, style, leaf);
		else
			cli_lyd_table_set_err_cell(cell, style);
	}
}

static void
cli_lyd_table_load_err_entry(const struct cli_table * table,
                             const char * const *     style)
{
	cli_lyd_table_assert((const struct cli_lyd_table *)table);

	struct libscols_line *   ln = cli_table_new_line(table);
	unsigned int             c;
	struct libscols_column * col;

	cli_assert(ln);

	cli_table_foreach_col(table, c, col)
		cli_lyd_table_set_err_cell(cli_table_line_get_cell(ln, c),
		                           style);
}

static int
cli_lyd_table_load_list(struct cli_table *         table,
                        const struct cli_context * context,
                        void *                     data)
{
	cli_lyd_table_assert((const struct cli_lyd_table *)table);
	cli_assert_context(context);
	cli_lyd_assert_flags((sr_get_oper_flag_t)data);

	sr_data_t *          list;
	const char * const * style = cli_get_style(context);
	int                  err;

	cli_table_remove_lines(table);

	err = cli_lyd_load_data(context,
	                        cli_table_get_userdata(table),
	                        2,
	                        (sr_get_oper_flag_t)data,
	                        &list);
	if (!err) {
		struct lyd_node * entry;

		cli_lyd_foreach_data(list, entry)
			cli_lyd_table_load_list_entry(table, style, entry);

		cli_lyd_unload_data(list);
	}
	else
		cli_lyd_table_load_err_entry(table, style);

	return 0;
}

static const struct cli_table_ops cli_lyd_table_list_ops = {
	.load = cli_lyd_table_load_list,
	.fini = _cli_lyd_table_fini
};

#define CLI_LYD_TABLE_LIST_INIT_NR (4U)
#if CLI_LYD_TABLE_LIST_INIT_NR > CLI_TABLE_COLUMN_MAX
#error Invalid maximum number of data list table columns !
#endif

int
cli_lyd_table_init_list(struct cli_lyd_table *         table,
                        const struct lysc_node_list *  list,
                        cli_lyd_table_filter_node_fn * filter,
                        const struct cli_context *     context)
{
	cli_assert(table);
	cli_assert(list);
	cli_assert(filter);
	cli_assert_context(context);

	unsigned int                   cnt = 0;
	unsigned int                   nr = CLI_LYD_TABLE_LIST_INIT_NR;
	const struct lysc_node_leaf ** fields = NULL;
	const struct lysc_node *       child;
	int                            err;
	unsigned int                   c;

	fields = cli_malloc(nr * sizeof(fields[0]));
	cli_assert(fields);
	cli_lysc_foreach_child((struct lysc_node *)list, child) {
		if ((child->nodetype == LYS_LEAF) && filter(child)) {
			assert(nr <= CLI_TABLE_COLUMN_MAX);
			assert(cnt <= nr);

			if (cnt == nr) {
				nr *= 2;
				if (nr >= CLI_TABLE_COLUMN_MAX) {
					err = -ENOBUFS;
					goto fini;
				}

				fields = cli_realloc(
					fields,
					sizeof(nr * sizeof(fields[0])));
				cli_assert(fields);
			}

			fields[cnt++] = (const struct lysc_node_leaf *)child;
		}
	}

	if (!cnt) {
		err = -ENOENT;
		goto free;
	}

	cli_table_init(&table->super, context, false, &cli_lyd_table_list_ops);

	for (c = 0; c < cnt; c++) {
		struct libscols_column * col;

		col = cli_table_new_col(&table->super,
		                        fields[c]->name,
		                        (double)cnt / 100.0,
		                        SCOLS_FL_WRAP);
		if (!col) {
			cli_assert(errno != ENOBUFS);
			err = -errno;
			goto fini;
		}

		cli_table_col_set_userdata(col, (void *)fields[c]->name);

		cli_table_cell_set_color(
			cli_table_col_get_head(col),
			cli_style_get_color(cli_get_style(context),
			                    CLI_LABEL_STYLE_KIND));
	}

	cli_table_set_userdata(&table->super,
	                       cli_lysc_node_xpath((const struct lysc_node *)
	                                           list));

	cli_free(fields);

	return 0;

fini:
	cli_table_fini(&table->super);
free:
	cli_free(fields);

	return err;
}
