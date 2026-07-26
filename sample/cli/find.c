#include "find.h"
#include "cli.h"
#include "cmd.h"

/******************************************************************************
 * `find' command handling.
 * List menu directory entries recursively.
 ******************************************************************************/

struct cli_find_show {
	const struct cli_dir * anc;
	char *                 path;
	size_t                 off;
};

static int
cli_find_show_dir(struct cli_dir *    directory,
                  enum cli_walk_event event,
                  void *              data)
{
	cli_dir_assert(directory);
	cli_assert(data);

	switch (event) {
	case CLI_WALK_PRE_EVT:
		{
			const struct cli_find_show * show = data;
			ssize_t                      ret;

			ret = cli_dir_mkrel(directory,
			                    show->anc,
			                    &show->path[show->off],
			                    CLI_PATH_MAX - show->off);

			cli_assert(ret);
			if (ret > 0)
				printf("%s\n", show->path);
			else
				cli_log("find: cannot show: %s.",
				        cli_dir_strerror(-ret));

			break;
		}

	case CLI_WALK_POST_EVT:
		break;

	default:
		cli_assert(0);
	}

	return CLI_WALK_CONT_RET;
}

static int
cli_find_exec_work(struct cli_work * work, struct cli_context * context)
{
	struct cli_dir_work *  wk = (struct cli_dir_work *)work;
	const struct cli_dir * dir;
	int                    ret;
	struct cli_find_show   show;

	ret = cli_dir_work_search(wk, context, &dir);
	if (ret)
		return ret;

	/*
	 * Given the directory descriptor found above, display its descendant
	 * directory entries.
	 */
	show.anc = dir;
	show.path = wk->norm;
	show.off = wk->len;

	return cli_dir_walk((struct cli_dir *)dir, cli_find_show_dir, &show);
}

static const struct cli_work_ops cli_find_work_ops = {
	.exec    = cli_find_exec_work,
	.release = cli_dir_work_release
};

static int
cli_find_parse_cmd(const struct cli_cmd * command,
                   const struct cli_dir * directory,
                   struct cli_context *   context,
                   int                    argc,
                   const char * const     argv[])
{
	cli_cmd_assert(command);
	cli_dir_assert(directory);
	cli_assert_context(context);
	cli_assert(argv);

	struct cli_dir_work * wk;
	int                   ret;

	/* Cannot fail. */
	wk = cli_dir_work_create(sizeof(*wk), command, &cli_find_work_ops);

	ret = cli_cmd_parse_args(command, directory, context, argc, argv, wk);
	if (ret < 0)
		goto destroy;

	cli_assert(ret == argc);
	ret = cli_sched_work(context, &wk->super);
	if (ret) {
		cli_cmd_log(command, "cannot schedule work.");
		goto destroy;
	}

	return argc;

destroy:
	cli_dir_work_destroy(wk);

	return ret;
}

static const struct cli_cmd_ops cli_find_cmd_ops = {
	.parse    = cli_find_parse_cmd,
	.complete = cli_cmd_complete_args
};

void
cli_find_build_cmd(struct cli_dir * directory)
{
	cli_dir_assert(directory);

	struct cli_cmd * cmd;

	/*
	 * No need to check for returned code since cli_cmd_create() cannot fail
	 * with the "find" name argument.
	 */
	cli_assert(sizeof("find") <= CLI_ARG_MAX);
	cli_cmd_createn_add(&cmd, "find", &cli_find_cmd_ops, directory);

	/* Cannot fail either. */
	cli_dir_work_createn_add_arg(false, (struct cli_node *)cmd);
}
