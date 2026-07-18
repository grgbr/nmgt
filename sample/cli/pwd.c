#include "pwd.h"
#include "cmd.h"
#include "cli.h"

/******************************************************************************
 * `pwd' command handling.
 * Show absolute path of current working directory.
 ******************************************************************************/

static int
cli_pwd_exec_work(struct cli_work *    work __cli_unused,
                  struct cli_context * context)
{

	char *  path;
	ssize_t ret;

	path = cli_malloc(CLI_PATH_MAX);
	cli_assert(path);

	ret = cli_dir_mkabs(cli_cwd(context), path, CLI_PATH_MAX);
	if (ret > 0) {
		printf("%s\n", path);
		ret = 0;
		goto out;
	}

	cli_log("pwd: cannot show current working directory: %s.",
	        cli_dir_strerror(-ret));

out:
	cli_free(path);

	return ret;
}

static const struct cli_work_ops cli_pwd_work_ops = {
	.exec    = cli_pwd_exec_work,
	.release = cli_null_release_work
};

static int
cli_pwd_parse_cmd(const struct cli_cmd * command,
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
	wk = cli_create_work(sizeof(*wk), &cli_pwd_work_ops);

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

static const struct cli_cmd_ops cli_pwd_cmd_ops = {
	.parse = cli_pwd_parse_cmd,
};

void
cli_pwd_build_cmd(struct cli_dir * directory)
{
	cli_dir_assert(directory);

	struct cli_cmd * cmd;

	/*
	 * No need to check for returned code since cli_cmd_create() cannot fail
	 * with the "ls" name argument.
	 */
	cli_assert(sizeof("pwd") <= CLI_ARG_MAX);
	cli_cmd_createn_add(&cmd, "pwd", &cli_pwd_cmd_ops, directory);
}
