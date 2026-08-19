#include "build.h"
#include "cli.h"
#include "show.h"
#include "yang.h"

/******************************************************************************
 * Command line hierarchy builder utilities.
 ******************************************************************************/

static bool
cli_build_node_outdated(const struct lysc_node * node)
{
	if (!(cli_lysc_status_flags(node) &
	      (LYS_STATUS_OBSLT | LYS_STATUS_DEPRC)))
		return false;

	/* Ignore obsolete and deprecated nodes. */
	cli_lysc_dbg(node, "obsolete / deprecated node ignored.");
	return true;
}

/******************************************************************************
 * Command line hierarchy build tracker.
 ******************************************************************************/

struct cli_build_stat;
struct cli_build;

typedef int cli_build_process_fn(struct cli_build_stat *,
                                 struct cli_context *,
                                 const struct lysc_node *,
                                 enum cli_walk_event,
                                 struct cli_build *);

struct cli_build_stat {
	cli_build_process_fn * process;
	struct cli_dir *       dir;
};

struct cli_build {
	unsigned int            cnt;
	unsigned int            nr;
	struct cli_build_stat * stats;
};

#define CLI_BUILD_INIT_NR (4U)

#define cli_build_assert(_build) \
	cli_assert(_build); \
	cli_assert((_build)->nr >= CLI_BUILD_INIT_NR); \
	cli_assert((_build)->cnt <= _build->nr); \
	cli_assert((_build)->stats)

static unsigned int
cli_build_count(const struct cli_build * builder)
{
	cli_build_assert(builder);

	return builder->cnt;
}

static void
cli_build_push(struct cli_build *     builder,
               cli_build_process_fn * process,
               struct cli_dir *       directory)
{
	cli_build_assert(builder);
	cli_assert(process);

	if (builder->cnt == builder->nr) {
		builder->nr *= 2;
		builder->stats = cli_realloc(builder->stats,
		                             builder->nr *
		                             sizeof(builder->stats[0]));
		cli_assert(builder->stats);
	}

	builder->stats[builder->cnt].process = process;
	builder->stats[builder->cnt++].dir = directory;
}

static struct cli_build_stat *
cli_build_pop(struct cli_build * builder)
{
	cli_build_assert(builder);
	cli_assert(builder->cnt);

	return &builder->stats[--builder->cnt];
}

static int
cli_build_process(struct cli_build *       builder,
                  const struct lysc_node * node,
                  enum cli_walk_event      event,
                  struct cli_context *     context)
{
	cli_build_assert(builder);
	cli_assert(builder->cnt);
	cli_assert(node);
	cli_assert((event == CLI_WALK_PRE_EVT) || (event == CLI_WALK_POST_EVT));
	cli_assert_context(context);

	struct cli_build_stat * stat = &builder->stats[builder->cnt - 1];

	cli_assert(stat->process);

	return stat->process(stat, context, node, event, builder);
}

static void
cli_build_clear(struct cli_build * builder)
{
	cli_build_assert(builder);

	builder->cnt = 0;
}

static void
cli_build_init(struct cli_build * builder)
{
	cli_assert(builder);

	builder->cnt = 0;
	builder->nr = CLI_BUILD_INIT_NR;
	builder->stats = cli_malloc(CLI_BUILD_INIT_NR *
	                            sizeof(builder->stats[0]));
}

static void
cli_build_fini(struct cli_build * builder)
{
	cli_build_assert(builder);

	cli_free(builder->stats);
}

/******************************************************************************/
/******************************************************************************/
/******************************************************************************/

