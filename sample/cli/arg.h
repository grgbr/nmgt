#ifndef _CLI_ARG_H
#define _CLI_ARG_H

#include "node.h"
#include <string.h>

#define CLI_ARG_MAX (128U)

struct cli_arg;
struct cli_cmd;
struct cli_dir;
struct cli_context;
struct cli_match;

/******************************************************************************
 * Argument utilities
 ******************************************************************************/

extern size_t
_cli_arg_isstr_valid(const char * string, size_t length);

/******************************************************************************
 * Base argument handling
 ******************************************************************************/

typedef int cli_arg_parse_fn(const struct cli_arg *,
                             const struct cli_cmd *,
                             const struct cli_dir *,
                             struct cli_context *,
                             int,
                             const char * const [],
                             void *);

typedef void cli_arg_complete_fn(const struct cli_arg *,
                                 const struct cli_cmd *,
                                 const struct cli_dir *,
                                 struct cli_context *,
                                 const char *,
                                 size_t,
                                 int,
                                 const char * const [],
                                 struct cli_match *);

struct cli_arg_ops {
	cli_arg_parse_fn *    parse;
	cli_arg_complete_fn * complete;
};

#define cli_arg_assert_ops(_opers) \
	cli_assert(_opers); \
	cli_assert((_opers)->parse); \
	cli_assert((_opers)->complete)

struct cli_arg {
	struct cli_node            super;
	const struct cli_arg_ops * ops;
};

#define cli_arg_assert(_arg) \
	cli_assert(_arg); \
	cli_arg_assert_ops((_arg)->ops)

static inline int
cli_arg_parse(const struct cli_arg * argument,
              const struct cli_cmd * command,
              const struct cli_dir * directory,
              struct cli_context *   context,
              int                    argc,
              const char * const     argv[],
              void *                 data)
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
cli_arg_complete(const struct cli_arg * argument,
                 const struct cli_cmd * command,
                 const struct cli_dir * directory,
                 struct cli_context *   context,
                 const char *           word,
                 size_t                 length,
                 int                    argc,
                 const char * const     argv[],
                 struct cli_match *     matches)
{
	cli_arg_assert(argument);
	cli_assert(command);
	cli_assert(directory);
	cli_assert(context);
	cli_assert(word);
	cli_assert(length < CLI_ARG_MAX);
	cli_assert(strnlen(word, CLI_ARG_MAX) == length);
	cli_assert(argc >= 0);
	cli_assert(!argc || argv);
	cli_assert(matches);

	return argument->ops->complete(argument,
	                               command,
	                               directory,
	                               context,
	                               word,
	                               length,
	                               argc,
	                               argv,
	                               matches);
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

/******************************************************************************
 * Parameter argument handling
 ******************************************************************************/

struct cli_arg_parm {
	struct cli_arg super;
	const char *   name;
	size_t         len;
};

extern int
cli_arg_create_parm(struct cli_arg_parm **     parameter,
                    const char *               name,
                    const struct cli_arg_ops * opers);

extern int
cli_arg_createn_add_parm(struct cli_arg_parm **     parameter,
                         const char *               name,
                         const struct cli_arg_ops * opers,
                         struct cli_node *          cmd_or_arg);

/******************************************************************************
 * Choice argument handling
 ******************************************************************************/

extern struct cli_arg *
cli_arg_create_choice(void);

extern struct cli_arg *
cli_arg_createn_add_choice(struct cli_node * cmd_or_arg);

/******************************************************************************
 * Keyword parameter argument handling
 ******************************************************************************/

struct cli_arg_kword_term {
	const char *       value;
	size_t             len;
	cli_arg_parse_fn * on_match;
};

#warning Use static_assert()
#define CLI_ARG_KWORD_TERM(_value, _on_match) \
	{ \
		.value    = _value, \
		.len      = sizeof(_value) - 1, \
		.on_match = _on_match \
	}

struct cli_arg_kword_parm {
	struct cli_arg_parm               super;
	unsigned int                      nr;
	const struct cli_arg_kword_term * terms;
};

extern int
cli_arg_create_kword_parm(struct cli_arg_kword_parm **      parameter,
                          const char *                      name,
                          const struct cli_arg_kword_term * terminals,
                          unsigned int                      nr);

extern int
cli_arg_createn_add_kword_parm(struct cli_arg_kword_parm **      parameter,
                               const char *                      name,
                               const struct cli_arg_kword_term * terminals,
                               unsigned int                      nr,
                               struct cli_node *                 cmd_or_arg);

#endif /* _CLI_ARG_H */
