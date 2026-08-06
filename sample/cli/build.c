#include "build.h"
#include "cli.h"
#include "config.h"
#include "status.h"
#include "yang.h"

struct cli_tree_build {
	struct cli_dir * parent;
};

#define CLI_TREE_BUILD_SETUP(_dir) \
	{ .parent = _dir }

static int
cli_build_handle_container(const struct lysc_node * node,
                           struct cli_tree_build *  build)
{
#warning Implement me!!
#if 0
	const struct lysc_ext_instance * ext;
	int                              err;
	const char *                     msg;

#warning What to do when multiple cliext statement are there ?!!!
	cli_lysc_foreach_extension(node->exts, ext) {
		if (cli_lysc_is_extension(ext, "mkdir")) {
			err = cli_dir_make(&build->parent,
			                   ext->argument,
			                   CLI_DIR_NODE_TYPE,
			                   node);
			if (err) {
				msg = "cannot create directory entry";
				goto err;
			}
		}
		else if (cli_lysc_is_extension(ext, "dirref")) {
			err = cli_dir_search((const struct cli_dir **)
			                     &build->parent,
			                     ext->argument);
			if (err) {
				msg = "cannot find directory entry";
				goto err;
			}
		}
	}

	return 0;

err:
	{
		char * xpath;

		xpath = cli_lysc_node_xpath(node);
		cli_log("'%s': %s: %s.", xpath, msg, cli_dir_strerror(-err));
		cli_free(xpath);
	}

	return err;

#warning What to do when no cliext statement is there ?!!!
#else
	struct cli_dir * dir;

	dir = cli_dir_create_node(node->name, node);
	if (!dir) {
		char * xpath;

		xpath = cli_lysc_node_xpath(node);
		cli_log("'%s': cannot create directory entry.", xpath);
		cli_free(xpath);

		return -ENAMETOOLONG;
	}

	cli_dir_add_child(build->parent, dir);

	build->parent = dir;

	return 0;
#endif
}

static int
cli_build_handle_leaf(struct cli_context *          context,
                      const struct lysc_node *      node,
                      const struct cli_tree_build * build)
{
	const struct lysc_node_leaf * leaf = (const struct lysc_node_leaf *)
	                                     node;

	cli_config_make_cmd(build->parent, leaf, context);
	cli_status_make_cmd(build->parent, leaf, context);

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

	struct cli_tree_build * build = data;
	int                     ret;

	cli_assert(build);
	cli_assert(build->parent);

	switch (node->nodetype) {
	case LYS_CONTAINER:
	case LYS_LIST:
		if (event == CLI_WALK_PRE_EVT) {
			ret = cli_build_handle_container(node, build);
			if (ret)
				return ret;
		}
		else if (event == CLI_WALK_POST_EVT)
			build->parent = build->parent->parent;

		break;

	case LYS_LEAF:
		if (event == CLI_WALK_PRE_EVT) {
			ret = cli_build_handle_leaf(context, node, build);
			if (ret)
				return ret;

			return CLI_WALK_SKIP_RET;
		}

		break;

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

static int
cli_build_handle_module(struct cli_context *      context,
                        const struct lys_module * module)
{
	const struct lysc_module *       scm = module->compiled;
	const struct lysc_ext_instance * ext;
	int                              err;
	struct cli_tree_build            build =
		CLI_TREE_BUILD_SETUP(&context->root);

	cli_lysc_foreach_extension(scm->exts, ext) {
		if (cli_lysc_is_extension(ext, "mkdir")) {
			err = cli_build_mkdir(&build.parent,
			                      ext->argument,
			                      module);
			if (err)
				return err;
		}
	}

	if (scm->data) {
		err = cli_build_mkdir(&build.parent, module->name, module);
		if (err)
			return err;

		err = cli_lys_walk_module(context,
		                          module,
		                          cli_build_tree_dir,
		                          &build);
		if (err)
			return err;
	}

	return 0;
}

int
cli_build_from_schema(struct cli_context * context)
{
	unsigned int              m;
	const struct lys_module * mod;

	cli_lys_foreach_module(context, m, mod) {
		int ret;

		ret = cli_build_handle_module(context, mod);
		if (ret)
			return ret;
	}

	return 0;
}
