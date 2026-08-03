#ifndef _CLI_H
#define _CLI_H

#include "shell.h"
#include "dir.h"
#include "work.h"
#include <stdbool.h>
#include <sysrepo.h>

/******************************************************************************
 * CLI style handling.
 ******************************************************************************/

#define CLI_RESET_COLOR        "\033[0m"
#define CLI_BOLD_COLOR         "\033[1m"
#define CLI_HALFBRIGHT_COLOR   "\033[2m"
#define CLI_UNDERLINE_COLOR    "\033[4m"
#define CLI_BLINK_COLOR        "\033[5m"
#define CLI_REVERSE_COLOR      "\033[7m"

/* Standard colors */
#define CLI_BLACK_COLOR        "\033[30m"
#define CLI_RED_COLOR          "\033[31m"
#define CLI_GREEN_COLOR        "\033[32m"
#define CLI_BROWN_COLOR        "\033[33m"
#define CLI_BLUE_COLOR         "\033[34m"
#define CLI_MAGENTA_COLOR      "\033[35m"
#define CLI_CYAN_COLOR         "\033[36m"
#define CLI_GRAY_COLOR         "\033[37m"

/* Bold variants */
#define CLI_DARK_GRAY_COLOR    "\033[1;30m"
#define CLI_BOLD_RED_COLOR     "\033[1;31m"
#define CLI_BOLD_GREEN_COLOR   "\033[1;32m"
#define CLI_BOLD_YELLOW_COLOR  "\033[1;33m"
#define CLI_BOLD_BLUE_COLOR    "\033[1;34m"
#define CLI_BOLD_MAGENTA_COLOR "\033[1;35m"
#define CLI_BOLD_CYAN_COLOR    "\033[1;36m"

#define CLI_WHITE_COLOR        "\033[1;37m"

enum cli_style_kind {
	CLI_LABEL_STYLE_KIND = 0,
	CLI_VALUE_STYLE_KIND,
	CLI_DEFAULT_STYLE_KIND,
	CLI_ERROR_STYLE_KIND,
	CLI_STYLE_KIND_NR
};

static inline const char *
cli_style_get_color(const char * const * style, enum cli_style_kind kind)
{
	cli_assert(kind >= 0);
	cli_assert(kind < CLI_STYLE_KIND_NR);

	if (style) {
		cli_assert(!style[kind] || style[kind][0] != '\0');
		return style[kind];
	}
	else
		return NULL;
}

/******************************************************************************
 * Global CLI context
 ******************************************************************************/

#define CLI_WORK_NR (128U)
#if CLI_WORK_NR < CLI_EXPR_BLK_MAX
#error The number of differed work MUST be large enough to support, at least, \
       a full block of expressions !
#endif /* CLI_WORK_NR < CLI_EXPR_BLK_MAX */

struct cli_context {
	sr_conn_ctx_t *        conn;
	sr_session_ctx_t *     sess;
	const struct ly_ctx *  lyctx;
	unsigned int           wkcnt;
	struct cli_work *      wkq[CLI_WORK_NR];
	struct cli_dir         root;
	const struct cli_dir * cwd;
	struct ly_out *        lyout;
	bool                   isatty;
	bool                   colored;
	const char * const *   style;
	bool                   interact;
};

#define cli_assert_context(_ctx) \
	cli_assert(_ctx); \
	cli_assert((_ctx)->conn); \
	cli_assert((_ctx)->sess); \
	cli_assert((_ctx)->lyctx); \
	cli_dir_assert(&(_ctx)->root); \
	cli_dir_assert((_ctx)->cwd); \
	cli_assert((_ctx)->lyout)

extern unsigned int
cli_term_cols(const struct cli_context * context);

static inline bool
cli_isatty(const struct cli_context * context)
{
	cli_assert_context(context);

	return context->isatty;
}

static inline bool
cli_has_colors(const struct cli_context * context)
{
	cli_assert_context(context);

	return context->colored;
}

static inline const char * const *
cli_get_style(const struct cli_context * context)
{
	cli_assert_context(context);

	return context->style;
}

static inline bool
cli_isinteractive(const struct cli_context * context)
{
	cli_assert_context(context);

	return context->interact;
}

static inline const struct cli_dir *
cli_cwd(const struct cli_context * context)
{
	cli_assert_context(context);

	return context->cwd;
}

extern void
cli_chdir(struct cli_context * context, const struct cli_dir * directory);

static inline int
cli_sched_work(struct cli_context * context, struct cli_work * work)
{
	cli_assert_context(context);
	cli_assert_work(work);

	if (context->wkcnt >= CLI_WORK_NR)
		return -EBUSY;

	context->wkq[context->wkcnt++] = work;

	return 0;
}

#endif