#if 0
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
	int                      ret;

	/* Probe for YANG cly extension set. */
	ret = lysc_cly_load(node, &cly);
	if (ret != LY_SUCCESS) {
		cli_lysc_log(node,
		             "cannot process menu entry: %s.",
		             ly_strerr(ret));
		return -EBADR;
	}

	has_cly = lysc_cly_exists(&cly);
	if (has_cly) {
		/* A set of YANG cly extensions is present... */

		if (lysc_cly_is_ignored(&cly))
			/* ...but it has been requested to be ignored. */
			goto unload;

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
		ret = cli_dir_make_child(&root,
		                         node->module->name,
		                         CLI_DIR_MOD_TYPE,
		                         node->module);
		if (ret)
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
		ret = -errno;
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
			ret = cli_build_ext_cmd(context, dir, node, cmd);
			if (ret)
				goto unload;
		}
	}
	else {
		ret = cli_show_make_config_cmd(
			dir,
			(const struct lysc_node_container *)node,
			"config",
			context);
		if (ret)
			goto unload;

		ret = cli_show_make_oper_cmd(
			dir,
			(const struct lysc_node_container *)node,
			"status",
			context);
		if (ret)
			goto unload;
	}

	lysc_cly_unload(&cly);

	return 0;

nodir:
	cli_lysc_log(node,
	             "cannot create menu directory entry: %s.",
	             cli_dir_strerror(-ret));
unload:
	lysc_cly_unload(&cly);

	return ret;
}

static int
cli_build_handle_list(struct cli_context *     context,
                      const struct lysc_node * node,
                      struct cli_build_stat *       builder)
{
	struct cli_dir * dir = builder->dir;

	if (!dir) {
		/*
		 * Create the top-level directory related to the module owning
		 * the node.
		 */

		int ret;

		dir = &context->root;
		ret = cli_dir_make_child(&dir,
		                         node->module->name,
		                         CLI_DIR_MOD_TYPE,
		                         node->module);
		if (ret) {
			cli_lysc_log(node,
			             "cannot create menu directory entry: %s.",
			             cli_dir_strerror(-ret));
			return ret;
		}

		builder->dir = dir;
	}

	ret = cli_show_make_config_list_cmd(dir,
	                                    (const struct lysc_node_list *)node,
	                                    "config",
	                                    context);
	if (ret)
		return ret;

	ret = cli_show_make_oper_list_cmd(dir,
	                                  (const struct lysc_node_list *)node,
	                                  "status",
	                                  context);
	if (ret)
		return ret;

	return 0;
}

static int
cli_build_tree_dir(struct cli_context *     context,
                   const struct lysc_node * node,
                   enum cli_walk_event      event,
                   struct cli_build_stat *  builder)
{
	cli_assert_context(context);
	cli_assert(node);
	cli_assert((event == CLI_WALK_PRE_EVT) || (event == CLI_WALK_POST_EVT));

	int ret;

	if (cli_lysc_status_flags(node) &
	    (LYS_STATUS_OBSLT | LYS_STATUS_DEPRC)) {
		/* Ignore obsolete and deprecated nodes. */
		cli_lysc_dbg(node, "obsolete / deprecated node ignored.");
		return CLI_WALK_SKIP_RET;
	}

	switch (node->nodetype) {
	case LYS_CONTAINER:
		if (event == CLI_WALK_PRE_EVT) {
			ret = cli_build_handle_container(context, node);
			if (ret)
				return ret;
		}

		break;

	case LYS_LIST:
		if (event == CLI_WALK_PRE_EVT) {
			ret = cli_build_handle_list(context, node, builder);
			if (ret)
				return ret;
		}

		break;

	case LYS_LEAF:
		return CLI_WALK_SKIP_RET;

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
		if (event == CLI_WALK_PRE_EVT)
			cli_lysc_dbg(node,
			             "'%s' node type support not implemented !",
			             cli_ly_nodetype_str(node->nodetype));
		return CLI_WALK_SKIP_RET;
	}

	return CLI_WALK_CONT_RET;
}

#endif


/******************************************************************************
 * Container command line builder state handling.
 ******************************************************************************/

static int
cli_build_process_container(struct cli_build_stat *  state,
                            struct cli_context *     context,
                            const struct lysc_node * node,
                            enum cli_walk_event      event,
                            struct cli_build *       builder)
{
#warning Implement me!!
	return (event == CLI_WALK_PRE_EVT) ? CLI_WALK_SKIP_RET : CLI_WALK_CONT_RET;
}

/******************************************************************************
 * List command line builder state handling.
 ******************************************************************************/

