#include "yang.h"
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include <assert.h>

/******************************************************************************
 * Utilities
 ******************************************************************************/

#if defined(CONFIG_CLI_ASSERT)

bool
cli_ischr_valid(int chr)
{
	return isalnum(chr) || ispunct(chr) || isblank(chr) || (chr == '\n');
}

void
cli_assert_args(int argc, const char * const argv[])
{
	cli_assert(argc);
	cli_assert(argv);

	int    a;
	size_t len;

	for (a = 0, len = 0; a < argc; a++) {
		cli_assert(argv[a]);

		size_t       alen = strnlen(argv[a], CLI_LINE_MAX);
		unsigned int c;

		len += alen;
		cli_assert(len < CLI_LINE_MAX);

		for (c = 0; c < alen; c++)
			cli_assert(cli_ischr_valid(argv[a][c]));
	}

	cli_assert(argv[argc] == NULL);
}

#else  /* !defined(CONFIG_CLI_ASSERT) */

static inline void
cli_assert_args(int argc, const char * const argv[])
{
}

#endif /* defined(CONFIG_CLI_ASSERT) */

/******************************************************************************
 * Overall cli context handling
 ******************************************************************************/

static int
cli_sched_work(struct cli_context * context, struct cli_work * work)
{
	cli_assert_context(context);
	cli_assert_work(work);

	if (context->wkcnt >= CLI_WORK_NR)
		return -EBUSY;

	context->wkq[context->wkcnt++] = work;

	return 0;
}

