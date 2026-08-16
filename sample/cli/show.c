/******************************************************************************
 * Generic show command handling.
 *
 * Print configuration or operational state data related to the directory node
 * given in argument.
 ******************************************************************************/

#include "show.h"
#include "lyd_table.h"
#include "yang.h"
#include "cmd.h"

enum cli_data_format {
    CLI_TABLE_DATA_FMT = 0,
    CLI_JSON_DATA_FMT,
    CLI_XML_DATA_FMT,
    CLI_DATA_FMT_NR
};

struct cli_show_work {
	struct cli_dir_work  super;
	enum cli_data_format format;
};

#define cli_show_assert_work(_work) \
	cli_assert(_work); \
	cli_assert((_work)->format >= 0); \
	cli_assert((_work)->format < CLI_DATA_FMT_NR)

struct cli_show_cmd {
	struct cli_cmd         super;
	struct cli_lyd_table * table;
	sr_get_oper_flag_t     flags;
};

#define cli_show_assert_cmd(_cmd) \
	cli_assert(_cmd); \
	cli_cmd_assert(&(_cmd)->super); \
	cli_lyd_table_assert((_cmd)->table); \
	cli_assert(((_cmd)->flags == SR_OPER_NO_STATE) || \
	           ((_cmd)->flags == SR_OPER_NO_CONFIG))

#warning TODO: implement arbitrary output stream for pager support.
static int
cli_show_table(const struct cli_cmd *     command,
               struct cli_lyd_table *     table,
               const struct cli_context * context)
{
	cli_show_assert_cmd((const struct cli_show_cmd *)command);
	cli_lyd_table_assert(table);
	cli_assert_context(context);

	int ret;

	ret = cli_table_load((const struct cli_table *)table, context, NULL);
	if (ret) {
		cli_cmd_log(command,
		            "cannot load table data: %s.",
		            sr_strerror(ret));
		return -ENOMSG;
	}

	ret = cli_table_show((const struct cli_table *)table, false, stdout);
	if (ret) {
		cli_cmd_log(command,
		            "cannot show table data: %s.",
		            strerror(-ret));
		return -ENOMSG;
	}

	return 0;
}

#warning TODO: implement arbitrary output stream for pager support.
static int
cli_show_format(const struct cli_cmd *     command,
                const struct lysc_node *   schema,
                sr_get_oper_flag_t         flags,
                LYD_FORMAT                 format,
                const struct cli_context * context)
{
	cli_show_assert_cmd((const struct cli_show_cmd *)command);
	cli_assert(schema);
	cli_assert((flags == SR_OPER_NO_STATE) || (flags == SR_OPER_NO_CONFIG));
	cli_assert((format == LYD_JSON) || (format == LYD_XML));
	cli_assert_context(context);

	sr_data_t * data;
	int         ret;

	ret = cli_lyd_load_from_schema(context,
	                               schema,
	                               2,
	                               flags,
	                               &data);
	if (ret) {
		cli_cmd_log(command, "cannot load data: %s.", sr_strerror(ret));
		return -ENOMSG;
	}

	ret = lyd_print_all(context->lyout,
	                    data->tree,
	                    format,
	                    LYD_PRINT_WD_ALL_TAG);
	if (ret) {
		cli_cmd_log(command, "cannot show data: %s.", ly_strerr(ret));
		ret = -EBADR;
	}

	cli_lyd_unload(data);

	return -EBADR;
}

static int
cli_show_exec_work(struct cli_work * work, struct cli_context * context)
{
	cli_show_assert_work((const struct cli_show_work *)work);
	cli_show_assert_cmd((const struct cli_show_cmd *)
	                    (((const struct cli_show_work *)work)->super.cmd));
	cli_assert_context(context);

	const struct cli_show_work * wk = (const struct cli_show_work *)work;
	const struct cli_show_cmd *  cmd = (const struct cli_show_cmd *)
	                                   wk->super.cmd;
	const struct cli_dir *       dir;
	int                          ret;

	ret = cli_dir_work_search(&wk->super, context, &dir);
	if (ret)
		return ret;

	/*
	 * Given the directory descriptor found above, display its state related
	 * informations.
	 */
	switch (wk->format) {
	case CLI_TABLE_DATA_FMT:
		ret = cli_show_table(&cmd->super, cmd->table, context);
		break;

	case CLI_JSON_DATA_FMT:
		ret = cli_show_format(&cmd->super,
		                      dir->sch_node,
		                      cmd->flags,
		                      LYD_JSON,
		                      context);
		break;

	case CLI_XML_DATA_FMT:
		ret = cli_show_format(&cmd->super,
		                      dir->sch_node,
		                      cmd->flags,
		                      LYD_XML,
		                      context);
		break;

	default:
		cli_assert(0);
	}

	return ret;
}