static int
cli_build_process_list(struct cli_build_stat *  state,
                       struct cli_context *     context,
                       const struct lysc_node * node,
                       enum cli_walk_event      event,
                       struct cli_build *       builder)
{
#warning Implement me!!
	return (event == CLI_WALK_PRE_EVT) ? CLI_WALK_SKIP_RET : CLI_WALK_CONT_RET;
}

/******************************************************************************
 * Top-level module command line builder state handling.
 ******************************************************************************/

static int
cli_build_top_dir(struct cli_dir **        directory,
                  const struct lysc_node * node,
                  struct cli_context *     context)
{
	cli_assert(directory);
	cli_assert(node);
	cli_assert_context(context);

	struct cli_dir * dir = &context->root;
	int              ret;

	/*
	 * Create the top-level directory related to the module owning
	 * the node.
	 */
	ret = cli_dir_make_child(&dir,
	                         node->module->name,
	                         CLI_DIR_MOD_TYPE,
	                         node->module);
	if (ret)
		goto nodir;

	/*
	 * Then, create the directory entry related to the container given in
	 * argument underneath.
	 */
	ret = cli_dir_make_child(&dir, node->name, CLI_DIR_NODE_TYPE, node);
	if (ret)
		goto nodir;

	*directory = dir;

	return 0;

nodir:
	cli_lysc_log(node,
	             "cannot create top-level menu directory entry: %s.",
	             cli_dir_strerror(-ret));

	return ret;
}

static int
cli_build_topcont_dir(struct cli_dir **                  directory,
                      const struct lysc_node_container * container,
                      struct cli_context *               context)
{
	cli_assert(directory);
	cli_assert(container);
	cli_assert_context(context);

	struct cli_dir * dir;
	int              ret;

	/*
	 * Create the directory entry related to the container given in
	 * argument.
	 */
	ret = cli_build_top_dir(&dir,
	                        (const struct lysc_node *)container,
	                        context);
	if (ret)
		return ret;

	/*
	 * Create and attach a show configuration command to the directory just
	 * created.
	 */
	ret = cli_show_make_config_cmd(dir, container, "config", context);
	if (ret)
		return ret;

	/*
	 * Create and attach a show operational state command to the directory
	 * just created.
	 */
	ret = cli_show_make_oper_cmd(dir, container, "status", context);
	if (ret)
		return ret;

	*directory = dir;

	return 0;
}

static int
cli_build_handle_topcont(struct cli_build_stat *            state __cli_unused,
                         struct cli_context *               context,
                         const struct lysc_node_container * container,
                         enum cli_walk_event                event,
                         struct cli_build *                 builder)
{
	cli_assert(state);
	cli_assert(!state->dir);
	cli_assert_context(context);
	cli_assert(container);
	cli_assert(((const struct lysc_node *)container)->nodetype ==
	           LYS_CONTAINER);
	cli_assert((event == CLI_WALK_PRE_EVT) || (event == CLI_WALK_POST_EVT));
	cli_build_assert(builder);

	if (event == CLI_WALK_PRE_EVT) {
		if (cli_build_node_outdated((const struct lysc_node *)
		                            container)) {
			struct cli_dir * dir;
			int              ret;

			ret = cli_build_topcont_dir(&dir, container, context);
			cli_assert(ret <= 0);
			if (ret)
				return ret;

			cli_build_push(builder,
			               cli_build_process_container,
			               dir);
		}
		else
			return CLI_WALK_SKIP_RET;
	}
	else /* if (event == CLI_WALK_POST_EVT) */
		cli_build_pop(builder);

	return CLI_WALK_CONT_RET;
}

static int
cli_build_toplist_dir(struct cli_dir **             directory,
                      const struct lysc_node_list * list,
                      struct cli_context *          context)
{
	cli_assert(directory);
	cli_assert(list);
	cli_assert_context(context);

	struct cli_dir * dir;
	int              ret;

	/* Create the directory entry related to the list given in argument. */
	ret = cli_build_top_dir(&dir, (const struct lysc_node *)list, context);
	if (ret)
		return ret;

