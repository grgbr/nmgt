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
	struct cli_dir * dir;

	dir = cli_dir_create_node(node->name, node);
	if (!dir) {
		char * xpath;

		xpath = cli_lysc_node_xpath(node);
		cli_log("'%s': cannot create node directory entry.", xpath);
		cli_free(xpath);

		return -ENAMETOOLONG;
	}

	cli_dir_add_child(build->parent, dir);

	build->parent = dir;

	return 0;
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
cli_build_mkdir(const char *              name,
                struct cli_dir **         parent,
                const struct lys_module * module)
{
	cli_assert(name);
	cli_assert(parent);
	cli_dir_assert(*parent);
	cli_assert(module);

	struct cli_dir * dir;

	dir = cli_dir_create_module(name, module);
	if (!dir) {
		char * xpath;
		int    err = errno;

		xpath = cli_lys_module_xpath(module);
		cli_log("'%s': cannot create module directory entry '%s': %s.",
		        xpath,
		        name,
		        strerror(err));
		cli_free(xpath);

		return -err;
	}

	cli_dir_add_child(*parent, dir);

	*parent = dir;

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

	LY_ARRAY_FOR(scm->exts, typeof(*ext), ext) {
		cli_assert(ext->def);
		cli_assert(ext->def->module);

		const struct lysc_ext * def = ext->def;

		if (!strcmp(def->module->name, "cli-extensions") &&
		    !strcmp(def->module->ns, "urn:cli:yang:cli-extensions") &&
		    !(def->flags & LYS_STATUS_DEPRC)) {
			if (!strcmp(def->name, "mkdir") &&
			    (def->argname && !strcmp(def->argname, "path"))) {
				err = cli_build_mkdir(ext->argument,
				                      &build.parent,
				                      module);
				if (err)
					return err;
			}
		}
	}

	if (scm->data) {
		err = cli_build_mkdir(module->name, &build.parent, module);
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