static const struct cli_work_ops cli_show_work_ops = {
	.exec    = cli_show_exec_work,
	.release = cli_dir_work_release
};

static int
cli_show_parse_cmd(const struct cli_cmd * command,
                   const struct cli_dir * directory,
                   struct cli_context *   context,
                   int                    argc,
                   const char * const     argv[])
{
	cli_show_assert_cmd((const struct cli_show_cmd *)command);
	cli_dir_assert(directory);
	cli_assert_context(context);
	cli_assert(argv);

	struct cli_show_work * wk;
	int                    ret;

	/* Cannot fail. */
	wk = (struct cli_show_work *)
	     cli_dir_work_create(sizeof(*wk), command, &cli_show_work_ops);
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
cli_show_fini_cmd(struct cli_cmd * command)
{
	struct cli_lyd_table * tbl = ((struct cli_show_cmd *)command)->table;

	cli_lyd_table_fini(tbl);
	cli_free(tbl);
}

static const struct cli_cmd_ops cli_show_cmd_ops = {
	.parse    = cli_show_parse_cmd,
	.complete = cli_cmd_complete_args,
	.fini     = cli_show_fini_cmd
};

static int
cli_show_on_table_match(const struct cli_arg * argument __cli_unused,
                        const struct cli_cmd * command __cli_unused,
                        const struct cli_dir * directory __cli_unused,
                        struct cli_context *   context __cli_unused,
                        int                    argc __cli_unused,
                        const char * const     argv[] __cli_unused,
                        void *                 data)
{
	((struct cli_show_work *)data)->format = CLI_TABLE_DATA_FMT;

	return 0;
}

static int
cli_show_on_json_match(const struct cli_arg * argument __cli_unused,
                       const struct cli_cmd * command __cli_unused,
                       const struct cli_dir * directory __cli_unused,
                       struct cli_context *   context __cli_unused,
                       int                    argc __cli_unused,
                       const char * const     argv[] __cli_unused,
                       void *                 data)
{
	((struct cli_show_work *)data)->format = CLI_JSON_DATA_FMT;

	return 0;
}

static int
cli_show_on_xml_match(const struct cli_arg * argument __cli_unused,
                       const struct cli_cmd * command __cli_unused,
                       const struct cli_dir * directory __cli_unused,
                       struct cli_context *   context __cli_unused,
                       int                    argc __cli_unused,
                       const char * const     argv[] __cli_unused,
                       void *                 data)
{
	((struct cli_show_work *)data)->format = CLI_XML_DATA_FMT;

	return 0;
}

static const struct cli_arg_kword_term cli_show_format_terms[] = {
	CLI_ARG_KWORD_TERM("table", cli_show_on_table_match),
	CLI_ARG_KWORD_TERM("json",  cli_show_on_json_match),
	CLI_ARG_KWORD_TERM("xml",   cli_show_on_xml_match),
};

static bool
cli_show_filter_config_node(const struct lysc_node * node)
{
	cli_assert(node);
	cli_assert(node->nodetype == LYS_LEAF);

	return (cli_lysc_conf_flags(node) == LYS_CONFIG_W) &&
	       !(cli_lysc_status_flags(node) & LYS_STATUS_DEPRC);
}

static bool
cli_show_filter_oper_node(const struct lysc_node * node)
{
	cli_assert(node);
	cli_assert(node->nodetype == LYS_LEAF);

	return (cli_lysc_conf_flags(node) == LYS_CONFIG_R) &&
	       !(cli_lysc_status_flags(node) & LYS_STATUS_DEPRC);
}

static int
cli_show_create_cmd(struct cli_show_cmd **             command,
                    struct cli_dir *                   directory,
                    const struct lysc_node_container * container,
                    const char *                       name,
                    cli_lyd_table_filter_node_fn *     filter,
                    sr_get_oper_flag_t                 flags,
                    const struct cli_context *         context)
{
	cli_assert(command);
	cli_dir_assert(directory);
	cli_assert(container);
	cli_assert(cli_cmd_name_isok(name));
	cli_assert(filter);
	cli_assert((flags == SR_OPER_NO_STATE) || (flags == SR_OPER_NO_CONFIG));
	cli_assert_context(context);

	struct cli_show_cmd *       cmd;
	struct cli_lyd_table *      tbl;
	int                         err;
	struct cli_arg *            choice;
	struct cli_arg_kword_parm * fmt;

	/*
	 * No need to check for returned code since cli_cmd_create() cannot fail
	 * with a command name validated thanks to cli_cmd_name_isok().
	 */
	cli_cmd_sized_create((struct cli_cmd **)&cmd,
	                     sizeof(*cmd),
	                     name,
	                     &cli_show_cmd_ops);

	/*
	 * Cannot fail since calling cli_show_make_cmd() previously made sure
	 * that there is at least one candidate leaf for the directory given in
	 * argument.
	 */
	tbl = cli_malloc(sizeof(*tbl));
	err = cli_lyd_table_init(tbl, &container->node, filter, context);
	if (err) {
		cli_assert(err == -ENOENT);
		goto free;
	}

	cmd->table = tbl;
	cmd->flags = flags;

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
	                               cli_show_format_terms,
	                               cli_array_nr(cli_show_format_terms),
	                               (struct cli_node *)choice);

	cli_dir_add_cmd(directory, &cmd->super);

	*command = cmd;

	return 0;

free:
	cli_free(tbl);

	return err;
}

