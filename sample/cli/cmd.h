#ifndef _CLI_CMD_H
#define _CLI_CMD_H

#include "common.h"

struct cli_cmd;

typedef int cli_cmd_parse_fn(const struct cli_cmd *,
                             const struct cli_menu *,
                             int,
                             const char * const [],
                             struct cli_context *)

typedef void cli_cmd_release_fn(struct cli_cmd *, struct cli_context *);

struct cli_cmd_ops {
	cli_cmd_parse_fn * parse;
	cli_cmd_fini_fn *  fini;
};

#define cli_cmd_assert_ops(_ops) \
	cli_assert(_ops); \
	cli_assert((_ops)->parse); \
	cli_assert((_ops)->fini)

struct cli_cmd {
	const struct cli_cmd_ops * ops;
	struct cli_cmd *           next;
};

#define cli_cmd_assert(_cmd) \
	cli_assert(_cmd); \
	cli_cmd_assert_ops((_cmd)->ops)

#define CLI_CMD_INIT(_ops) \
	{ .ops = _ops, .next = NULL }

#define cli_cmd_foreach(_first, _cmd) \
	for (_cmd = _first; _cmd; _cmd = (_cmd)->next)

#define cli_cmd_foreach_safe(_first, _cmd, _tmp) \
	for (_cmd = _first; \
	     (_cmd) && (_tmp = (_cmd)->next); \
	     _cmd = _tmp, _tmp = (_cmd)->next)

static inline int
cli_cmd_parse(const struct cli_cmd *  command,
              const struct cli_menu * menu,
              int                     argc,
              const char * const      argv[],
              struct cli_context *    context,
{
	cli_cmd_assert(cmd);
	cli_menu_assert(menu);
	cli_assert_args(argc, argv);
	cli_assert_context(context);

	return command->ops->parse(command, menu, argc, argv, context);
}

static inline void
cli_cmd_init(struct cli_cmd *           command,
             const struct cli_cmd_ops * opers,
             struct cli_context *       context)
{
	cli_assert(command);
	cli_cmd_assert_ops(opers);
	cli_assert_context(context);

	command->ops = ops;
	command->next = NULL;
}

static inline void
cli_cmd_fini(struct cli_cmd * command, struct cli_context * context)
{
	cli_cmd_assert(cmd);
	cli_assert_context(context);

	command->ops->fini(command, context);
}

#endif /* _CLI_CMD_H */
