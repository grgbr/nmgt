#ifndef _CLI_CMD_H
#define _CLI_CMD_H

#include "arg.h"

struct cli_cmd;
struct cli_dir;
struct cli_context;
struct cli_match;

typedef int cli_cmd_parse_fn(const struct cli_cmd *,
                             const struct cli_dir *,
                             struct cli_context *,
                             int,
                             const char * const []);

typedef void cli_cmd_complete_fn(const struct cli_cmd *,
                                 const struct cli_dir *,
                                 struct cli_context *,
                                 const char *,
                                 size_t,
                                 int,
                                 const char * const [],
                                 struct cli_match *);

typedef void cli_cmd_fini_fn(struct cli_cmd *);

struct cli_cmd_ops {
	cli_cmd_parse_fn *    parse;
	cli_cmd_complete_fn * complete;
	cli_cmd_fini_fn *     fini;
};

#define cli_cmd_assert_ops(_ops) \
	cli_assert(_ops); \
	cli_assert((_ops)->parse); \
	cli_assert((_ops)->complete); \
	cli_assert((_ops)->fini)

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

extern void
cli_cmd_complete_args(const struct cli_cmd * command,
                      const struct cli_dir * directory,
                      struct cli_context *   context,
                      const char *           word,
                      size_t                 length,
                      int                    argc,
                      const char * const     argv[],
                      struct cli_match *     matches);

extern void
cli_cmd_complete(const struct cli_cmd * command,
                 const struct cli_dir * directory,
                 struct cli_context *   context,
                 const char *           word,
                 size_t                 length,
                 int                    argc,
                 const char * const     argv[],
                 struct cli_match *     matches);

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

static inline void
cli_cmd_null_fini(struct cli_cmd * command __cli_unused)
{
	cli_cmd_assert(command);
}

extern int
cli_cmd_sized_create(struct cli_cmd **          command,
                     size_t                     size,
                     const char *               name,
                     const struct cli_cmd_ops * opers);

static inline int
cli_cmd_create(struct cli_cmd **          command,
               const char *               name,
               const struct cli_cmd_ops * opers)
{
	cli_assert(command);
	cli_assert(name);
	cli_cmd_assert_ops(opers);

	return cli_cmd_sized_create(command, sizeof(**command), name, opers);
}

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
