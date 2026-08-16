#include "build.h"
#include "cli.h"
#include "show.h"
#include "yang.h"

static int
cli_build_ext_cmd(const struct cli_context *  context,
                  struct cli_dir *            directory,
                  const struct lysc_node *    node,
                  const struct lysc_cly_cmd * command)
{
	cli_assert_context(context);
	cli_dir_assert(directory);
	cli_assert(node);
	cli_assert(command);
	cli_assert(command->name);

	int ret;

	switch (command->kind) {
	case LYSC_CLY_SHOW_CONFIG_CMD_KIND:
		ret = cli_show_make_config_cmd(
			directory,
			(const struct lysc_node_container *)node,
			command->name,
			context);
		break;

	case LYSC_CLY_SHOW_STATE_CMD_KIND:
		ret = cli_show_make_oper_cmd(
			directory,
			(const struct lysc_node_container *)node,
			command->name,
			context);
		break;

	case LYSC_CLY_SET_CONFIG_CMD_KIND:
		/* Implement me ! */
		assert(0);

	default:
		assert(0);
	}

	return ret;
}

static int
cli_build_handle_container(struct cli_context *     context,
                           const struct lysc_node * node)
{
	struct lysc_cly          cly;
	ly_bool                  has_cly;
	const struct lysc_node * menu = NULL;
	struct cli_dir *         dir;
	int                      err;

	/* Probe for YANG cly extension set. */
	err = lysc_cly_load(node, &cly);
	if (err != LY_SUCCESS) {
		cli_lysc_log(node,
		             "cannot process menu entry: %s.",
		             ly_strerr(err));
		return -EBADR;
	}

	has_cly = lysc_cly_exists(&cly);
	if (has_cly) {
		/* A set of YANG cly extensions is present... */

		if (lysc_cly_is_ignored(&cly))
			/* ...but it has been requested to be ignored. */
			return 0;

		/*
		 * Retrieve menu node: subsequent commands will be attached
		 * to it.
		 */
		menu = lysc_cly_get_menu(&cly);
		cli_assert(menu);
	}

	if (!menu) {
		/*
		 * Create a menu directory hierarchy described by the node's
		 * YANG path rooted under its module related directory menu
		 * entry.
		 */
		struct cli_dir * root = &context->root;

		/*
		 * Create the top-level directory related to the module owning
		 * the node.
		 */
		err = cli_dir_make_child(&root,
		                         node->module->name,
		                         CLI_DIR_MOD_TYPE,
		                         node->module);
		if (err)
			goto nodir;

		/*
		 * Then, create the menu directory entry hierarchy related to
		 * the node underneath.
		 */
		dir = cli_dir_make_from_node(node,
		                             root,
		                             CLI_DIR_NODE_TYPE,
		                             node);
	}
	else {
		/*
		 * Create the menu directory hierarchy as requested by the cly
		 * extensions under our root directory entry.
		 */
		dir = cli_dir_make_from_node(menu,
		                             &context->root,
		                             CLI_DIR_MOD_TYPE,
		                             node->module);
	}
	if (!dir) {
		err = -errno;
		goto nodir;
	}

	/*
	 * Eventually, create and attach commands to the menu directory entry
	 * just created.
	 */
	if (has_cly) {
		/*
		 * Instantiate commands defined by the YANG cly extensions
		 * found.
		 */
		const struct lysc_cly_cmd * cmd;

		lysc_cly_foreach_command(&cly, cmd) {
			err = cli_build_ext_cmd(context, dir, node, cmd);
			if (err)
				return err;
		}

		return 0;
	}

	err = cli_show_make_config_cmd(dir,
	                               (const struct lysc_node_container *)node,
	                               "config",
	                               context);
	if (err)
		return err;

	err = cli_show_make_oper_cmd(dir,
	                             (const struct lysc_node_container *)node,
	                             "status",
	                             context);
	if (err)
		return err;

	return 0;

nodir:
	cli_lysc_log(node,
	             "cannot create directory entry: %s.",
	             cli_dir_strerror(-err));

	return err;
}

static int
cli_build_tree_dir(struct cli_context *     context,
                   const struct lysc_node * node,
                   enum cli_walk_event      event,
                   void *                   data __cli_unused)
{
	cli_assert_context(context);
	cli_assert(node);
	cli_assert((event == CLI_WALK_PRE_EVT) || (event == CLI_WALK_POST_EVT));

	int ret;

	switch (node->nodetype) {
	case LYS_CONTAINER:
		if (event == CLI_WALK_PRE_EVT) {
			ret = cli_build_handle_container(context, node);
			if (ret)
				return ret;
		}

		break;

	case LYS_LEAF:
		return CLI_WALK_SKIP_RET;

	case LYS_LIST:
	case LYS_CHOICE:
	case LYS_LEAFLIST:
	case LYS_ANYXML:
	case LYS_ANYDATA:
	case LYS_CASE:
	case LYS_RPC:
	case LYS_ACTION:
	case LYS_NOTIF:
	case LYS_INPUT:
	case LYS_OUTPUT:

	case LYS_USES:
	case LYS_GROUPING:
	case LYS_AUGMENT:
	case LYS_UNKNOWN:
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
		return CLI_WALK_SKIP_RET;
	}

	return CLI_WALK_CONT_RET;
}

#if 0
static int
cli_build_mkdir(struct cli_dir **         parent,
                const char *              path,
                const struct lys_module * module)
{
	cli_assert(parent);
	cli_dir_assert(*parent);
	cli_assert(path);
	cli_assert(module);

	int err;

	err = cli_dir_make(parent, path, CLI_DIR_MOD_TYPE, module);
	if (err) {
		char * xpath;

		xpath = cli_lys_module_xpath(module);
		cli_log("'%s': cannot create module directory entry '%s': %s.",
		        xpath,
		        path,
		        strerror(-err));
		cli_free(xpath);

		return err;
	}

	return 0;
}
#endif

int
cli_build_from_schema(struct cli_context * context)
{
	unsigned int              m;
	const struct lys_module * mod;

	cli_lys_foreach_module(context, m, mod) {
		if (mod->compiled->data) {
			int ret;

			ret = cli_lys_walk_module(context,
			                          mod,
			                          cli_build_tree_dir,
			                          NULL);
			if (ret)
				return ret;
		}
	}

	return 0;
}
