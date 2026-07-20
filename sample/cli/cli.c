#include "cli.h"
#include "yang.h"
#include "list.h"
#include "cd.h"
#include "pwd.h"
#include "find.h"
#include "xpath.h"
#include "schema.h"
#include <sys/ioctl.h>

/******************************************************************************
 * Utilities
 ******************************************************************************/

unsigned int
cli_term_cols(const struct cli_context * context)
{
	cli_assert_context(context);

	struct winsize wsz;

#warning TODO: plug in a SIGWINCH signal handler instead
	if (context->isatty && !ioctl(STDOUT_FILENO, TIOCGWINSZ, &wsz))
		return (unsigned int)wsz.ws_col;
	else
		return 0;
}

/******************************************************************************
 * Work handling
 ******************************************************************************/

static void
cli_release_workq(struct cli_context * context)
{
	cli_assert_context(context);

	unsigned int w;

	for (w = 0; w < context->wkcnt; w++)
		cli_destroy_work(context->wkq[w]);

	context->wkcnt = 0;
}

static int
cli_exec_workq(struct cli_context * context)
{
	cli_assert_context(context);

	unsigned int w;
	int          ret;

	for (w = 0, ret = 0; (w < context->wkcnt) && !ret; w++) {
		struct cli_work * wk = context->wkq[w];

		ret = wk->ops->exec(wk, context);
	}

	cli_release_workq(context);

	return ret;
}

/******************************************************************************
 * Overall cli context handling
 ******************************************************************************/

static int
cli_parse(struct cli_context * context, int argc, const char * const argv[])
{
	cli_assert_context(context);
	cli_assert_args(argc, argv);

	return cli_dir_parse_cmd(context->cwd, context, argc, argv);
}

#if defined(CONFIG_CLI_LOG)

static sr_log_level_t cli_log_lvl;

static void
cli_sr_log_cb(sr_log_level_t level, const char * message)
{
	if (level <= cli_log_lvl) {
		const char * lvl;

		switch (level) {
		case SR_LL_ERR:
			lvl = "err";
			break;
		case SR_LL_WRN:
			lvl = "warn";
			break;
		case SR_LL_INF:
			lvl = "info";
			break;
		case SR_LL_VRB:
			lvl = "verb";
			break;
		case SR_LL_DBG:
			lvl = "dbg";
			break;
		default:
			cli_assert(0);
		}

		cli_log("{%*.*s} repo: %s", 4, 4, lvl, message);
	}
}

static void
cli_setup_log(sr_log_level_t level)
{
	/* Disable all internal sysrepo / libyang logging messages... */
	sr_log_stderr(SR_LL_NONE);

	/* ... and setup our own logging callback instead. */
	switch (level) {
	case SR_LL_NONE:
		/* Disable sysrepo / libyang logging entirely. */
		sr_log_set_cb(NULL);
		break;

	case SR_LL_ERR:
	case SR_LL_WRN:
	case SR_LL_INF:
	case SR_LL_VRB:
	case SR_LL_DBG:
		/*
		 * Install our logging callback. It will always be called for
		 * every messages, regardless of any log level.
		 */
		cli_log_lvl = level;
		sr_log_set_cb(cli_sr_log_cb);
		break;

	default:
		cli_assert(0);
	}
}

#else  /* !defined(CONFIG_CLI_LOG) */

static void
cli_setup_log(sr_log_level_t level)
{
	/* Disable sysrepo / libyang logging entirely. */
	sr_log_set_cb(NULL);
}

#endif /* defined(CONFIG_CLI_LOG) */

static int
cli_init_context(struct cli_context * context)
{
	cli_assert(context);

	int err;

	cli_setup_log(CONFIG_CLI_LOG_LEVEL);

#if defined(CONFIG_CLI_DEBUG)
	/*
	 * SR_CTX_SET_PRIV_PARSED is required to print YANG diagram trees.
	 * In addition, SR_CTX_SET_PRIV_PARSED is activated for non-printed
	 * contexts only.
	 * This is why the SR_CTX_NO_PRINTED option must also be
	 * passed at the expense of reduced performances.
	 */
	err = sr_context_options(SR_CTX_NO_PRINTED | SR_CTX_SET_PRIV_PARSED,
	                         1,
	                         NULL);
	if (err != SR_ERR_OK) {
		cli_log("cannot setup repo context options: %s",
		        sr_strerror(err));
		return err;
	}
#endif /* defined(CONFIG_CLI_DEBUG) */

	err = sr_connect(SR_CONN_DEFAULT, &context->conn);
	if (err != SR_ERR_OK) {
		cli_log("cannot open repo connection: %s",
		        sr_strerror(err));
		return err;
	}

	err = sr_session_start(context->conn,
	                       SR_DS_OPERATIONAL,
	                       &context->sess);
	if (err != SR_ERR_OK) {
		cli_log("cannot start repo session: %s",
		        sr_strerror(err));
		goto disconnect;
	}

	context->lyctx = sr_session_acquire_context(context->sess);
	assert(context->lyctx);

	err = ly_out_new_file(stdout, &context->lyout);
	if (err != LY_SUCCESS) {
		cli_log("cannot open yang output printer: %s",
		        ly_strerr(err));
		err = SR_ERR_LY;
		goto release;
	}

	context->wkcnt = 0;
	cli_dir_init_root(&context->root);
	context->cwd = &context->root;
	context->isatty = !!isatty(STDOUT_FILENO);

	return SR_ERR_OK;

release:
	sr_session_release_context(context->sess);
	sr_session_stop(context->sess);
disconnect:
	sr_disconnect(context->conn);

	return err;
}

