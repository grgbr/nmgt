#ifndef _CLI_COMMON_H
#define _CLI_COMMON_H

#define _GNU_SOURCE

#include <sysrepo.h>
#include <stdbool.h>

#define CONFIG_CLI_ASSERT 1
#define CONFIG_CLI_LOG 1
#define CONFIG_CLI_LOG_LEVEL 5

/******************************************************************************
 * Utilities
 ******************************************************************************/

#if defined(CONFIG_CLI_ASSERT)
#include <assert.h>

#define cli_assert(...) assert(__VA_ARGS__)

#else  /* !defined(CONFIG_CLI_ASSERT) */

#define cli_assert(...)

#endif /* defined(CONFIG_CLI_ASSERT) */

#define cli_log(_format, ...) \
	fprintf(stderr, \
	        "%s: " _format "\n", \
	        program_invocation_short_name, \
	        ## __VA_ARGS__)

extern void *
cli_malloc(size_t size);

static inline void
cli_free(void * data)
{
	free(data);
}

#define CLI_WALK_CONT_RET (0)
#define CLI_WALK_SKIP_RET (1)

enum cli_walk_event {
	CLI_WALK_PRE_EVT,
	CLI_WALK_POST_EVT,
	CLI_WALK_EVT_NR
};

/******************************************************************************
 * Work handling
 ******************************************************************************/

struct cli_work;
struct cli_context;

typedef int cli_work_exec_fn(const struct cli_work *, struct cli_context *);

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
cli_release_work(struct cli_work * work, struct cli_context * context)
{
	cli_assert_work(work);
	cli_assert(context);

	work->ops->release(work, context);
}

static inline void
cli_destroy_work(struct cli_work * work)
{
	cli_assert_work(work);

	cli_free(work);
}

/******************************************************************************
 * Node handling
 ******************************************************************************/

struct cli_node;

typedef int cli_node_parse_fn(const struct cli_node *,
                              int,
                              const char * const [],
                              struct cli_context *);

typedef void cli_node_release_fn(struct cli_node *, struct cli_context *);

struct cli_node_ops {
	cli_node_parse_fn *   parse;
	cli_node_release_fn * release;
};

struct cli_node {
	const struct cli_node_ops * ops;
	struct cli_node *           next;
	struct cli_node *           prev;
	struct cli_node *           child;
};

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
	struct cli_node       root;
	struct ly_out *       lyout;
	sr_data_t *           select;
	bool                  isatty;
};

#define cli_assert_context(_ctx) \
	cli_assert(_ctx); \
	cli_assert((_ctx)->conn); \
	cli_assert((_ctx)->sess); \
	cli_assert((_ctx)->lyctx); \
	cli_assert((_ctx)->lyout)

extern unsigned int
cli_term_cols(const struct cli_context * context);

#endif /* _CLI_COMMON_H */
