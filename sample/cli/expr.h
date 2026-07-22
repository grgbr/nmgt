#ifndef _CLI_EXPR_H
#define _CLI_EXPR_H

#include "common.h"

struct cli_expr {
	struct cli_expr * next;
	unsigned int      cnt;
	unsigned int      nr;
	const char **     args;
};

#define cli_expr_assert(_expr) \
	cli_assert(_expr); \
	cli_assert((_expr)->nr); \
	cli_assert((_expr)->nr <= CLI_EXPR_ARGS_MAX); \
	cli_assert((_expr)->cnt <= (_expr)->nr); \
	cli_assert((_expr)->args)

static inline unsigned int
cli_expr_arg_cnt(const struct cli_expr * expression)
{
	cli_expr_assert(expression);
	cli_assert(expression->cnt);

	return expression->cnt;
}

static inline const char * const *
cli_expr_arg_cnt(const struct cli_expr * expression)
{
	cli_expr_assert(expression);
	cli_assert(expression->cnt);

	return expression->args;
}

/******************************************************************************
 * Command line expression block / sequence handling.
 ******************************************************************************/

struct cli_expr_blk {
	unsigned int       cnt;
	struct cli_expr *  head;
	struct cli_expr ** tail;
	char *             line;
};

#define CLI_EXPR_BLK_MAX (CLI_LINE_MAX / 2U)

#define cli_expr_blk_assert(_blk) \
	cli_assert(_blk); \
	cli_assert((_blk)->cnt <= CLI_EXPR_BLK_MAX); \
	cli_assert((_blk)->head || ((_blk)->tail == &(_blk)->head)); \
	cli_assert(!(_blk)->line || (*(_blk)->line != '\0')); \
	cli_assert(!(_blk)->line || \
	           (strnlen((_blk)->line, CLI_LINE_MAX) < CLI_LINE_MAX))

#define CLI_EXPR_BLK_INIT(_blk) \
	{ \
		.cnt  = 0, \
		.head = NULL, \
		.tail = &(_blk)->head, \
		.line = NULL \
	}

#define cli_expr_blk_foreach(_blk, _expr) \
	for (_expr = (_blk)->head; _expr; _expr = (_expr)->next)

#define cli_expr_blk_foreach_safe(_blk, _expr, _tmp) \
	for (_expr = (_blk)->head; \
	     (_expr) && (_tmp = (_expr)->next, 1); \
	     _expr = (_tmp))

extern int
cli_expr_blk_parse_line(struct cli_expr_blk * block, char * line);

extern void
cli_expr_blk_init(struct cli_expr_blk * block);

extern void
cli_expr_blk_fini(struct cli_expr_blk * block);

#endif /* _CLI_EXPR_H */
