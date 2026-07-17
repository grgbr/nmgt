#ifndef _CLI_ARG_H
#define _CLI_ARG_H

#include "node.h"
#include <string.h>

#define CLI_ARG_MAX (128U)

struct cli_arg;
struct cli_cmd;
struct cli_dir;
struct cli_context;

typedef int cli_arg_parse_fn(const struct cli_arg *,
                             const struct cli_cmd *,
                             const struct cli_dir *,
                             const struct cli_context *,
                             int,
                             const char * const [],
                             void *);

struct cli_arg_ops {
	cli_arg_parse_fn * parse;
};

#define cli_arg_assert_ops(_opers) \
	cli_assert(_opers); \
	cli_assert((_opers)->parse)

struct cli_arg {
	struct cli_node            super;
	const struct cli_arg_ops * ops;
};

#define cli_arg_assert(_arg) \
	cli_assert(_arg); \
	cli_arg_assert_ops((_arg)->ops)

static inline int
cli_arg_parse(const struct cli_arg *     argument,
              const struct cli_cmd *     command,
              const struct cli_dir *     directory,
              const struct cli_context * context,
              int                        argc,
              const char * const         argv[],
              void *                     data)
{
	cli_arg_assert(argument);
	cli_assert(command);
	cli_assert(directory);
	cli_assert(context);
	cli_assert(argc > 0);
	cli_assert(argv);
	cli_assert(argv[0]);

	return argument->ops->parse(argument,
	                            command,
	                            directory,
	                            context,
	                            argc,
	                            argv,
	                            data);
}

static inline void
cli_arg_add_child(struct cli_arg * argument, struct cli_arg * child)
{
	cli_arg_assert(argument);
	cli_arg_assert(child);

	cli_node_add_child(&argument->super, &child->super);
}

extern struct cli_arg *
cli_arg_create(size_t size, const struct cli_arg_ops * opers);

static inline void
cli_arg_destroy(struct cli_arg * argument)
{
	cli_arg_assert(argument);

	cli_free(argument);
}

struct cli_arg_key {
	struct cli_arg super;
	const char *   name;
	size_t         len;
};

#define cli_arg_assert_key(_arg) \
	cli_assert(_arg); \
	cli_arg_assert(&(_arg)->super); \
	cli_assert((_arg)->name); \
	cli_assert((_arg)->len); \
	cli_assert((_arg)->len < CLI_ARG_MAX); \
	cli_assert(strnlen((_arg)->name, CLI_ARG_MAX) == (_arg)->len)

extern int
cli_arg_create_key(struct cli_arg_key **      key,
                   const char *               name,
                   const struct cli_arg_ops * opers);

static inline void
cli_arg_destroy_key(struct cli_arg_key * key)
{
	cli_arg_assert_key(key);

	cli_arg_destroy(&key->super);
}

extern struct cli_arg *
cli_arg_create_choice(void);

extern int
cli_arg_create_keyopt(struct cli_arg_key ** keyopt, const char * name);

extern int
cli_arg_parse_term(const struct cli_arg * terminal,
                   int                    argc,
                   const char * const     argv[]);

#endif /* _CLI_ARG_H */
