#ifndef _CLI_H
#define _CLI_H

#include "dir.h"
#include <stdbool.h>
#include <sysrepo.h>

/******************************************************************************
 * Work handling
 ******************************************************************************/

struct cli_work;
struct cli_context;

typedef int cli_work_exec_fn(struct cli_work *, struct cli_context *);

typedef void cli_work_release_fn(struct cli_work *, struct cli_context *);

struct cli_work_ops {
	cli_work_exec_fn *    exec;
	cli_work_release_fn * release;
};

#define cli_assert_work_ops(_ops) \
	cli_assert(_ops); \
	cli_assert((_ops)->exec); \
	cli_assert((_ops)->release)

struct cli_work {
	const struct cli_work_ops * ops;
};

#define cli_assert_work(_work) \
	cli_assert(_work); \
	cli_assert_work_ops((_work)->ops)

static inline struct cli_work *
cli_create_work(size_t size, const struct cli_work_ops * ops)
{
	cli_assert(size >= sizeof(struct cli_work));
	cli_assert_work_ops(ops);

	struct cli_work * wk;

	wk = cli_malloc(size);
	wk->ops = ops;

	return wk;
}

static inline void
cli_null_release_work(struct cli_work *    work __cli_unused,
                      struct cli_context * context __cli_unused)
{
}

static inline void
cli_destroy_work(struct cli_work * work, struct cli_context * context)
{
	cli_assert_work(work);

	work->ops->release(work, context);
	cli_free(work);
}

/******************************************************************************
 * Global CLI context
 ******************************************************************************/

#define CLI_WORK_NR (128U)

struct cli_context {
	sr_conn_ctx_t *       conn;
	sr_session_ctx_t *    sess;
	const struct ly_ctx * lyctx;
	unsigned int          wkcnt;
	struct cli_work *     wkq[CLI_WORK_NR];
	struct cli_dir        root;
	struct cli_dir *      cwd;
	struct ly_out *       lyout;
	bool                  isatty;
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

static inline struct cli_dir *
cli_cwd(const struct cli_context * context)
{
	cli_assert_context(context);

	return context->cwd;
}

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
