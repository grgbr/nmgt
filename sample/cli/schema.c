#include "schema.h"
#include "yang.h"

/******************************************************************************
 * `schema' command handling.
 * Print schema for current working command line node.
 ******************************************************************************/

static int
cli_show_module_yang(struct cli_context * context,
                     const char *         module)
{
	const struct lys_module * mod;
	int                       ret;

	mod = cli_lys_find_module(context, module);
	if (!mod) {
		cli_log("'%s': module not found.", module);
		return SR_ERR_NOT_FOUND;
	}

	ret = cli_lys_print_module_yang(context, mod, 0);
	if (ret != LY_SUCCESS) {
		cli_log("'%s': cannot show module YANG: %s.",
		        module,
		        ly_strerr(ret));
		return SR_ERR_LY;
	}

	return SR_ERR_OK;
}

static int
cli_show_module_diag(struct cli_context * context, const char * module)
{
	const struct lys_module * mod;
	int                       ret;

	mod = cli_lys_find_module(context, module);
	if (!mod) {
		cli_log("'%s': invalid module.", module);
		return SR_ERR_NOT_FOUND;
	}

	ret = cli_lys_print_module_diag(context, mod);
	if (ret != LY_SUCCESS) {
		cli_log("'%s': cannot show module tree diagram: %s.",
		        module,
		        ly_strerr(ret));
		return SR_ERR_LY;
	}

	return SR_ERR_OK;
}

static int
cli_show_node_yang(struct cli_context *     context,
                   const struct lysc_node * subtree,
                   const char *             xpath)
{
	const struct lysc_node * node;
	int                      ret;

	node = cli_lysc_find_node(context, subtree, xpath);
	if (!node) {
		cli_log("'%s': XPATH node not found.", xpath);
		return SR_ERR_NOT_FOUND;
	}

	ret = cli_lysc_print_node_yang(context, node, 0);
	if (ret != LY_SUCCESS) {
		cli_log("'%s': cannot show XPATH node YANG: %s.",
		        xpath,
		        ly_strerr(ret));
		return SR_ERR_LY;
	}

	return SR_ERR_OK;
}

static int
cli_show_nodeset_yang(struct cli_context *     context,
                      const struct lysc_node * subtree,
                      const char *             xpath)
{
	struct ly_set * nodes;

	nodes = cli_lysc_find_nodeset(context, subtree, xpath);
	if (nodes) {
		cli_assert(nodes->count);

		int ret;

		ret = cli_lysc_print_nodeset_yang(context, nodes, 0);
		ly_set_free(nodes, NULL);

		if (ret == LY_SUCCESS)
			return SR_ERR_OK;

		cli_log("'%s': cannot show XPATH nodes YANG: %s.",
		        xpath,
		        ly_strerr(ret));
		return SR_ERR_LY;
	}
	else {
		cli_log("'%s': XPATH nodes not found.", xpath);
		return SR_ERR_NOT_FOUND;
	}
}

static int
cli_show_node_diag(struct cli_context *     context,
                   const struct lysc_node * subtree,
                   const char *             xpath)
{
	const struct lysc_node * node;
	int                      ret;

	node = cli_lysc_find_node(context, subtree, xpath);
	if (!node) {
		cli_log("'%s': XPATH node not found.", xpath);
		return SR_ERR_NOT_FOUND;
	}

	ret = cli_lysc_print_node_diag(context, node);
	if (ret != LY_SUCCESS) {
		cli_log("'%s': cannot show XPATH node tree diagram: %s.",
		        xpath,
		        ly_strerr(ret));
		return SR_ERR_LY;
	}

	return SR_ERR_OK;
}

static int
cli_show_nodeset_diag(struct cli_context *     context,
                      const struct lysc_node * subtree,
                      const char *             xpath)
{
	struct ly_set * nodes;

	nodes = cli_lysc_find_nodeset(context, subtree, xpath);
	if (nodes) {
		cli_assert(nodes->count);

		int ret;

		ret = cli_lysc_print_nodeset_diag(context, nodes);
		ly_set_free(nodes, NULL);

		if (ret == LY_SUCCESS)
			return SR_ERR_OK;

		cli_log("'%s': cannot show XPATH nodes tree diagram: %s.",
		        xpath,
		        ly_strerr(ret));
		return SR_ERR_LY;
	}
	else {
		cli_log("'%s': XPATH nodes not found.", xpath);
		return SR_ERR_NOT_FOUND;
	}
}

static int
cli_schema_exec_work(const struct cli_work * work,
                     struct cli_context *    context)
{
	int ret;

	//cli_show_node_yang(context, NULL, "/oven:oven-state");

	//cli_show_nodeset_yang(context, NULL, "/oven:*");
	// first unprefixed top-level container
	//cli_show_nodeset_yang(context, NULL, "/oven");
	//cli_show_nodeset_yang(context, NULL, "oven");

	//cli_show_node_diag(context, NULL, "/oven:oven-state");

	//cli_show_nodeset_diag(context, NULL, "/oven:*");
	// first unprefixed top-level container
	//cli_show_nodeset_diag(context, NULL, "/oven");
	//cli_show_nodeset_diag(context, NULL, "oven");

#if 0
	if (!context->select) {
		ret = cli_load_config(context, "/oven:oven-state/temperature", 0, &context->select);
		if (ret != SR_ERR_OK) {
			cli_log("schema: cannot load data: %s.",
			        sr_strerror(ret));
			return ret;
		}
#warning TODO: set prompt
	}

#warning FIXME (implemented / internal / out format)
#if 1
	const struct lyd_node * node;
	LY_LIST_FOR(context->select->tree, node) {
		ret = lys_print_node(context->lyout,
		                     node->schema,
		                     LYS_OUT_TREE,
		                     0,
		                     0/* LYS_PRINT_NO_SUBSTMT */);
	}
#else
	const struct lyd_node * node;
	LY_LIST_FOR(context->select->tree, node) {
		ret = lys_print_module(context->lyout,
		                       node->schema->module,
		                       LYS_OUT_TREE,
		                       0,
		                       0/* LYS_PRINT_NO_SUBSTMT */);
	}
#endif
#endif
	if (ret != LY_SUCCESS) {
		cli_log("schema: cannot display: %s.", ly_strerr(ret));
		return SR_ERR_LY;
	}

	return SR_ERR_OK;
}

static void
cli_schema_release_work(struct cli_work *    work,
                        struct cli_context * context __cli_unused)
{
	cli_destroy_work(work);
}

static const struct cli_work_ops cli_schema_work_ops = {
	.exec    = cli_schema_exec_work,
	.release = cli_releasen_destroy_work
};

static int
cli_schema_sched_work(struct cli_context * context)
{
	struct cli_work * wk;

	wk = cli_create_work(sizeof(*wk), &cli_schema_work_ops);

	return cli_sched_work(context, wk);
}

static int
cli_schema_parse_cmd(const struct cli_node * node,
                     int                     argc,
                     const char * const      argv[],
                     struct cli_context *    context)
{
	cli_assert_node(node);
	cli_assert_args(argc, argv);
	cli_assert_context(context);

	if (strcmp(argv[0], "schema"))
		return 0;

	if (argc == 1) {
		cli_schema_sched_work(context);

		return 1;
	}
	else
		cli_log("schema: too many argument.");

	return -EINVAL;
}

static const struct cli_node_ops cli_schema_cmd_ops = {
	.parse =   cli_schema_parse_cmd,
	.release = cli_release_node_null
};

static struct cli_node cli_schema_cmd = CLI_NODE_SETUP(cli_schema_cmd,
                                                       &cli_schema_cmd_ops);
