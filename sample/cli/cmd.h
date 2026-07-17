#ifndef _CLI_CMD_H
#define _CLI_CMD_H

#include "arg.h"

struct cli_cmd;
struct cli_dir;
struct cli_context;

typedef int cli_cmd_parse_fn(const struct cli_cmd *,
                             const struct cli_dir *,
                             struct cli_context *,
                             int,
                             const char * const []);

struct cli_cmd_ops {
	cli_cmd_parse_fn * parse;
};

#define cli_cmd_assert_ops(_ops) \
	cli_assert(_ops); \
	cli_assert((_ops)->parse)

struct cli_cmd {
	struct cli_node            super;
	const struct cli_cmd_ops * ops;
	const char *               name;
};

#define cli_cmd_assert(_cmd) \
	cli_assert(_cmd); \
	cli_node_assert(&(_cmd)->super); \
	cli_cmd_assert_ops((_cmd)->ops); \
	cli_assert((_cmd)->name); \
	cli_assert((_cmd)->name[0] != '\0'); \
	cli_assert(strnlen((_cmd)->name, CLI_ARG_MAX) < CLI_ARG_MAX)

#define cli_cmd_log(_cmd, _format, ...) \
	cli_log("%s: " _format, (_cmd)->name, ## __VA_ARGS__)

extern int
cli_cmd_parse_args(const struct cli_cmd * command,
                   const struct cli_dir * directory,
                   struct cli_context *   context,
                   int                    argc,
                   const char * const     argv[],
                   void *                 data);

extern int
cli_cmd_parse(const struct cli_cmd * command,
              const struct cli_dir * dir,
              struct cli_context *   context,
              int                    argc,
              const char * const     argv[]);

static inline void
cli_cmd_add_arg(struct cli_cmd * command, struct cli_arg * argument)
{
	cli_cmd_assert(command);
	cli_arg_assert(argument);

	cli_node_add_child(&command->super, &argument->super);
}

extern int
cli_cmd_init(struct cli_cmd *           command,
             const char *               name,
             const struct cli_cmd_ops * opers);

extern void
cli_cmd_fini(struct cli_cmd * command);

extern int
cli_cmd_create(struct cli_cmd **          command,
               const char *               name,
               const struct cli_cmd_ops * opers);

extern int
cli_cmd_createn_add(struct cli_cmd **          command,
                    const char *               name,
                    const struct cli_cmd_ops * opers,
                    struct cli_dir *           directory);

static inline void
cli_cmd_destroy(struct cli_cmd * command)
{
	cli_cmd_fini(command);
	cli_free(command);
}

#endif /* _CLI_CMD_H */
