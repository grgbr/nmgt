#ifdef _CLI_TABLE_H
#define _CLI_TABLE_H

#include "common.h"

struct cli_table_column_desc {
	const char * label;
	double       whint;
	int          flags;
};

/* TODO: add static assert support !! */
#define CLI_TABLE_COLUMN_INIT_DESC(_label, _whint, _flags) \
	{ \
		.label = _label, \
		.whint = _whint, \
		.flags = _flags \
	}

struct cli_table_desc {
	bool                                 nohead;
	unsigned int                         cnt;
	unsigned int                         nr;
	const struct cli_table_column_desc * cols;
};

/* TODO: add static assert support !! */
#define CLI_TABLE_INIT_DESC(_cols) \
	{ \
		.cnt   = cli_array_nr(_cols), \
		.nr    = cli_array_nr(_cols), \
		.cols  = _cols
	}

struct cli_table {
	const struct cli_table      * scols;
	const struct cli_table_desc * desc;
};


#endif /* _CLI_TABLE_H */

#include "table.h"
#include <libsmartcols/libsmartcols.h>

#define CLI_TABLE_COLUMN_INIT_NR   (2U)
#define CLI_TABLE_COLUMN_MAX       (32U)
#define CLI_TABLE_LABEL_MAX        (64U)
#define CLI_TABLE_COLUMN_WIDTH_MAX (128.)


#define cli_table_assert_desc(_table) \
	cli_assert(_table); \
	cli_assert((_table)->nr); \
	cli_assert((_table)->cnt <= (_table)->nr); \
	cli_assert((_table)->cols)

#define cli_table_assert_col_flags(_flags) \
	cli_assert(!((_flags) & \
		     (SCOLS_FL_TRUNC | \
		      SCOLS_FL_TREE | \
		      SCOLS_FL_RIGHT | \
		      SCOLS_FL_STRICTWIDTH | \
		      SCOLS_FL_NOEXTREMES | \
		      SCOLS_FL_HIDDEN | \
		      SCOLS_FL_WRAP)))

#define cli_table_assert_col_desc(_col) \
	cli_assert(_col); \
	cli_assert((_col)->label); \
	cli_assert((_col)->label[0] != '\0'); \
	cli_assert(strnlen((_col)->label, CLI_TABLE_LABEL_MAX) < \
	           CLI_TABLE_LABEL_MAX); \
	cli_assert((_col)->whint > 0.); \
	cli_assert((_col)->whint <= CLI_TABLE_COLUMN_WIDTH_MAX); \
	cli_table_assert_col_flags((_col)->flags)

int
cli_table_new_col_desc(struct cli_table_desc * table,
                       const char *            label,
                       double                  whint,
                       int                     flags)
{
	cli_table_assert_desc(table);
	cli_assert(label);
	cli_assert(label[0] != '\0');
	cli_assert(strnlen(label, CLI_TABLE_LABEL_MAX) < CLI_TABLE_LABEL_MAX);
	cli_assert(whint > 0.);
	cli_assert(whint <= CLI_TABLE_COLUMN_WIDTH_MAX);
	cli_table_assert_col_flags(flags);

	struct cli_table_column_desc * col;

	if (table->cnt >= table->nr) {
		unsigned int nr = table->nr * 2;

		if (nr > CLI_TABLE_COLUMN_MAX)
			return -ENOBUFS;

		table->cols = cli_realloc(table->cols,
		                          nr * sizeof(table->cols[0]));
		cli_assert(table->cols);

		table->nr = nr;
	}

	col = (struct cli_table_column_desc *)table->cols[table->cnt++];
	col->label = label;
	col->whint = whint;
	col->flags = flags;

	return 0;
}

void
cli_table_init_desc(struct cli_table_desc * table, bool noheadings)
{
	cli_assert(table);

	table->nohead = noheadings;
	table->cnt = 0;
	table->nr = CLI_TABLE_COLUMN_INIT_NR;
	table->cols = cli_malloc(CLI_TABLE_COLUMN_INIT_NR *
	                         sizeof(table->cols[0]));
}

void
cli_table_fini_desc(struct cli_table_desc * table)
{
	cli_assert(table);

	cli_free(tables->cols);
}

/******************************************************************************/

cli_table_load
cli_table_clear
cli_table_show

int
cli_table_init(struct cli_table *            table,
               const struct cli_table_desc * desc,
               bool                          maxout,
               const struct cli_context *    context)
{
	cli_assert(table);
	cli_table_assert_desc(desc);

	struct libscols_table * tbl;
	unsigned int            c;

	tbl = scols_new_table();
	if (!tbl) {
		cli_assert(errno == ENOMEM);
		return -errno;
	}

	scols_table_enable_colors(tbl, cli_has_colors(context));
	scols_table_enable_noheadings(tbl, desc->nohead);

	for (c = 0; c < desc->cnt; c++) {
		const struct cli_table_column_desc * cdesc = &desc->cols[c];
		struct libscols_column *             col;
		int                                  err;

		cli_table_assert_col_desc(cdesc);

		col = scols_table_new_column(tbl,
		                             cdesc->label,
		                             cdesc->whint,
		                             cdesc->flags);
		if (!col) {
			cli_assert(errno == ENOMEM);
			abort();
		}

		/* Highlight column header title. */
		err = scols_cell_set_color(scols_column_get_header(col),
		                           "bold");
		cli_assert(!err);
	}

	table->scols = tbl;
	table->desc = desc;

	return 0;









	scols_table_enable_maxout()
	scols_table_enable_ascii()
}

void
cli_table_fini()
{

}
