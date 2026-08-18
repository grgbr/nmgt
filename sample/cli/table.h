#ifndef _CLI_TABLE_H
#define _CLI_TABLE_H

#include "common.h"
#include <stdbool.h>
#include <libsmartcols/libsmartcols.h>

#define CLI_TABLE_COLUMN_MAX       (32U)
#if (CLI_TABLE_COLUMN_MAX < 2) || (CLI_TABLE_COLUMN_MAX > UINT_MAX)
#error Invalid maximum number of table columns !
#endif
#define CLI_TABLE_LABEL_MAX        (64U)
#define CLI_TABLE_COLUMN_WIDTH_MAX (128.)

struct cli_table_column_desc {
	const char * label;
	double       whint;
	int          flags;
};

/*
 * TODO: Add static assert support to make sure that the following is true.
 * - 0 < strnlen(_label, CLI_TABLE_LABEL_MAX) < CLI_TABLE_LABEL_MAX
 * - 0. < whint <= CLI_TABLE_COLUMN_WIDTH_MAX
 * - !((_flags) & \
 *     (SCOLS_FL_TRUNC | \
 *      SCOLS_FL_TREE | \
 *      SCOLS_FL_RIGHT | \
 *      SCOLS_FL_STRICTWIDTH | \
 *      SCOLS_FL_NOEXTREMES | \
 *      SCOLS_FL_HIDDEN | \
 *      SCOLS_FL_WRAP))
 */
#define CLI_TABLE_SETUP_COLUMN_DESC(_label, _whint, _flags) \
	{ \
		.label = _label, \
		.whint = _whint, \
		.flags = _flags \
	}

struct cli_table_desc {
	bool                                 nohead;
	unsigned int                         nr;
	const struct cli_table_column_desc * cols;
};

/*
 * TODO: Add static assert support to make sure that the following is true.
 * - 0 < cli_array_nr(_cols) < CLI_TABLE_COLUMN_MAX
 */
#define CLI_TABLE_SETUP_DESC(_nohead, _cols) \
	{ \
		.nohead = _nohead, \
		.cnt    = cli_array_nr(_cols), \
		.nr     = cli_array_nr(_cols), \
		.cols   = _cols \
	}

struct cli_table;
struct cli_context;

typedef int (cli_table_load_fn)(struct cli_table *,
                                const struct cli_context *,
                                void *);

typedef void (cli_table_fini_fn)(struct cli_table *);

struct cli_table_ops {
	cli_table_load_fn * load;
	cli_table_fini_fn * fini;
};

#define cli_table_assert_ops(_ops) \
	cli_assert((_ops)->load); \
	cli_assert((_ops)->fini)

struct cli_table {
	const struct cli_table_ops * ops;
	struct libscols_table *      scols;
};

#define cli_table_assert(_table) \
	cli_assert(_table); \
	cli_table_assert_ops((_table)->ops); \
	cli_assert((_table)->scols)

extern int
cli_table_show(const struct cli_table * table, bool maxout, FILE * stdio);

static inline int
cli_table_load(struct cli_table *         table,
               const struct cli_context * context,
               void *                     data)
{
	cli_table_assert(table);
	cli_assert(context);

	return table->ops->load(table, context, data);
}

extern void
cli_table_cell_set_data(struct libscols_cell * cell, const char * data);

extern void
cli_table_cell_set_color(struct libscols_cell * cell, const char * color);

static inline struct libscols_cell *
cli_table_line_get_cell(struct libscols_line * line, unsigned int cell_index)
{
	cli_assert(line);
	cli_assert((size_t)cell_index < scols_line_get_ncells(line));

	struct libscols_cell * cell;

	cell = scols_line_get_cell(line, cell_index);
	cli_assert(cell);

	return cell;
}

static inline void *
cli_table_line_get_userdata(struct libscols_line * line)
{
	cli_assert(line);

	return scols_line_get_userdata(line);
}

static inline void
cli_table_line_set_userdata(struct libscols_line * line, void * data)
{
	cli_assert(line);

	int err;

	err = scols_line_set_userdata(line, data);
	cli_assert(!err);
}

extern void
cli_table_line_set_color(struct libscols_line * line, const char * color);

extern void *
cli_table_col_get_userdata(struct libscols_column * column);

extern void
cli_table_col_set_userdata(struct libscols_column * column, void * data);

extern void
cli_table_col_set_color(struct libscols_column * column, const char * color);

static inline unsigned int
cli_table_get_line_count(const struct cli_table * table)
{
	cli_table_assert(table);

	size_t cnt;

	cnt = scols_table_get_nlines(table->scols);
	cli_assert(cnt <= UINT_MAX);

	return (unsigned int)cnt;
}

static inline struct libscols_line *
cli_table_get_line(const struct cli_table * table, unsigned int line_index)
{
	cli_table_assert(table);
	cli_assert(line_index < scols_table_get_nlines(table->scols));

	struct libscols_line * ln;

	ln = scols_table_get_line(table->scols, line_index);
	cli_assert(ln);

	return ln;
}

#define cli_table_foreach_line(_table, _indx, _line) \
	for (_indx = 0; \
	     ((_indx) < cli_table_get_line_count(_table)) && \
	     (_line = cli_table_get_line(_table, _indx)); \
	     (_indx)++)

extern struct libscols_line *
cli_table_new_line(const struct cli_table * table);

static inline void
cli_table_remove_lines(const struct cli_table * table)
{
	cli_table_assert(table);

	scols_table_remove_lines(table->scols);
}

static inline unsigned int
cli_table_get_col_count(const struct cli_table * table)
{
	cli_table_assert(table);

	size_t cnt;

	cnt = scols_table_get_ncols(table->scols);
	cli_assert(cnt <= CLI_TABLE_COLUMN_MAX);

	return (unsigned int)cnt;
}

static inline struct libscols_column *
cli_table_get_col(const struct cli_table * table, unsigned int column_index)
{
	cli_table_assert(table);
	cli_assert(column_index < scols_table_get_ncols(table->scols));

	struct libscols_column * col;

	col = scols_table_get_column(table->scols, column_index);
	cli_assert(col);

	return col;
}

#define cli_table_foreach_col(_table, _indx, _col) \
	for (_indx = 0; \
	     ((_indx) < cli_table_get_col_count(_table)) && \
	     (_col = cli_table_get_col(_table, _indx)); \
	     (_indx)++)

extern struct libscols_column *
cli_table_new_col(const struct cli_table * table,
                  const char *             label,
                  double                   whint,
                  int                      flags);

extern void *
cli_table_get_userdata(struct cli_table * table);

extern void
cli_table_set_userdata(struct cli_table * table, void * data);

extern void
cli_table_init(struct cli_table *           table,
               const struct cli_context *   context,
               bool                         nohead,
               const struct cli_table_ops * opers);

extern void
cli_table_init_from_desc(struct cli_table *            table,
                         const struct cli_context *    context,
                         const struct cli_table_desc * descriptor,
                         const struct cli_table_ops *  opers);

static inline void
cli_table_fini(struct cli_table * table)
{
	cli_table_assert(table);

	table->ops->fini(table);
	scols_unref_table(table->scols);
}

#endif /* _CLI_TABLE_H */
