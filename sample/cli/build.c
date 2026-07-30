#include "build.h"
#include "cli.h"
#include "status.h"
#include "yang.h"

static int
cli_build_handle_leaf(struct cli_context *          context,
                      const struct lysc_node *      node,
                      const struct cli_tree_build * build)
{
	const struct lysc_node_leaf * leaf = (const struct lysc_node_leaf *)
	                                     node;

	cli_status_make_cmd(build->parent);

	return 0;
}

int
cli_build_tree_dir(struct cli_context * context,
                   struct lysc_node *   node,
                   enum cli_walk_event  event,
                   void *               data)
{
	cli_assert_context(context);
	cli_assert(node);
	cli_assert((event == CLI_WALK_PRE_EVT) || (event == CLI_WALK_POST_EVT));

	struct cli_tree_build * build = data;

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

	case LYS_LEAF:
		if (event == CLI_WALK_PRE_EVT) {
			int ret;

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
