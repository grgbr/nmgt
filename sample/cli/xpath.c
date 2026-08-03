#include "xpath.h"
#include "yang.h"
#include "cmd.h"

/******************************************************************************
 * `xpath' command handling.
 * Print XPATH for current working directory.
 ******************************************************************************/

static int
cli_xpath_exec_work(struct cli_work * work, struct cli_context * context)
{
	struct cli_dir_work *  wk = (struct cli_dir_work *)work;
	const struct cli_dir * dir;
	int                    ret;
	char *                 xpath;

	ret = cli_dir_work_search(wk, context, &dir);
	if (ret)
		return ret;

	/*
	 * Given the directory descriptor found above, compute the corresponding
	 * XPATH and display it.
	 */
	xpath = cli_dir_xpath(dir);
	cli_assert(xpath);
	printf("%s\n", xpath);
	cli_free(xpath);

	return 0;

}

static const struct cli_work_ops cli_xpath_work_ops = {
	.exec    = cli_xpath_exec_work,
	.release = cli_dir_work_release
};

static int
cli_xpath_parse_cmd(const struct cli_cmd * command,
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
	wk = cli_dir_work_create(sizeof(*wk), command, &cli_xpath_work_ops);

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

static const struct cli_cmd_ops cli_xpath_cmd_ops = {
	.parse    = cli_xpath_parse_cmd,
	.complete = cli_cmd_complete_args,
	.fini     = cli_cmd_null_fini
};

void
cli_xpath_build_cmd(struct cli_dir * directory)
{
	cli_dir_assert(directory);

	struct cli_cmd * cmd;

	/*
	 * No need to check for returned code since cli_cmd_create() cannot fail
	 * with the "xpath" name argument.
	 */
	cli_assert(sizeof("xpath") <= CLI_ARG_MAX);
	cli_dir_create_cmdn_add(directory, &cmd, "xpath", &cli_xpath_cmd_ops);

	/* Cannot fail either. */
	cli_dir_work_createn_add_arg(false, (struct cli_node *)cmd);
}
