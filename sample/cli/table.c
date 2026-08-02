#include "table.h"
#include "cli.h"

#define CLI_TABLE_COLUMN_INIT_NR   (2U)

#define cli_table_assert_col_label(_label) \
	cli_assert(_label); \
	cli_assert(_label[0] != '\0'); \
	cli_assert(strnlen(_label, CLI_TABLE_LABEL_MAX) < \
	           CLI_TABLE_LABEL_MAX)

#define cli_table_assert_col_whint(_whint) \
	cli_assert(_whint > 0.); \
	cli_assert(_whint <= CLI_TABLE_COLUMN_WIDTH_MAX)

#define cli_table_assert_col_flags(_flags) \
	cli_assert(!((_flags) & \
		     ~(SCOLS_FL_TRUNC | \
		       SCOLS_FL_TREE | \
		       SCOLS_FL_RIGHT | \
		       SCOLS_FL_STRICTWIDTH | \
		       SCOLS_FL_NOEXTREMES | \
		       SCOLS_FL_HIDDEN | \
		       SCOLS_FL_WRAP)))

#define cli_table_assert_col_desc(_col) \
	cli_assert(_col); \
	cli_table_assert_col_label((_col)->label); \
	cli_table_assert_col_whint((_col)->whint); \
	cli_table_assert_col_flags((_col)->flags)

#define cli_table_assert_desc(_table) \
	cli_assert(_table); \
	cli_assert((_table)->nr); \
	cli_assert((_table)->nr <= CLI_TABLE_COLUMN_MAX); \
	cli_assert((_table)->cols)

int
cli_table_show(const struct cli_table * table, bool maxout, FILE * stdio)
{
	cli_table_assert(table);
	cli_assert(stdio);

	int ret;

	ret = scols_table_set_stream(table->scols, stdio);
	cli_assert(!ret);

	if (!maxout) {
		scols_table_enable_maxout(table->scols, 0);
		scols_table_enable_minout(table->scols, 1);
	}
	else {
		scols_table_enable_minout(table->scols, 0);
		scols_table_enable_maxout(table->scols, 1);
	}

	ret = scols_print_table(table->scols);
	if (ret == -ENOMEM)
		abort();

	return ret;
}

void
cli_table_cell_set_data(struct libscols_cell * cell, const char * data)
{
	cli_assert(cell);

	int err;

	err = scols_cell_set_data(cell, data);
	if (err) {
		cli_assert(err == -ENOMEM);
		abort();
	}
}

void
cli_table_cell_set_color(struct libscols_cell * cell, char * color)
{
	cli_assert(cell);

	int err;

	err = scols_cell_set_color(cell, color);
	if (err) {
		cli_assert(err == -ENOMEM);
		abort();
	}
}

int
cli_table_new_col(const struct cli_table * table,
                  const char *             label,
                  double                   whint,
                  int                      flags)
{
	cli_table_assert(table);
	cli_table_assert_col_label(label);
	cli_table_assert_col_whint(whint);
	cli_table_assert_col_flags(flags);

	size_t                   len;
	struct libscols_column * col;
	int                      err;

	len = strnlen(label, CLI_TABLE_LABEL_MAX);
	if (!len)
		return -ENODATA;
	if (len >= CLI_TABLE_LABEL_MAX)
		return -ENAMETOOLONG;

	if (scols_table_get_ncols(table->scols) >= CLI_TABLE_COLUMN_MAX)
		return -ENOBUFS;

	col = scols_table_new_column(table->scols, label, whint, flags);
	if (!col) {
		cli_assert(errno == ENOMEM);
		abort();
	}

	/* Highlight column header title. */
	err = scols_cell_set_color(scols_column_get_header(col), "bold");
	cli_assert(!err);

	return 0;
}

struct libscols_line *
cli_table_new_line(const struct cli_table * table)
{
	struct libscols_line * ln;

	ln = scols_table_new_line(table->scols, NULL);
	if (!ln) {
		cli_assert(errno == ENOMEM);
		abort();
	}

	return ln;
}

void
cli_table_init(struct cli_table *         table,
               const struct cli_context * context,
               bool                       nohead,
               cli_table_load_fn *        load)
{
	cli_assert(table);
	cli_assert_context(context);
	cli_assert(load);

	struct libscols_table * tbl;

	tbl = scols_new_table();
	if (!tbl) {
		cli_assert(errno == ENOMEM);
		abort();
	}

	scols_table_enable_ascii(tbl, 1);
	scols_table_enable_colors(tbl, (int)cli_has_colors(context));
	scols_table_enable_noheadings(tbl, nohead);

	table->load = load;
	table->scols = tbl;
}

void
cli_table_init_from_desc(struct cli_table *            table,
                         const struct cli_context *    context,
                         const struct cli_table_desc * descriptor,
                         cli_table_load_fn *           load)
{
	cli_assert(table);
	cli_assert_context(context);
	cli_table_assert_desc(descriptor);
	cli_assert(load);

	unsigned int c;

	cli_table_init(table, context, descriptor->nohead, load);

	for (c = 0; c < descriptor->nr; c++) {
		const struct cli_table_column_desc * cdesc =
			&descriptor->cols[c];
		int                                  err;

		cli_table_assert_col_desc(cdesc);

		err = cli_table_new_col(table,
		                        cdesc->label,
		                        cdesc->whint,
		                        cdesc->flags);
		cli_assert(!err);
	}
}
