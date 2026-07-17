#include "cmd.h"
#include "cli.h"

static bool
cli_cmd_has_args(const struct cli_cmd * command)
{
	cli_cmd_assert(command);

	return cli_node_has_child(&command->super);
}

int
cli_cmd_parse_args(const struct cli_cmd * command,
                   const struct cli_dir * directory,
                   struct cli_context *   context,
                   int                    argc,
                   const char * const     argv[],
                   void *                 data)
{
	cli_cmd_assert(command);
	cli_dir_assert(directory);
	cli_assert_context(context);
	cli_assert(argv);

	if (!argc)
		return 0;

	if (cli_cmd_has_args(command)) {
		int cnt = 0;

		do {
			struct cli_node * child;
			int               ret = 0;

			cli_node_foreach_child(&command->super, child) {
				const struct cli_arg * arg =
					(const struct cli_arg *)child;

				ret = cli_arg_parse(arg,
				                    command,
				                    directory,
				                    context,
				                    argc - cnt,
				                    &argv[cnt],
				                    data);
				if (ret)
					break;
			}

			cli_assert(ret <= (argc - cnt));
			if (!ret) {
				cli_cmd_log(command,
				            "'%s': invalid argument.",
				            argv[cnt]);
				return -EINVAL;
			}
			else if (ret < 0)
				return ret;

			cnt += ret;
		} while (cnt < argc);

		cli_assert(cnt == argc);

		return argc;
	}

	cli_cmd_log(command, "too many arguments.");

	return -EINVAL;
}

int
cli_cmd_parse(const struct cli_cmd * command,
              const struct cli_dir * directory,
              struct cli_context *   context,
              int                    argc,
              const char * const     argv[])
{
	cli_cmd_assert(command);
	cli_dir_assert(directory);
	cli_assert_context(context);
	cli_assert_args(argc, argv);

	int ret = 0;

	if (!strcmp(argv[0], command->name)) {
		int ret;

		ret = command->ops->parse(command,
		                          directory,
		                          context,
		                          argc - 1,
		                          &argv[1]);
		if (ret >= 0) {
			cli_assert((ret + 1) == argc);
			return argc;
		}
	}

	return ret;
}

int
cli_cmd_init(struct cli_cmd *           command,
             const char *               name,
             const struct cli_cmd_ops * opers)
{
	cli_assert(command);
	cli_assert(name);
	cli_cmd_assert_ops(opers);

	size_t len;

	len = strnlen(name, CLI_ARG_MAX);
	if (!len)
		return -ENODATA;
	if (len == CLI_ARG_MAX)
		return -ENAMETOOLONG;

	cli_node_setup(&command->super);
	command->ops = opers;
	command->name = name;

	return 0;
}

static int
cli_cmd_destroy_arg(struct cli_node *   node,
                    enum cli_walk_event event,
                    void *              data __cli_unused)
{
	cli_arg_assert((struct cli_arg *)node);

	switch (event) {
	case CLI_WALK_PRE_EVT:
		break;

	case CLI_WALK_POST_EVT:
		cli_arg_destroy((struct cli_arg *)node);
		break;

	default:
		cli_assert(0);
	}

	return 0;
}

void
cli_cmd_fini(struct cli_cmd * command)
{
	cli_node_walk_safe(&command->super, cli_cmd_destroy_arg, NULL);
}

int
cli_cmd_create(struct cli_cmd **          command,
               const char *               name,
               const struct cli_cmd_ops * opers)
{
	struct cli_cmd * cmd;
	int              err;

	cmd = cli_malloc(sizeof(*cmd));

	err = cli_cmd_init(cmd, name, opers);
	if (err)
		return err;

	*command = cmd;

	return 0;
}
