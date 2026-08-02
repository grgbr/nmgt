#include "status.h"
#include "lyd_table.h"
#include "yang.h"
#include "cmd.h"

struct cli_status_cmd {
	struct cli_cmd         super;
	struct cli_lyd_table * table;
};

static int
cli_status_show_table(const struct cli_cmd *     command,
                      const struct cli_context * context)
{
	cli_cmd_assert(command);
	cli_lyd_table_assert(((struct cli_status_cmd *)command)->table);
	cli_assert_context(context);

	struct cli_status_cmd * cmd = (struct cli_status_cmd *)command;
	int                     ret;

	ret = cli_table_load((struct cli_table *)cmd->table, context, NULL);
	if (ret) {
		cli_cmd_log(command, "cannot load: %s.", sr_strerror(ret));
		return -ENOMSG;
	}

	ret = cli_table_show((struct cli_table *)cmd->table, false, stdout);
	if (ret) {
		cli_cmd_log(command, "cannot show: %s.", strerror(-ret));
		return -ENOMSG;
	}

	return 0;
}

static int
cli_status_show_json(const struct cli_cmd *     command,
                     const struct lysc_node *   schema,
                     const struct cli_context * context)
{
	sr_data_t * data;
	int         ret;

	ret = cli_lyd_load_from_schema(context,
	                               schema,
	                               2,
	                               SR_OPER_NO_CONFIG,
	                               &data);
	if (ret) {
		cli_cmd_log(command, "cannot load: %s.", sr_strerror(ret));
		return -ENOMSG;
	}

	ret = lyd_print_all(context->lyout,
	                    data->tree,
	                    LYD_JSON,
	                    LYD_PRINT_WD_ALL_TAG);
	if (ret) {
		cli_cmd_log(command, "cannot show: %s.", ly_strerr(ret));
		ret = -EBADR;
	}

	cli_lyd_unload(data);

	return -EBADR;
}

/******************************************************************************
 * `status' command handling.
 * Print state data related to the directory node given in argument.
 ******************************************************************************/

enum cli_data_format {
    CLI_TABLE_DATA_FMT = 0,
    CLI_JSON_DATA_FMT,
    CLI_XML_DATA_FMT,
    CLI_DATA_FMT_NR
};

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
	switch (wk->format) {
	case CLI_TABLE_DATA_FMT:
		ret = cli_status_show_table(wk->super.cmd, context);
		break;

	case CLI_JSON_DATA_FMT:
		ret = cli_status_show_json(wk->super.cmd,
		                           dir->sch_node,
		                           context);
		break;

	case CLI_XML_DATA_FMT:
	default:
		cli_assert(0);
	}

	return ret;
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

static void
cli_status_fini_cmd(struct cli_cmd * command)
{
	struct cli_lyd_table * tbl = ((struct cli_status_cmd *)command)->table;

	cli_lyd_table_fini(tbl);
	cli_free(tbl);
}

static const struct cli_cmd_ops cli_status_cmd_ops = {
	.parse    = cli_status_parse_cmd,
	.complete = cli_cmd_complete_args,
	.fini     = cli_status_fini_cmd
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

static bool
cli_status_filter_node(const struct lysc_node * node)
{
	cli_assert(node);
	cli_assert(node->nodetype == LYS_LEAF);

	return (cli_lysc_conf_flags(node) == LYS_CONFIG_R) &&
	       !(cli_lysc_status_flags(node) & LYS_STATUS_DEPRC);
}

static struct cli_status_cmd *
cli_status_create_cmd(struct cli_dir *           directory,
                      const struct cli_context * context)
{
	cli_dir_assert(directory);
	cli_assert(cli_dir_type(directory) == CLI_DIR_NODE_TYPE);
	cli_assert_context(context);

	struct cli_status_cmd *     cmd;
	struct cli_lyd_table *      tbl;
	int                         err;
	struct cli_arg *            choice;
	struct cli_arg_kword_parm * fmt;

	/*
	 * No need to check for returned code since cli_cmd_create() cannot fail
	 * with the "status" name argument.
	 */
	cli_assert(sizeof("status") <= CLI_ARG_MAX);
	cli_cmd_sized_create((struct cli_cmd **)&cmd,
	                     sizeof(*cmd),
	                     "status",
	                     &cli_status_cmd_ops);

	/*
	 * Cannot fail since calling cli_status_make_cmd() previously made sure
	 * that there is at least one candidate leaf for the directory given in
	 * argument.
	 */
	tbl = cli_malloc(sizeof(*tbl));
	err = cli_lyd_table_init(tbl,
	                         directory->sch_node,
	                         cli_status_filter_node,
	                         context);
	cli_assert(!err);
	cmd->table = tbl;

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

	cli_dir_add_cmd(directory, &cmd->super);

	return cmd;
}

struct cli_cmd *
cli_status_make_cmd(struct cli_dir *              directory,
                    const struct lysc_node_leaf * leaf,
                    const struct cli_context *    context)
{
	cli_dir_assert(directory);
	cli_assert(cli_dir_type(directory) == CLI_DIR_NODE_TYPE);
	cli_assert(leaf);
	cli_assert_context(context);

	/*
	 * Make sure that there is at least one leaf suitable for the status
	 * command to display available informations.
	 */
	if (cli_status_filter_node(&leaf->node)) {
		struct cli_cmd * cmd;

		cmd = cli_dir_find_cmd(directory, "status");
		if (cmd)
			return cmd;

		return (struct cli_cmd *)cli_status_create_cmd(directory,
		                                               context);
	}
	else
		return NULL;
}
