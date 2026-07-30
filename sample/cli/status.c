#include "status.h"
#include "yang.h"
#include "cmd.h"

enum cli_data_format {
    CLI_TABLE_DATA_FMT = 0,
    CLI_JSON_DATA_FMT,
    CLI_XML_DATA_FMT,
    CLI_DATA_FMT_NR
};

/******************************************************************************
 * `status' command handling.
 * Print state data related to the directory node given in argument.
 ******************************************************************************/

struct cli_status_work {
	struct cli_dir_work  super;
	enum cli_data_format format;
};

static int
cli_status_exec_work(struct cli_work * work, struct cli_context * context)
{
	const struct cli_status_work * wk = (const struct cli_status_work *)
	                                    work;
	const struct cli_dir *         dir;
	int                            ret;

	ret = cli_dir_work_search(&wk->super, context, &dir);
	if (ret)
		return ret;

	if (cli_dir_type(dir) != CLI_DIR_NODE_TYPE) {
		cli_cmd_log(wk->super.cmd, "not available.");
		return -ENOMSG;
	}

	/*
	 * Given the directory descriptor found above, display its state related
	 * informations.
	 */
	sr_data_t * data;
	ret = cli_lyd_load_from_node(context, dir->sch_node, 2, &data);
	if (ret) {
		cli_cmd_log(wk->super.cmd,
		            "cannot load: %s.",
		            sr_strerror(ret));
		return -ENOMSG;
	}

	ret = lyd_print_all(context->lyout,
	                    data->tree,
	                    LYD_JSON,
	                    LYD_PRINT_WD_ALL_TAG);
	if (ret) {
		cli_cmd_log(wk->super.cmd, "cannot show: %s.", ly_strerr(ret));
		ret = -EBADR;
	}

	cli_lyd_unload(data);

	return 0;
}

static const struct cli_work_ops cli_status_work_ops = {
	.exec    = cli_status_exec_work,
	.release = cli_dir_work_release
};

static int
cli_status_parse_cmd(const struct cli_cmd * command,
                     const struct cli_dir * directory,
                     struct cli_context *   context,
                     int                    argc,
                     const char * const     argv[])
{
	cli_cmd_assert(command);
	cli_dir_assert(directory);
	cli_assert_context(context);
	cli_assert(argv);

	struct cli_status_work * wk;
	int                      ret;

	/* Cannot fail. */
	wk = (struct cli_status_work *)
	     cli_dir_work_create(sizeof(*wk), command, &cli_status_work_ops);
	wk->format = CLI_TABLE_DATA_FMT;

	ret = cli_cmd_parse_args(command, directory, context, argc, argv, wk);
	if (ret < 0)
		goto destroy;

	cli_assert(ret == argc);
	ret = cli_sched_work(context, (struct cli_work *)wk);
	if (ret) {
		cli_cmd_log(command, "cannot schedule work.");
		goto destroy;
	}

	return argc;

destroy:
	cli_dir_work_destroy((struct cli_dir_work *)wk);

	return ret;
}

static const struct cli_cmd_ops cli_status_cmd_ops = {
	.parse    = cli_status_parse_cmd,
	.complete = cli_cmd_complete_args
};

static int
cli_status_on_table_match(const struct cli_arg * argument __cli_unused,
                          const struct cli_cmd * command __cli_unused,
                          const struct cli_dir * directory __cli_unused,
                          struct cli_context *   context __cli_unused,
                          int                    argc __cli_unused,
                          const char * const     argv[] __cli_unused,
                          void *                 data)
{
	((struct cli_status_work *)data)->format = CLI_TABLE_DATA_FMT;

	return 0;
}

static int
cli_status_on_json_match(const struct cli_arg * argument __cli_unused,
                         const struct cli_cmd * command __cli_unused,
                         const struct cli_dir * directory __cli_unused,
                         struct cli_context *   context __cli_unused,
                         int                    argc __cli_unused,
                         const char * const     argv[] __cli_unused,
                         void *                 data)
{
	((struct cli_status_work *)data)->format = CLI_JSON_DATA_FMT;

	return 0;
}

static int
cli_status_on_xml_match(const struct cli_arg * argument __cli_unused,
                        const struct cli_cmd * command __cli_unused,
                        const struct cli_dir * directory __cli_unused,
                        struct cli_context *   context __cli_unused,
                        int                    argc __cli_unused,
                        const char * const     argv[] __cli_unused,
                        void *                 data)
{
	((struct cli_status_work *)data)->format = CLI_XML_DATA_FMT;

	return 0;
}

static const struct cli_arg_kword_term cli_status_format_terms[] = {
	CLI_ARG_KWORD_TERM("table", cli_status_on_table_match),
	CLI_ARG_KWORD_TERM("json",  cli_status_on_json_match),
	CLI_ARG_KWORD_TERM("xml",   cli_status_on_xml_match),
};

static struct cli_cmd *
cli_status_create_cmd(struct cli_dir * directory)
{
	cli_dir_assert(directory);
	cli_assert(cli_dir_type(directory) == CLI_DIR_NODE_TYPE);

	struct cli_cmd *            cmd;
	struct cli_arg *            choice;
	struct cli_arg_kword_parm * fmt;

	/*
	 * No need to check for returned code since cli_cmd_create() cannot fail
	 * with the "status" name argument.
	 */
	cli_assert(sizeof("status") <= CLI_ARG_MAX);
	cli_cmd_createn_add(&cmd, "status", &cli_status_cmd_ops, directory);

	/* Cannot fail. */
	choice = cli_arg_createn_add_choice((struct cli_node *)cmd);
	cli_dir_work_createn_add_arg(false, (struct cli_node *)choice);

	/*
	 * No need to check for returned code since this cannot fail with the
	 * "format" name argument.
	 */
	cli_assert(sizeof("format") <= CLI_ARG_MAX);
	cli_arg_createn_add_kword_parm(&fmt,
	                               "format",
	                               cli_status_format_terms,
	                               cli_array_nr(cli_status_format_terms),
	                               (struct cli_node *)choice);

	return cmd;
}

struct cli_cmd *
cli_status_make_cmd(struct cli_dir * directory)
{
	cli_dir_assert(directory);
	cli_assert(cli_dir_type(directory) == CLI_DIR_NODE_TYPE);

	struct cli_cmd * cmd;

	cmd = cli_dir_find_cmd(directory, "status");
	if (!cmd)
		cmd = cli_status_create_cmd(directory);

	cli_assert(cmd);

	return cmd;
}
