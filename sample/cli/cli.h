#ifndef _CLI_H
#define _CLI_H

#include "shell.h"
#include "dir.h"
#include "work.h"
#include <stdbool.h>
#include <sysrepo.h>

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
	bool                   interact;
	struct cli_shell       shell;
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
