#include "yang.h"

/******************************************************************************
 * Libyang utils
 ******************************************************************************/

const char *
cli_ly_basetype_str(LY_DATA_TYPE basetype)
{
	switch (basetype) {
	case LY_TYPE_BINARY:
		return "bin";
	case LY_TYPE_UINT8:
		return "uint8";
	case LY_TYPE_UINT16:
		return "uint16";
	case LY_TYPE_UINT32:
		return "uint32";
	case LY_TYPE_UINT64:
		return "uint64";
	case LY_TYPE_STRING:
		return "str";
	case LY_TYPE_BITS:
		return "bits";
	case LY_TYPE_BOOL:
		return "bool";
	case LY_TYPE_DEC64:
		return "dec64";
	case LY_TYPE_EMPTY:
		return "empty";
	case LY_TYPE_ENUM:
		return "enum";
	case LY_TYPE_IDENT:
		return "idref";
	case LY_TYPE_INST:
		return "instid";
	case LY_TYPE_LEAFREF:
		return "leafref";
	case LY_TYPE_UNION:
		return "union";
	case LY_TYPE_INT8:
		return "int8";
	case LY_TYPE_INT16:
		return "int16";
	case LY_TYPE_INT32:
		return "int32";
	case LY_TYPE_INT64:
		return "int64";
	default:
		return "??";
	}
}

/******************************************************************************
 * Libyang module handling
 ******************************************************************************/

char *
cli_lys_module_xpath(const struct lys_module * module)
{
	char * xpath;

	if (asprintf(&xpath, "/%s", module->name) < 0)
		/* Memory allocation failure... */
		abort();

	return xpath;
}

const struct lys_module *
cli_lys_find_module(const struct cli_context * context, const char * module)
{
	cli_assert_context(context);
	cli_assert(module);

	const struct lys_module * mod;

	mod = ly_ctx_get_module_implemented(context->lyctx, module);
	if (mod && !sr_is_module_internal(mod))
		return mod;

	return NULL;
}

const struct lys_module *
cli_lys_next_module(const struct cli_context * context, unsigned int * index)
{
	const struct lys_module * mod;

	mod = ly_ctx_get_module_iter(context->lyctx, index);
	while (mod) {
		if (mod->implemented &&
		    mod->compiled->data &&
		    !sr_is_module_internal(mod)) {
			/*
			 * Return external implemented modules that hold
			 * top-level data node(s).
			 */
			break;
		}

		mod = ly_ctx_get_module_iter(context->lyctx, index);
	}

	return mod;
}

int
cli_lys_walk_module(struct cli_context *      context,
                    const struct lys_module * module,
                    cli_lysc_visit_fn *       visit,
                    void *                    data)
{
	cli_assert_context(context);
	cli_assert(module);
	cli_assert(module->compiled);
	cli_assert(visit);

	const struct lysc_node * root;
	int                      ret = 0;

	/*
	 * Iterate over schema nodes only, i.e., not actions / rpcs, neither
	 * notifications (which may be reached thanks to the parent container
	 * lysc_node).
	 */
	LY_LIST_FOR(module->compiled->data, root) {
		ret = cli_lysc_walk_node(context, root, visit, data);
		if (ret < 0)
			return ret;
	}

	cli_assert(!ret);
	return 0;
}

LY_ERR
cli_lys_print_module_diag(const struct cli_context * context,
                          const struct lys_module *  module)
{
	/*
	 * Note: the LYS_PRINT_NO_SUBSTMT option is ignored when outputting
	 * module YANG tree diagram.
	 */
	return lys_print_module(context->lyout,
	                        module,
	                        LYS_OUT_TREE,
	                        cli_term_cols(context),
	                        0);
}

/******************************************************************************
 * Libyang (compiled) schema node handling
 ******************************************************************************/

char *
cli_lysc_node_xpath(const struct lysc_node * node)
{
	char * xpath;

	xpath = lysc_path(node, LYSC_PATH_DATA, NULL, 0);
	if (!xpath)
		abort();

	return xpath;
}

