#define _GNU_SOURCE

#include <sysrepo.h>
#include <assert.h>

LY_ERR
clily_lysc_tree_dfs(const struct lysc_node * root,
                    lysc_dfs_clb             process_node,
                    void *                   data)
{
	assert(root);
	assert(process_node);

	struct lysc_node * node;

	LYSC_TREE_DFS_BEGIN(root, node) {
		LY_ERR err;

		err = process_node(node, data, &LYSC_TREE_DFS_continue);
		if (err != LY_SUCCESS)
			return err;

		LYSC_TREE_DFS_END(root, node);
	}

	return LY_SUCCESS;
}

LY_ERR
clily_lysc_module_dfs(const struct lys_module * module,
                      lysc_dfs_clb              process_node,
                      void *                    data)
{
	assert(module);
	assert(module->compiled);
	assert(process_node);

	const struct lysc_node * root;

	/*
	* Iterate over schema nodes only (i.e., not actions / rpcs, neither
	* notifications).
	*/
	LY_LIST_FOR(module->compiled->data, root) {
		LY_ERR err;

		err = clily_lysc_tree_dfs(root, process_node, data);
		if (err != LY_SUCCESS)
			return err;
	}

	return LY_SUCCESS;
}

/*
 * Iterate over YANG implemented modules, skipping sysrepo / libyang internal
 * ones.
 * Return compiled and (features) implemented, i.e. completely resolved modules
 * only.
 */
static const struct lys_module *
clily_get_module_iter(const struct ly_ctx * context, uint32_t * index)
{
	const struct lys_module * mod;

	mod = ly_ctx_get_module_iter(context, index);
	while (mod) {
		if (mod->implemented && !sr_is_module_internal(mod))
			break;
		mod = ly_ctx_get_module_iter(context, index);
	}

	return mod;
}

#define CLILY_FOREACH_MODULE(_context, _index, _module) \
	for ((_index) = 0, \
	     (_module) = ly_ctx_get_module_iter(_context, &_index); \
	     _module; \
	     (_module) = ly_ctx_get_module_iter(_context, &_index))

#define CLILY_FOREACH_MODULE_IMPL(_context, _index, _module) \
	for ((_index) = 0, \
	     (_module) = clily_get_module_iter(_context, &(_index)); \
	     _module; \
	     (_module) = clily_get_module_iter(_context, &(_index)))

static LY_ERR
clily_print_module_schema(const struct ly_ctx * context)
{
	struct ly_out *           out;
	LY_ERR                    ret;
	uint32_t                  m;
	const struct lys_module * mod;

	ret = ly_out_new_file(stdout, &out);
	if (ret != LY_SUCCESS)
		return ret;

	CLILY_FOREACH_MODULE_IMPL(context, m, mod) {
		ret = lys_print_module(out, mod, LYS_OUT_YANG_COMPILED, 0, 0);
		if (ret != LY_SUCCESS)
			break;
	}

	ly_out_free(out, NULL, 0);

	return ret;
}

static LY_ERR
clyli_print_command_clb(struct lysc_node * node, void * data, ly_bool * goon)
{
	switch (node->nodetype) {
	case LYS_CONTAINER:
	case LYS_CHOICE:
	case LYS_LEAF:
	case LYS_LEAFLIST:
	case LYS_LIST:
	case LYS_ANYXML:
	case LYS_ANYDATA:
	case LYS_CASE:

	case LYS_RPC:
	case LYS_ACTION:
	case LYS_NOTIF:

	case LYS_USES:
	case LYS_INPUT:
	case LYS_OUTPUT:
	case LYS_GROUPING:
	case LYS_AUGMENT:
		printf("%s\n", node->name);
		break;

	case LYS_UNKNOWN:
	default:
		assert(0);
	}

	return LY_SUCCESS;
}

static LY_ERR
clily_print_module_commands(const struct ly_ctx * context)
{
	uint32_t                  m;
	const struct lys_module * mod;

	CLILY_FOREACH_MODULE_IMPL(context, m, mod) {
		LY_ERR ret;

		ret = clily_lysc_module_dfs(mod, clyli_print_command_clb, NULL);
		if (ret != LY_SUCCESS)
			return ret;
	}

	return LY_SUCCESS;
}

static LY_ERR
clily_print_module_commands_full(const struct ly_ctx * context)
{
	uint32_t                  m;
	const struct lys_module * mod;

	CLILY_FOREACH_MODULE_IMPL(context, m, mod) {
		LY_ERR ret;

		ret = lysc_module_dfs_full(mod, clyli_print_command_clb, NULL);
		if (ret != LY_SUCCESS)
			return ret;
	}

	return LY_SUCCESS;
}

int
main(int argc, const char * const argv[])
{
	sr_conn_ctx_t *       conn;
	const struct ly_ctx * ctx;
	int                   ret;

	/* Connect to sysrepo. */
	ret = sr_connect(0, &conn);
	if (ret != SR_ERR_OK)
		return EXIT_FAILURE;

	/* Acquire libyang context. */
	ctx = sr_acquire_context(conn);
	assert(ctx);

	//ret = clily_print_module_schema(ctx);
	ret = clily_print_module_commands(ctx);
	//ret = clily_print_module_commands_full(ctx);

release_ctx:
	sr_release_context(conn);
disconnect:
	sr_disconnect(conn);

	return (ret == SR_ERR_OK) ? EXIT_SUCCESS : EXIT_FAILURE;
}
