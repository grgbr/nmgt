#ifndef _CLI_WORK_H
#define _CLI_WORK_H

#include "common.h"

struct cli_work;
struct cli_context;

typedef int cli_work_exec_fn(struct cli_work *, struct cli_context *);

typedef void cli_work_release_fn(struct cli_work *);

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
cli_null_release_work(struct cli_work * work __cli_unused)
{
}

static inline void
cli_destroy_work(struct cli_work * work)
{
	cli_assert_work(work);

	work->ops->release(work);
	cli_free(work);
}

#endif /* _CLI_WORK_H */
