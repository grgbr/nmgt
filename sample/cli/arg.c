#include "arg.h"
#include "cmd.h"
#include <string.h>

#define cli_arg_foreach_child(_arg, _child) \
	for (_child = (const struct cli_arg *)((_arg)->super.child); \
	     _child; \
	     _child = (const struct cli_arg *)((_child)->super.next))

static void
cli_arg_setup(struct cli_arg * argument, const struct cli_arg_ops * opers)
{
	cli_assert(argument);
	cli_arg_assert_ops(opers);

	cli_node_setup(&argument->super);
	argument->ops = opers;
}

struct cli_arg *
cli_arg_create(size_t size, const struct cli_arg_ops * opers)
{
	cli_assert(size >= sizeof(struct cli_arg));
	cli_arg_assert_ops(opers);

	struct cli_arg * arg;

	arg = cli_malloc(size);
	cli_assert(arg);

	cli_arg_setup(arg, opers);

	return arg;
}

static int
cli_arg_setup_key(struct cli_arg_key *       key,
                  const char *               name,
                  const struct cli_arg_ops * opers)
{
	cli_assert(key);
	cli_assert(name);
	cli_arg_assert_ops(opers);

	size_t len;

	len = strnlen(name, CLI_ARG_MAX);
	if (!len)
		return -ENODATA;
	if (len == CLI_ARG_MAX)
		return -ENAMETOOLONG;

	cli_arg_setup(&key->super, opers);
	key->name = name;
	key->len = len;

	return 0;
}

int
cli_arg_create_key(struct cli_arg_key **      key,
                   const char *               name,
                   const struct cli_arg_ops * opers)
{
	cli_assert(key);
	cli_assert(name);
	cli_arg_assert_ops(opers);

	struct cli_arg_key * k;
	int                  ret;

	k = cli_malloc(sizeof(*k));
	cli_assert(k);

	ret = cli_arg_setup_key(k, name, opers);
	if (!ret) {
		*key = k;
		return 0;
	}

	cli_free(k);

	return ret;
}

int
cli_arg_parse_term(const struct cli_arg * terminal,
                   int                    argc,
                   const char * const     argv[])
{
	cli_arg_assert_key((const struct cli_arg_key *)terminal);
	cli_assert(argc == 1);
	cli_assert(argv[0]);
	cli_assert(strnlen(argv[0], CLI_ARG_MAX) < CLI_ARG_MAX);

	const struct cli_arg_key * term = (const struct cli_arg_key *)terminal;
	const char *               str = argv[0];
	size_t                     len;

	len = strnlen(str, term->len + 1);
	if ((len != term->len) || memcmp(str, term->name, term->len))
		return 0;

	return 1;
}

int
cli_arg_parse_keyopt(const struct cli_arg *     keyopt,
                     const struct cli_cmd *     command,
                     const struct cli_dir *     directory,
                     const struct cli_context * context,
                     int                        argc,
                     const char * const         argv[],
                     void *                     data)
{
	cli_arg_assert_key((const struct cli_arg_key *)keyopt);
	cli_cmd_assert(command);
	cli_assert(argc == 1);
	cli_assert(argv[0]);
	cli_assert(strnlen(argv[0], CLI_ARG_MAX) < CLI_ARG_MAX);

	const struct cli_arg_key * key = (const struct cli_arg_key *)keyopt;
	const char *               str = argv[0];
	size_t                     len;
	const struct cli_node *    val;

	len = strcspn(str, "=");
	if ((len != key->len) ||
	    (str[len] != '=') ||
	    memcmp(str, key->name, key->len))
		return 0;

	str = &str[len + 1];
	if (*str == '\0') {
		cli_cmd_log(command,
		            "'%s': missing option value.",
		            key->name);
		return -EINVAL;
	}

	cli_node_foreach_child(&keyopt->super, val) {
		int ret;

		ret = cli_arg_parse((const struct cli_arg *)val,
		                    command,
		                    directory,
		                    context,
		                    1,
		                    &str,
		                    data);
		cli_assert(ret <= 1);
		if (ret)
			return ret;
	}

	cli_cmd_log(command, "'%s': invalid %s option value.",
	            str,
	            key->name);

	return -EINVAL;
}

int
cli_arg_parse_choice(const struct cli_arg *     choice,
                     const struct cli_cmd *     command,
                     const struct cli_dir *     directory,
                     const struct cli_context * context,
                     int                        argc,
                     const char * const         argv[],
                     void *                     data)
{
	cli_arg_assert(choice);
	cli_cmd_assert(command);
	cli_assert(argc == 1);
	cli_assert(argv[0]);
	cli_assert(strnlen(argv[0], CLI_ARG_MAX) < CLI_ARG_MAX);

	const struct cli_node * opt;

	cli_node_foreach_child(&choice->super, opt) {
		int ret;

		ret = cli_arg_parse((const struct cli_arg *)opt,
		                    command,
		                    directory,
		                    context,
		                    1,
		                    &argv[0],
		                    data);
		cli_assert(ret <= 1);
		if (ret)
			return ret;
	}

	cli_cmd_log(command, "'%s': invalid argument.", argv[0]);

	return -EINVAL;
}