#warning Remove recursion
int
cli_lysc_walk_node(struct cli_context *     context,
                   const struct lysc_node * node,
                   cli_lysc_visit_fn *      visit,
                   void *                   data)
{
	cli_assert_context(context);
	cli_assert(node);
	cli_assert(visit);

	int ret;

	ret = visit(context, node, CLI_WALK_PRE_EVT, data);
	if (ret == CLI_WALK_CONT_RET) {
		const struct lysc_node * child;

		LY_LIST_FOR(cli_lysc_child(node), child) {
			ret = cli_lysc_walk_node(context, child, visit, data);
			if (ret < 0)
				return ret;
		}

		ret = visit(context, node, CLI_WALK_POST_EVT, data);
		cli_assert(ret <= 0);
	}

	return (ret >= 0) ? 0 : ret;
}

LY_ERR
cli_lysc_print_nodeset_yang(const struct cli_context * context,
                            const struct ly_set *      nodeset,
                            bool                       nosub)
{
	cli_assert(nodeset);
	cli_assert(nodeset->count);

	unsigned int n;

	for (n = 0; n < nodeset->count; n++) {
		int ret;

		/*
		 * Note: line width argument is ignored when outputting node
		 * YANG specification.
		 */
		ret = cli_lysc_print_node_yang(context,
		                               nodeset->snodes[n],
		                               nosub);
		if (ret != LY_SUCCESS)
			return ret;
	}

	return LY_SUCCESS;
}

LY_ERR
cli_lysc_print_nodeset_diag(const struct cli_context * context,
                            const struct ly_set *      nodeset)
{
	cli_assert(nodeset);
	cli_assert(nodeset->count);

	unsigned int n;

	for (n = 0; n < nodeset->count; n++) {
		int ret;

		/*
		 * Note: line width argument is ignored when outputting node
		 * YANG specification.
		 */
		ret = cli_lysc_print_node_diag(context, nodeset->snodes[n]);
		if (ret != LY_SUCCESS)
			return ret;
	}

	return LY_SUCCESS;
}

/******************************************************************************
 * Libyang data handling
 ******************************************************************************/

int
cli_lyd_load(const struct cli_context * context,
             const char *               xpath,
             unsigned int               depth,
             sr_get_oper_flag_t         flags,
             sr_data_t **               data)
{
	cli_assert_context(context);
	cli_assert(xpath);
	cli_assert(xpath[0]);
	cli_assert(strnlen(xpath, CLI_XPATH_MAX) < CLI_XPATH_MAX);
	cli_assert(data);

	int err;

	err = sr_get_data(context->sess,
	                  xpath,
	                  depth,
	                  0,
	                  flags,
	                  data);
	if (err != SR_ERR_OK)
		return err;

	if (!*data)
		return SR_ERR_NOT_FOUND;

	if (!(*data)->tree) {
		sr_release_data(*data);
		*data = NULL;
		return SR_ERR_NOT_FOUND;
	}

	return SR_ERR_OK;
}

int
cli_lyd_load_from_schema(const struct cli_context * context,
                         const struct lysc_node *   node,
                         unsigned int               depth,
                         sr_get_oper_flag_t         flags,
                         sr_data_t **               data)
{
	cli_assert_context(context);
	cli_assert(node);
	cli_assert(data);

	char * xpath;
	int    ret;

	xpath = cli_lysc_node_xpath(node);
	cli_assert(xpath);

	ret = cli_lyd_load(context, xpath, depth, flags, data);

	cli_free(xpath);

	return ret;
}

int
cli_lyd_load_node(const struct cli_context * context,
                  const char *               xpath,
                  sr_data_t **               data)
{
	cli_assert_context(context);
	cli_assert(xpath);
	cli_assert(xpath[0]);
	cli_assert(strnlen(xpath, CLI_XPATH_MAX) < CLI_XPATH_MAX);
	cli_assert(data);

	int err;

	err = sr_get_node(context->sess, xpath, 0, data);
	if (err != SR_ERR_OK)
		return err;

	cli_assert(*data);
	cli_assert((*data)->tree);

	return SR_ERR_OK;
}

int
cli_lyd_load_node_from_schema(const struct cli_context * context,
                              const struct lysc_node *   node,
                              sr_data_t **               data)
{
	cli_assert_context(context);
	cli_assert(node);
	cli_assert(data);

	char * xpath;
	int    ret;

	xpath = cli_lysc_node_xpath(node);
	cli_assert(xpath);

	ret = cli_lyd_load_node(context, xpath, data);

	cli_free(xpath);

	return ret;
}
