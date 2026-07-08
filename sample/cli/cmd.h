#ifndef _CLI_CMD_H
#define _CLI_CMD_H

#include "common.h"

struct cli_cmd;
struct cli_dir;

typedef int cli_cmd_parse_fn(const struct cli_cmd *,
                             const struct cli_dir *,
                             int,
                             const char * const [],
                             void *);

typedef void cli_cmd_fini_fn(struct cli_cmd *);

struct cli_cmd_ops {
	cli_cmd_parse_fn * parse;
	cli_cmd_fini_fn *  fini;
};

#define cli_cmd_assert_ops(_ops) \
	cli_assert(_ops); \
	cli_assert((_ops)->parse); \
	cli_assert((_ops)->fini)

struct cli_cmd {
	struct cli_cmd *           next;
	const struct cli_cmd_ops * ops;
};

#define cli_cmd_assert(_cmd) \
	cli_assert(_cmd); \
	cli_cmd_assert_ops((_cmd)->ops)

#define CLI_CMD_INIT(_opers) \
	{ .next = NULL, .ops = _opers }

#define cli_cmd_foreach(_first, _cmd) \
	for (_cmd = _first; _cmd; _cmd = (_cmd)->next)

#define cli_cmd_foreach_safe(_first, _cmd, _tmp) \
	for (_cmd = _first; \
	     (_cmd) && (_tmp = (_cmd)->next, 1); \
	     _cmd = _tmp)

static inline int
cli_cmd_parse(const struct cli_cmd * command,
              const struct cli_dir * dir,
              int                    argc,
              const char * const     argv[],
              void *                 data)
{
	cli_cmd_assert(command);
	cli_assert(dir);
	cli_assert_args(argc, argv);

	return command->ops->parse(command, dir, argc, argv, data);
}

static inline void
cli_cmd_init(struct cli_cmd * command, const struct cli_cmd_ops * opers)
{
	cli_assert(command);
	cli_cmd_assert_ops(opers);

	command->next = NULL;
	command->ops = opers;
}

extern void cli_cmd_null_fini(struct cli_cmd * command);

static inline void
cli_cmd_fini(struct cli_cmd * command)
{
	cli_cmd_assert(command);

	command->ops->fini(command);
}

extern struct cli_cmd *
cli_cmd_build(size_t size, const struct cli_cmd_ops * opers);

static inline struct cli_cmd *
cli_cmd_create(const struct cli_cmd_ops * opers)
{
	return cli_cmd_build(sizeof(struct cli_cmd), opers);
}

static inline void
cli_cmd_destroy(struct cli_cmd * command)
{
	cli_cmd_fini(command);
	cli_free(command);
}

#endif /* _CLI_CMD_H */