static int
cli_show_make_cmd(struct cli_dir *                   directory,
                  const struct lysc_node_container * container,
                  const char *                       name,
                  cli_lyd_table_filter_node_fn *     filter,
                  sr_get_oper_flag_t                 flags,
                  const struct cli_context *         context)
{
	cli_dir_assert(directory);
	cli_assert(container);
	cli_assert(name);
	cli_assert(filter);
	cli_assert((flags == SR_OPER_NO_STATE) || (flags == SR_OPER_NO_CONFIG));
	cli_assert_context(context);

	struct cli_cmd * cmd;
	const char *     msg;
	int              ret;

	if (!cli_cmd_name_isok(name)) {
		msg = "invalid name";
		ret = -EINVAL;
		goto err;
	}

	if (cli_dir_find_cmd(directory, name)) {
		msg = "already exists";
		ret = -EEXIST;
		goto err;
	}

	ret = cli_show_create_cmd((struct cli_show_cmd **)&cmd,
	                          directory,
	                          container,
	                          name,
	                          filter,
	                          flags,
	                          context);
	if (ret) {
		cli_assert(ret == -ENOENT);

#if defined(CONFIG_CLI_DEBUG)
		cli_lysc_log((const struct lysc_node *)container,
		             "ignoring '%s' show command creation: "
		             "no child schema data node found.",
		             name);
#endif /* defined(CONFIG_CLI_DEBUG) */
	}

	return 0;

err:
	cli_lysc_log((const struct lysc_node *)container,
	             "cannot create show command '%s': %s.",
	             name,
	             msg);

	return ret;
}

int
cli_show_make_config_cmd(struct cli_dir *                   directory,
                         const struct lysc_node_container * container,
                         const char *                       name,
                         const struct cli_context *         context)
{
	cli_dir_assert(directory);
	cli_assert(container);
	cli_assert(name);
	cli_assert_context(context);

	return cli_show_make_cmd(directory,
	                         container,
	                         name,
	                         cli_show_filter_config_node,
	                         SR_OPER_NO_STATE,
	                         context);
}

int
cli_show_make_oper_cmd(struct cli_dir *                   directory,
                       const struct lysc_node_container * container,
                       const char *                       name,
                       const struct cli_context *         context)
{
	cli_dir_assert(directory);
	cli_assert(container);
	cli_assert(name);
	cli_assert_context(context);

	return cli_show_make_cmd(directory,
	                         container,
	                         name,
	                         cli_show_filter_oper_node,
	                         SR_OPER_NO_CONFIG,
	                         context);
}