	/*
	 * Create and attach a show configuration command to the directory just
	 * created.
	 */
	ret = cli_show_make_config_list_cmd(dir, list, "config", context);
	if (ret)
		return ret;

	if (ret)
		return ret;

	/*
	 * Create and attach a show operational state command to the directory
	 * just created.
	 */
	ret = cli_show_make_oper_list_cmd(dir, list, "status", context);
	if (ret)
		return ret;

	*directory = dir;

	return 0;
}

static int
cli_build_handle_toplist(struct cli_build_stat *       state __cli_unused,
                         struct cli_context *          context,
                         const struct lysc_node_list * list,
                         enum cli_walk_event           event,
                         struct cli_build *            builder)
{
	cli_assert(state);
	cli_assert(!state->dir);
	cli_assert_context(context);
	cli_assert(list);
	cli_assert(((const struct lysc_node *)list)->nodetype == LYS_LIST);
	cli_assert((event == CLI_WALK_PRE_EVT) || (event == CLI_WALK_POST_EVT));
	cli_build_assert(builder);

	if (event == CLI_WALK_PRE_EVT) {
		if (cli_build_node_outdated((const struct lysc_node *)list)) {
			struct cli_dir * dir;
			int              ret;

			ret = cli_build_toplist_dir(&dir, list, context);
			cli_assert(ret <= 0);
			if (ret)
				return ret;

			cli_build_push(builder, cli_build_process_list, dir);
		}
		else
			return CLI_WALK_SKIP_RET;
	}
	else /* if (event == CLI_WALK_POST_EVT) */
		cli_build_pop(builder);

	return CLI_WALK_CONT_RET;
}

static int
cli_build_process_module(struct cli_build_stat *  state,
                         struct cli_context *     context,
                         const struct lysc_node * node,
                         enum cli_walk_event      event,
                         struct cli_build *       builder)
{
	cli_assert(state);
	cli_assert_context(context);
	cli_assert(node);
	cli_assert((event == CLI_WALK_PRE_EVT) || (event == CLI_WALK_POST_EVT));
	cli_build_assert(builder);

	switch (node->nodetype) {
	case LYS_CONTAINER:
		return cli_build_handle_topcont(
			state,
			context,
			(const struct lysc_node_container *)node,
			event,
			builder);

	case LYS_LIST:
		return cli_build_handle_toplist(
			state,
			context,
			(const struct lysc_node_list *)node,
			event,
			builder);

	case LYS_RPC:
	case LYS_ACTION:
	case LYS_NOTIF:
		if (event == CLI_WALK_PRE_EVT)
			cli_lysc_dbg(node,
			             "'%s' node type not implemented !",
			             cli_ly_nodetype_str(node->nodetype));
		break;

	default:
		if (event == CLI_WALK_PRE_EVT)
			cli_lysc_dbg(node,
			             "unexpected '%s' node type.",
			             cli_ly_nodetype_str(node->nodetype));
	}

	return CLI_WALK_SKIP_RET;
}

/******************************************************************************
 * Command line builder logic boostrapping.
 ******************************************************************************/

static int
cli_build_tree(struct cli_context *     context,
               const struct lysc_node * node,
               enum cli_walk_event      event,
               void *                   builder)
{
	cli_assert_context(context);
	cli_assert(node);
	cli_assert((event == CLI_WALK_PRE_EVT) || (event == CLI_WALK_POST_EVT));
	cli_assert(builder);

	return cli_build_process((struct cli_build *)builder,
	                         node,
	                         event,
	                         context);
}

int
cli_build_from_schema(struct cli_context * context)
{
	unsigned int              m;
	const struct lys_module * mod;
	struct cli_build          build;
	int                       ret = 0;

	cli_build_init(&build);

	cli_lys_foreach_module(context, m, mod) {
		cli_build_push(&build, cli_build_process_module, NULL);

		if (mod->compiled->data) {
			ret = cli_lys_walk_module(context,
			                          mod,
			                          cli_build_tree,
			                          &build);
			if (ret)
				break;
		}

		cli_assert(cli_build_count(&build) == 1);
		cli_build_clear(&build);
	}

	cli_build_fini(&build);

	return ret;
}
