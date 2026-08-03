#include "schema.h"
#include "yang.h"
#include "cmd.h"

/******************************************************************************
 * `schema' command handling.
 * Print schema for current working command line node.
 ******************************************************************************/

struct cli_schema_work {
	struct cli_dir_work super;
	LYS_OUTFORMAT       format;
};

struct cli_schema_show {
	const struct cli_schema_work * work;
	const struct cli_context *     context;
};

static int
cli_schema_show_dir(const struct cli_dir *         directory,
                    const struct cli_schema_show * show)
{
	cli_dir_assert(directory);
	cli_assert((cli_dir_type(directory) == CLI_DIR_MOD_TYPE) ||
	           (cli_dir_type(directory) == CLI_DIR_NODE_TYPE));
	cli_assert(show);
	cli_assert(show->work);
	cli_assert_context(show->context);

	int ret;

	switch (show->work->format) {
#if defined(CONFIG_CLI_DEBUG)
	case LYS_OUT_TREE:
		ret = cli_dir_show_diag(directory, show->context);
		if (ret)
			cli_cmd_log(
				show->work->super.cmd,
				"schema: cannot show YANG tree diagram: %s.",
				cli_dir_strerror(-ret));
		break;
#endif /* defined(CONFIG_CLI_DEBUG) */

	case LYS_OUT_YANG_COMPILED:
		ret = cli_dir_show_yang(directory, show->context);
		if (ret)
			cli_cmd_log(
				show->work->super.cmd,
				"schema: cannot show YANG specification: %s.",
				cli_dir_strerror(-ret));
		break;

	default:
		cli_assert(0);
	}

	return ret;
}

static int
cli_schema_show_visit(struct cli_dir *    directory,
                      enum cli_walk_event event,
                      void *              data)
{
	cli_dir_assert(directory);
	cli_assert(data);

	switch (event) {
	case CLI_WALK_PRE_EVT:
		if (cli_dir_type(directory) != CLI_DIR_NONE_TYPE) {
			cli_schema_show_dir(directory, data);

			return CLI_WALK_SKIP_RET;
		}

		break;

	case CLI_WALK_POST_EVT:
		break;

	default:
		cli_assert(0);
	}

	return CLI_WALK_CONT_RET;
}

static int
cli_schema_exec_work(struct cli_work * work, struct cli_context * context)
{
	const struct cli_schema_work * wk = (const struct cli_schema_work *)
	                                    work;
	const struct cli_dir *         dir;
	int                            ret;
	const struct cli_schema_show   show = {
		.work    = wk,
		.context = context
	};

	ret = cli_dir_work_search(&wk->super, context, &dir);
	if (ret)
		return ret;

	/*
	 * Given the directory descriptor found above, display its schema if it
	 * matches a real libyang object (module or node)...
	 */
	if (cli_dir_type(dir) != CLI_DIR_NONE_TYPE)
		return cli_schema_show_dir(dir, &show);

	/* ... search the highest level children pointing to a real libyang
	 * object and show their corresponding schemas.
	 */
	return cli_dir_walk((struct cli_dir *)dir,
	                    cli_schema_show_visit,
	                    (void *)&show);
}

static const struct cli_work_ops cli_schema_work_ops = {
	.exec    = cli_schema_exec_work,
	.release = cli_dir_work_release
};

static int
cli_schema_parse_cmd(const struct cli_cmd * command,
                     const struct cli_dir * directory,
                     struct cli_context *   context,
                     int                    argc,
                     const char * const     argv[])
{
	cli_cmd_assert(command);
	cli_dir_assert(directory);
	cli_assert_context(context);
	cli_assert(argv);

	struct cli_schema_work * wk;
	int                      ret;

	/* Cannot fail. */
	wk = (struct cli_schema_work *)
	     cli_dir_work_create(sizeof(*wk), command, &cli_schema_work_ops);
	wk->format = LYS_OUT_YANG_COMPILED;

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

static const struct cli_cmd_ops cli_schema_cmd_ops = {
	.parse    = cli_schema_parse_cmd,
	.complete = cli_cmd_complete_args,
	.fini     = cli_cmd_null_fini
};

static int
cli_schema_on_yang_match(const struct cli_arg * argument __cli_unused,
                         const struct cli_cmd * command __cli_unused,
                         const struct cli_dir * directory __cli_unused,
                         struct cli_context *   context __cli_unused,
                         int                    argc __cli_unused,
                         const char * const     argv[] __cli_unused,
                         void *                 data)
{
	((struct cli_schema_work *)data)->format = LYS_OUT_YANG_COMPILED;

	return 0;
}

#if defined(CONFIG_CLI_DEBUG)

static int
cli_schema_on_tree_match(const struct cli_arg * argument __cli_unused,
                         const struct cli_cmd * command __cli_unused,
                         const struct cli_dir * directory __cli_unused,
                         struct cli_context *   context __cli_unused,
                         int                    argc __cli_unused,
                         const char * const     argv[] __cli_unused,
                         void *                 data)
{
	((struct cli_schema_work *)data)->format = LYS_OUT_TREE;

	return 0;
}

#endif /* defined(CONFIG_CLI_DEBUG) */

static const struct cli_arg_kword_term cli_schema_format_terms[] = {
	CLI_ARG_KWORD_TERM("yang", cli_schema_on_yang_match),
#if defined(CONFIG_CLI_DEBUG)
	CLI_ARG_KWORD_TERM("tree", cli_schema_on_tree_match),
#endif /* defined(CONFIG_CLI_DEBUG) */
};

void
cli_schema_build_cmd(struct cli_dir * directory)
{
	cli_dir_assert(directory);

	struct cli_cmd *            cmd;
	struct cli_arg *            choice;
	struct cli_arg_kword_parm * fmt;

	/*
	 * No need to check for returned code since cli_cmd_create() cannot fail
	 * with the "schema" name argument.
	 */
	cli_assert(sizeof("schema") <= CLI_ARG_MAX);
	cli_dir_create_cmdn_add(directory, &cmd, "schema", &cli_schema_cmd_ops);

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
	                               cli_schema_format_terms,
	                               cli_array_nr(cli_schema_format_terms),
	                               (struct cli_node *)choice);
}