static int
cli_visitn_destroy_dir(struct cli_dir *    directory,
                       enum cli_walk_event event,
                       void *              data __cli_unused)
{
	if (event == CLI_WALK_POST_EVT)
		cli_dir_destroy(directory);

	return CLI_WALK_CONT_RET;
}

static void
cli_fini_context(struct cli_context * context)
{
	cli_assert_context(context);

	cli_release_workq(context);
	cli_dir_walk_safe(&context->root, cli_visitn_destroy_dir, NULL);
	cli_dir_fini_root(&context->root);
	ly_out_free(context->lyout, NULL, 0);
	sr_session_release_context(context->sess);
	sr_session_stop(context->sess);
	sr_disconnect(context->conn);
}

/******************************************************************************
 * Top-level logic
 ******************************************************************************/

struct cli_tree_builder {
	struct cli_dir * parent;
};

static int
cli_build_tree_dir(struct cli_context * context,
                   struct lysc_node *   node,
                   enum cli_walk_event  event,
                   void *               data)
{
	cli_assert_context(context);
	cli_assert(node);
	cli_assert((event == CLI_WALK_PRE_EVT) || (event == CLI_WALK_POST_EVT));

	struct cli_tree_builder * build = data;

	cli_assert(build);
	cli_assert(build->parent);

	switch (node->nodetype) {
	case LYS_CONTAINER:
	case LYS_LIST:
		if (event == CLI_WALK_PRE_EVT) {
			struct cli_dir * dir;

			dir = cli_dir_create_node(node->name, node);
			if (!dir) {
				char * xpath;

				xpath = cli_lysc_node_xpath(node);
				cli_log("'%s': "
				        "cannot create node directory entry.",
				        xpath);
				cli_free(xpath);

				return -ENAMETOOLONG;
			}

			cli_dir_add_child(build->parent, dir);

			build->parent = dir;
		}
		else if (event == CLI_WALK_POST_EVT)
			build->parent = build->parent->parent;

		break;

	default:
#if defined(CONFIG_CLI_DEBUG)
		if (event == CLI_WALK_PRE_EVT) {
			char * xpath;

			xpath = cli_lysc_node_xpath(node);
			cli_log("'%s': %s support not implemented !",
			        xpath,
			        cli_ly_nodetype_str(node->nodetype));
			cli_free(xpath);
		}
#endif /* defined(CONFIG_CLI_DEBUG) */
	}

	return CLI_WALK_CONT_RET;
}

static int
cli_init(struct cli_context * context, bool history)
{
	cli_assert(context);

	int                       ret;
	unsigned int              m;
	const struct lys_module * mod;
	struct cli_tree_builder   build;

	ret = cli_init_context(context);
	if (ret)
		return ret;

	cli_list_build_cmd(&context->root);
	cli_chdir_build_cmd(&context->root);
	cli_pwd_build_cmd(&context->root);
	cli_find_build_cmd(&context->root);
	cli_xpath_build_cmd(&context->root);
	cli_schema_build_cmd(&context->root);

	cli_lys_foreach_module(context, m, mod) {
		struct cli_dir * dir;

		dir = cli_dir_create_module(mod->name, mod);
		if (!dir) {
			char * xpath;

			xpath = cli_lys_module_xpath(mod);
			cli_log("'%s': cannot create module directory entry.",
			        xpath);
			cli_free(xpath);

			ret = -ENOTSUP;
			goto fini;
		}

		cli_dir_add_child(&context->root, dir);

		build.parent = dir;
		ret = cli_lys_walk_module(context,
		                          mod,
		                          cli_build_tree_dir,
		                          &build);
		if (ret)
			goto fini;
	}

	ret = cli_shell_init(&context->shell, history);
	if (ret)
		goto fini;

	return 0;

fini:
	cli_fini_context(context);

	return ret;
}

static void
cli_fini(struct cli_context * context)
{
	cli_shell_fini(&context->shell);
	cli_fini_context(context);
}

int
main(int argc, const char * const argv[])
{
	struct cli_context ctx;
	int                ret;

	if (argc < 2) {
		cli_log("invalid arguments.");
		return EXIT_FAILURE;
	}

	ret = cli_init(&ctx, true);
	if (ret)
		goto out;

	ret = cli_parse(&ctx, argc - 1, &argv[1]);
	if (ret)
		goto fini;

	ret = cli_exec_workq(&ctx);

fini:
	cli_fini(&ctx);
out:
	return (!ret) ? EXIT_SUCCESS : EXIT_FAILURE;
}
