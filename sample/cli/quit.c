#include "quit.h"
#include "cmd.h"
#include "cli.h"

/******************************************************************************
 * `quit' command handling.
 * Terminate command line.
 ******************************************************************************/

static int
cli_quit_exec_work(struct cli_work *    work __cli_unused,
                  struct cli_context * context __cli_unused)
{
	/*
	 * We might use cli_shell_shutdown() but we don't want to depend on the
	 * shell since we might be called from the command line directly...
	 */
	return -ESHUTDOWN;
}

static const struct cli_work_ops cli_quit_work_ops = {
	.exec    = cli_quit_exec_work,
	.release = cli_null_release_work
};

static int
cli_quit_parse_cmd(const struct cli_cmd * command,
                  const struct cli_dir * directory,
                  struct cli_context *   context,
                  int                    argc __cli_unused,
                  const char * const     argv[] __cli_unused)
{
	cli_cmd_assert(command);
	cli_dir_assert(directory);
	cli_assert_context(context);
	cli_assert(argv);

	struct cli_work * wk;
	int               ret;

	/* Cannot fail. */
	wk = cli_create_work(sizeof(*wk), &cli_quit_work_ops);

	ret = cli_sched_work(context, wk);
	if (ret) {
		cli_cmd_log(command, "cannot schedule work.");
		goto destroy;
	}

	return 0;

destroy:
	cli_destroy_work(wk);

	return ret;
}

static const struct cli_cmd_ops cli_quit_cmd_ops = {
	.parse = cli_quit_parse_cmd,
};

void
cli_quit_build_cmd(struct cli_dir * directory)
{
	cli_dir_assert(directory);

	struct cli_cmd * cmd;

	/*
	 * No need to check for returned code since cli_cmd_create() cannot fail
	 * with the "quit" name argument.
	 */
	cli_assert(sizeof("quit") <= CLI_ARG_MAX);
	cli_cmd_createn_add(&cmd, "quit", &cli_quit_cmd_ops, directory);
}
