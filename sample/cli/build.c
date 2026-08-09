#include "build.h"
#include "cli.h"
#include "show.h"
#include "yang.h"

static int
cli_build_handle_container(struct cli_context *       context,
                           const struct lysc_node *   node,
                           const struct lys_module *  module)
{
	struct cli_dir * dir = NULL;
	int              err;

#if 0
	const struct lysc_ext_instance * ext;

#warning What to do when multiple cliext statement are there ?!!!
	cli_lysc_foreach_extension(node->exts, ext) {
		if (cli_lysc_is_extension(ext, "dirref")) {
			dir = build->parent;
			err = cli_dir_search((const struct cli_dir **)&dir,
			                     ext->argument);
			if (err) {
				msg = "cannot find extension directory entry";
				path = ext->argument;
				goto err;
			}

			break;
		}

		if (cli_lysc_is_extension(ext, "mkdir")) {
			dir = build->parent;
			err = cli_dir_make(&dir,
			                   ext->argument,
			                   CLI_DIR_NODE_TYPE,
			                   node);
			if (err) {
				msg = "cannot create extension directory entry";
				path = ext->argument;
				goto err;
			}

			break;
		}
	}

#warning What to do when no cliext statement is there ?!!!
	if (!dir) {
		dir = cli_dir_create_node(node->name, node);
		if (!dir) {
			msg = "cannot create directory entry.";
			path = node->name;
			err = -errno;
			goto err;
		}

		cli_dir_add_child(build->parent, dir);
	}
#endif

	dir = cli_dir_make_from_node(node, &context->root, module);
	if (!dir) {
		err = errno;
		cli_lysc_log(node,
		             "cannot create directory entry: %s.",
		             cli_dir_strerror(err));
		return -err;
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
}

static int
cli_build_tree_dir(struct cli_context *     context,
                   const struct lysc_node * node,
                   enum cli_walk_event      event,
                   void *                   data)
{
	cli_assert_context(context);
	cli_assert(node);
	cli_assert((event == CLI_WALK_PRE_EVT) || (event == CLI_WALK_POST_EVT));
	cli_assert(data);

	const struct lys_module * mod = data;
	int                       ret;

	switch (node->nodetype) {
	case LYS_CONTAINER:
		if (event == CLI_WALK_PRE_EVT) {
			ret = cli_build_handle_container(context, node, mod);
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
			                          (void *)mod);
			if (ret)
				return ret;
		}
	}

	return 0;
}
