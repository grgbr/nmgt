#include "cli.h"
#include "expr.h"
#include "yang.h"
#include "list.h"
#include "build.h"
#include "cd.h"
#include "pwd.h"
#include "find.h"
#include "xpath.h"
#include "schema.h"
#include "quit.h"
#include <sys/ioctl.h>

/******************************************************************************
 * Utilities
 ******************************************************************************/

#warning TODO: plug in a SIGWINCH signal handler

unsigned int
cli_term_cols(const struct cli_context * context)
{
	cli_assert_context(context);

	if (context->isatty) {
		int cols;

		rl_get_screen_size(NULL, &cols);
		cli_assert(cols > 0);

		return cols;
	}
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

static bool
cli_shell_ison(const struct cli_context * context)
{
	return context->interact;
}

void
cli_chdir(struct cli_context * context, const struct cli_dir * directory)
{
	cli_assert_context(context);
	cli_dir_assert(directory);

	if (cli_shell_ison(context)) {
		char *  path;
		ssize_t len;

		path = cli_malloc(CLI_PATH_MAX);
		cli_assert(path);
		len = cli_dir_mkabs(directory, path,  CLI_PATH_MAX);
		cli_assert(len > 0);

		cli_shell_set_prompt(path);

		cli_free(path);
	}

	context->cwd = directory;
}

static int
cli_parse(struct cli_context * context, int argc, const char * const argv[])
{
	cli_assert_context(context);
	cli_assert_args(argc, argv);

	int ret;

	ret = cli_dir_parse_cmd(context->cwd, context, argc, argv);
	if (!ret && (context->cwd != &context->root))
		ret = cli_dir_parse_cmd(&context->root, context, argc, argv);

	if (ret == argc)
		return 0;

	if (!ret) {
		cli_log("'%s': no such command.", argv[0]);
		return -EINVAL;
	}

	cli_assert(ret < 0);

	return ret;
}

static int
cli_parse_expr_blk(struct cli_context *        context,
                   const struct cli_expr_blk * expr_block)
{
	cli_assert_context(context);
	cli_assert(expr_block);

	const struct cli_expr * expr;

	cli_expr_blk_foreach(expr_block, expr) {
		int ret;

		ret = cli_parse(context,
		                cli_expr_arg_cnt(expr),
		                cli_expr_args(expr));
		if (ret)
			return ret;
	}

	return 0;
}

static void
cli_complete(struct cli_match * matches,
             const char *       word,
             size_t             length,
             int                argc,
             const char * const argv[],
             void *             data)
{
	cli_assert(matches);
	cli_assert(word);
	cli_assert(length < CLI_ARG_MAX);
	cli_assert(strnlen(word, CLI_ARG_MAX) == length);
	cli_assert(argc >= 0);
	cli_assert(!argc || argv);

	struct cli_context * ctx = data;

	cli_dir_complete_cmd(ctx->cwd,
	                     ctx,
	                     word,
	                     length,
	                     argc,
	                     argv,
	                     matches);
	if (ctx->cwd != &ctx->root)
		cli_dir_complete_cmd(&ctx->root,
		                     ctx,
		                     word,
		                     length,
		                     argc,
		                     argv,
		                     matches);
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
cli_init_context(struct cli_context * context, const char * const * style)
{
	cli_assert(context);
	cli_assert(style);

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
	context->colored = !!context->isatty;
	context->style = context->colored ? style : NULL;
	context->interact = false;

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

static const char * const cli_the_style[] = {
	[CLI_LABEL_STYLE_KIND]   = CLI_UNDERLINE_COLOR,
	[CLI_VALUE_STYLE_KIND]   = NULL,
	[CLI_DEFAULT_STYLE_KIND] = CLI_GRAY_COLOR,
	[CLI_ERROR_STYLE_KIND]   = CLI_BOLD_RED_COLOR
};

static int
cli_init(struct cli_context * context)
{
	cli_assert(context);

	int ret;

	ret = cli_init_context(context, cli_the_style);
	if (ret)
		return ret;

	cli_list_build_cmd(&context->root);
	cli_chdir_build_cmd(&context->root);
	cli_pwd_build_cmd(&context->root);
	cli_find_build_cmd(&context->root);
	cli_xpath_build_cmd(&context->root);
	cli_schema_build_cmd(&context->root);
	cli_quit_build_cmd(&context->root);

	ret = cli_build_from_schema(context);
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
	cli_fini_context(context);
}

int
main(int argc, const char * const argv[])
{
	cli_assert(argc == 1);
	cli_assert(argv);

	struct cli_context ctx;
	int                ret;

	ret = cli_init(&ctx);
	if (ret)
		goto out;

	if (argc == 1) {
		if (!ctx.isatty)
			/* Cannot run in interactive mode... */
			goto fini;

		ret = cli_shell_init(true, " \t;\n", cli_complete, &ctx);
		if (ret)
			goto fini_shell;

		ctx.interact = true;

		do {
			struct cli_expr_blk eblk = CLI_EXPR_BLK_INIT(eblk);

			ret = cli_shell_read_expr(&eblk);
			cli_assert(ret <= 0);
			switch (ret) {
			case 0:
				/* General expression syntax ok. */
				ret = cli_parse_expr_blk(&ctx, &eblk);
				if (!ret)
					ret = cli_exec_workq(&ctx);
				break;

			case -ESHUTDOWN:
				/* Shell shutdown requested. */
				break;

			case -ENODATA:
				/* Empty input. */
			case -EINVAL:
				/* Invalid expression argument character. */
			case -ENAMETOOLONG:
				/* Expression argument too long. */
			case -ENOBUFS:
				/* Too many expressions within block. */
			default:
				/* Other input line fetching / parsing error. */
				break;
			}

			/* Release expression block resources. */
			cli_expr_blk_fini(&eblk);
		} while (ret != -ESHUTDOWN);
		if (ret == -ESHUTDOWN)
			ret = 0;

fini_shell:
		cli_shell_fini();
	}
	else {
		ret = cli_parse(&ctx, argc - 1, &argv[1]);
		if (ret)
			goto fini;

		ret = cli_exec_workq(&ctx);
	}

fini:
	cli_fini(&ctx);
out:
	return (!ret) ? EXIT_SUCCESS : EXIT_FAILURE;
}
