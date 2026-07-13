#include "schema.h"
#include "yang.h"
#include "cmd.h"

/******************************************************************************
 * `schema' command handling.
 * Print schema for current working command line node.
 ******************************************************************************/

struct cli_schema_work {
	struct cli_work       super;
	struct cli_dir_search search;
	LYS_OUTFORMAT         format;
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
			cli_log("schema: cannot show YANG tree diagram: %s.",
			        cli_dir_strerror(-ret));
		break;
#endif /* defined(CONFIG_CLI_DEBUG) */

	case LYS_OUT_YANG_COMPILED:
		ret = cli_dir_show_yang(directory, show->context);
		if (ret)
			cli_log("schema: cannot show YANG specification: %s.",
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
		.context = context,
		.work    = wk
	};

	/* Search for the requested directory. */
	ret = cli_dir_exec_search(&wk->search, &dir, context);
	if (ret) {
		/*
		 * Searching for the current directory cannot fail. Hence,
		 * `wk->search.orig' should always exist here.
		 */
		cli_assert(wk->search.orig);

		cli_log("schema: '%s': %s.",
		        wk->search.orig,
		        cli_dir_strerror(-ret));

		return ret;
	}

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

static void
cli_schema_release_work(struct cli_work *    work,
                        struct cli_context * context __cli_unused)
{
	cli_dir_fini_search(&((struct cli_schema_work *)work)->search);
}

static const struct cli_work_ops cli_schema_work_ops = {
	.exec    = cli_schema_exec_work,
	.release = cli_schema_release_work
};

static int
cli_schema_parse_cmd(const struct cli_cmd * command __cli_unused,
                     const struct cli_dir * dir __cli_unused,
                     int                    argc,
                     const char * const     argv[],
                     void *                 data)
{
	cli_assert(argc >= 1);

	if (!strcmp(argv[0], "schema")) {
		if (argc <= 2) {
			ssize_t                  ret;
			struct cli_schema_work * wk;

			wk = (struct cli_schema_work *)
			     cli_create_work(sizeof(*wk), &cli_schema_work_ops);
			cli_assert(wk);
			cli_dir_init_search(&wk->search);

			ret = cli_dir_parse_search(&wk->search,
			                           (argc == 1) ? NULL
			                                       : argv[1]);
			if (ret) {
				cli_log("schema: invalid path: %s.",
				        cli_dir_strerror(-ret));
				goto destroy;
			}

			wk->format = LYS_OUT_YANG_COMPILED;
#warning Implement schema format option parsing support
#if 0
#if defined(CONFIG_CLI_DEBUG)
			wk->format = LYS_OUT_TREE;
#endif /* defined(CONFIG_CLI_DEBUG) */
#endif
			ret = cli_sched_work(data, &wk->super);
			if (ret) {
				cli_log("schema: cannot schedule work.");
				goto destroy;
			}

			return argc;

destroy:
			cli_destroy_work(&wk->super, data);

			return ret;
		}
		else {
			cli_log("schema: too many argument(s).");

			return -EINVAL;
		}
	}
	else
		return 0;
}

static const struct cli_cmd_ops cli_schema_cmd_ops = {
	.parse = cli_schema_parse_cmd,
	.fini  = cli_cmd_null_fini
};

void
cli_schema_build_cmd(struct cli_dir * directory)
{
	cli_dir_assert(directory);

	cli_dir_add_cmd(directory, cli_cmd_create(&cli_schema_cmd_ops));
}