static void
cli_release_workq(struct cli_context * context)
{
	cli_assert_context(context);

	unsigned int w;

	for (w = 0; w < context->wkcnt; w++)
		cli_release_work(context->wkq[w], context);

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

static int
cli_parse_root(const struct cli_node * node,
               int                     argc,
               const char * const      argv[],
               struct cli_context *    context)
{
	struct cli_node * child;
	int               ret;

	cli_foreach_child(node, child) {
		ret = cli_parse_node(child, argc, argv, context);
		if (ret)
			break;
	}

	if (ret < 0)
		return ret;

	if (ret != argc) {
		cli_log("'%s': invalid command.", argv[0]);
		return -EINVAL;
	}

	return 0;
}

static const struct cli_node_ops cli_root_ops = {
	.parse   = cli_parse_root,
	.release = cli_release_node_null
};

static int
cli_parse(struct cli_context * context, int argc, const char * const argv[])
{
	cli_assert_context(context);

	return cli_parse_node(&context->root, argc, argv, context);
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

	err = sr_connect(SR_CONN_DEFAULT, &context->conn);
	if (err != SR_ERR_OK) {
		cli_log("cannot open repo connection: %s",
		        sr_strerror(err));
		return err;
	}

	err = sr_session_start(context->conn, SR_DS_OPERATIONAL, &context->sess);
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
	cli_setup_node(&context->root, &cli_root_ops);
	context->select = NULL;
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
cli_visitn_destroy_node(struct cli_context * context,
                        struct cli_node *    node,
                        enum cli_walk_event  event,
                        void *               data __cli_unused)
{
	if (event == CLI_WALK_POST_EVT)
		cli_release_node(node, context);

	return CLI_WALK_CONT_RET;
}

static void
cli_fini_context(struct cli_context * context)
{
	cli_assert_context(context);

	cli_lyd_unload(context->select);
	cli_release_workq(context);
	cli_walk_node_safe(context,
	                   &context->root,
	                   cli_visitn_destroy_node,
	                   NULL);
	ly_out_free(context->lyout, NULL, 0);
	sr_session_release_context(context->sess);
	sr_session_stop(context->sess);
	sr_disconnect(context->conn);
}

/******************************************************************************
 * Top-level logic
 ******************************************************************************/

struct cli_container_cmd {
	struct cli_node          super;
	const struct lysc_node * lysc;
};

static struct cli_container_cmd *
cli_create_container_cmd(struct cli_context *        context,
                         struct lysc_node *          node,
                         const struct cli_node_ops * ops,
                         struct cli_node *           parent)
{
	cli_assert_context(context);
	cli_assert(!(ly_ctx_get_options(context->lyctx) &
	             LY_CTX_SET_PRIV_PARSED));

	struct cli_container_cmd * cmd;

	cmd = (struct cli_container_cmd *)cli_create_node(sizeof(*cmd), ops);
	cmd->lysc = node;
	cli_node_add_child(parent, &cmd->super);

	return cmd;
}

struct cli_list_work {
	struct cli_work super;
	char *          xpath;
};

static int
cli_list_exec_work(const struct cli_work * work, struct cli_context * context)
{
	const struct cli_list_work * wk = (const struct cli_list_work *)work;
	int                          ret;
	sr_data_t                  * data;
	const struct lyd_node *      node;

	ret = cli_lyd_load(context, wk->xpath, 1, &data);
	if (ret != SR_ERR_OK) {
		cli_log("list: '%s': %s.", wk->xpath, sr_strerror(ret));
		return ret;
	}

	cli_lyd_foreach(data, node)
		printf("%s\n", node->schema->name);

	cli_lyd_unload(data);

	return 0;
}

static void
cli_list_release_work(struct cli_work *    work,
                      struct cli_context * context __cli_unused)
{
	cli_free(((struct cli_list_work *)work)->xpath);
	cli_destroy_work(work);
}

static const struct cli_work_ops cli_list_work_ops = {
	.exec    = cli_list_exec_work,
	.release = cli_list_release_work
};

static int
cli_list_sched_work(char * xpath, struct cli_context * context)
{
	struct cli_list_work * wk;

	wk = (struct cli_list_work *)cli_create_work(sizeof(*wk),
	                                             &cli_list_work_ops);
	wk->xpath = xpath;

	return cli_sched_work(context, &wk->super);
}

static int
cli_list_parse_cmd(const struct cli_node * node,
                   int                     argc,
                   const char * const      argv[],
                   struct cli_context *    context)
{
	cli_assert_node(node);
	cli_assert_args(argc, argv);
	cli_assert_context(context);

	if (!strcmp(argv[0], "list")) {
		char * xpath;
		int    ret;

		xpath = cli_lysc_xpath(((const struct cli_container_cmd *)
		                        node)->lysc);
		cli_assert(xpath);

		ret = cli_list_sched_work(xpath, context);
		if (!ret) {
			/*
			 * Success: tell the caller that we consummed 1
			 * argument from the command line.
			 */
			return 1;
		}

		cli_free(xpath);

		return ret;
	}
	else {
		/*
		 * We are not concerned with this command: tell the caller that
		 * we consummed no argument from the command line.
		 */
		return 0;
	}
}

static const struct cli_node_ops cli_list_cmd = {
	.parse   = cli_list_parse_cmd,
	.release = cli_releasen_destroy_node
};

struct cli_cmd_generator_context {
	struct cli_node * parent;
};

static int
cli_generate_cmd(struct cli_context * context,
                 struct lysc_node *   node,
                 enum cli_walk_event  event,
                 void *               data)
{
#warning FIXME
	return CLI_WALK_SKIP_RET;

	struct cli_cmd_generator_context * gen = data;

	switch (event) {
	case CLI_WALK_PRE_EVT:
		break;
	case CLI_WALK_POST_EVT:
		gen->parent = gen->parent->parent;
		return CLI_WALK_CONT_RET;
	default:
		cli_assert(0);
	}

	switch (node->nodetype) {
	case LYS_CONTAINER:
#warning FIXME and adapt CLI_WALK_POST_EVT event processing accordingly
		/* gen->parent =*/ cli_create_container_cmd(context,
		                                       node,
		                                       &cli_list_cmd,
		                                       gen->parent);
		return CLI_WALK_SKIP_RET;

	default:
		{
			const char * xpath;

			xpath = cli_lysc_xpath(node);
			cli_log("'%s': %s support not implemented !",
			        xpath,
			        cli_ly_nodetype_str(node->nodetype));
			/* cli_free(xpath); */
			cli_assert(0);
		}
	}
}

/******************************************************************************/
/******************************************************************************/
/******************************************************************************/

static int
cli_list_module_exec(const struct cli_work * work, struct cli_context * context)
{
	unsigned int              m;
	const struct lys_module * mod;

	cli_lys_foreach_module(context, m, mod)
		printf("%s\n", mod->name);

	return 0;
}

static const struct cli_work_ops cli_list_module_work_ops = {
	.exec    = cli_list_module_exec,
	.release = cli_releasen_destroy_work
};

static int
cli_list_module_parse(const struct cli_node * node,
                      int                     argc,
                      const char * const      argv[],
                      struct cli_context *    context)
{
	cli_assert_node(node);
	cli_assert_args(argc, argv);
	cli_assert_context(context);

	if (!strcmp(argv[0], "list")) {
		struct cli_work * wk;
		int               ret;

		wk = cli_create_work(sizeof(*wk), &cli_list_module_work_ops);
		ret = cli_sched_work(context, wk);

		return (!ret) ? 1 : ret;
	}
	else {
		/*
		 * We are not concerned with this command: tell the caller that
		 * we consummed no argument from the command line.
		 */
		return 0;
	}
}

static const struct cli_node_ops cli_list_module_cmd_ops= {
	.parse =   cli_list_module_parse,
	.release = cli_release_node_null
};

static struct cli_node
cli_list_module_cmd = CLI_NODE_SETUP(cli_list_module_cmd,
                                     &cli_list_module_cmd_ops);

/******************************************************************************/
/******************************************************************************/
/******************************************************************************/


static int
cli_init(struct cli_context * context)
{
	int                       ret;
	unsigned int              m;
	const struct lys_module * mod;

	ret = cli_init_context(context);
	if (ret)
		return ret;

	cli_node_add_child(&context->root, &cli_xpath_cmd);
	cli_node_add_child(&context->root, &cli_schema_cmd);
	cli_node_add_child(&context->root, &cli_list_module_cmd);

	cli_lys_foreach_module(context, m, mod) {
		struct cli_cmd_generator_context gen = {
			.parent = &context->root
		};

		ret = cli_lys_walk_module(context,
		                          mod,
		                          cli_generate_cmd,
		                          &gen);
		if (ret)
			return ret;
	}

	return 0;
}

static void
cli_fini(struct cli_context * context)
{
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

	ret = cli_init(&ctx);
	if (ret)
		goto fini;

	ret = cli_parse(&ctx, argc - 1, &argv[1]);
	if (ret)
		goto fini;

	ret = cli_exec_workq(&ctx);

fini:
	cli_fini(&ctx);

	return (!ret) ? EXIT_SUCCESS : EXIT_FAILURE;
}
