#define _GNU_SOURCE

#include <sysrepo.h>
#include <assert.h>

static const char *
cli_schema_basetype_str(LY_DATA_TYPE basetype)
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

static inline const char *
cli_schema_nodetype_str(uint16_t nodetype)
{
	return lys_nodetype2str(nodetype);
}

enum cli_tree_walk_order {
	CLI_TREE_WALK_PRE_ORDER,
	CLI_TREE_WALK_POST_ORDER,
	CLI_TREE_WALK_ORDER_NR
};

#define CLI_TREE_WALK_CONT_RET (0)
#define CLI_TREE_WALK_SKIP_RET (1)

struct cli_data_tree_walk;

typedef int cli_data_tree_visit_fn(const struct lyd_node *,
                                   enum cli_tree_walk_order,
                                   const struct cli_data_tree_walk *);

struct cli_data_tree_walk {
	unsigned int depth;
	void *       priv;
};

#define CLI_DATA_TREE_WALK_INIT(_priv) \
	{ .depth = 0, .priv = _priv }

static inline unsigned int
cli_data_tree_walk_depth(const struct cli_data_tree_walk * walk)
{
	return walk->depth;
}

static inline void *
cli_data_tree_walk_priv(const struct cli_data_tree_walk * walk)
{
	return walk->priv;
}

static int
cli_data_tree_walk_recurs(struct cli_data_tree_walk * walk,
                          const struct lyd_node *     node,
                          cli_data_tree_visit_fn *    visit)
{
	assert(walk);
	assert(node);
	assert(visit);

	int ret;

	ret = visit(node, CLI_TREE_WALK_PRE_ORDER, walk);
	if (ret == CLI_TREE_WALK_CONT_RET) {
		const struct lyd_node * child;

		walk->depth++;

		LY_LIST_FOR(lyd_child(node), child) {
			ret = cli_data_tree_walk_recurs(walk, child, visit);
			if (ret < 0)
				return ret;
		}

		walk->depth--;

		ret = visit(node, CLI_TREE_WALK_POST_ORDER, walk);
	}

	return (ret != CLI_TREE_WALK_SKIP_RET) ? ret : CLI_TREE_WALK_CONT_RET;
}

static int
cli_data_tree_walk(struct cli_data_tree_walk * walk,
                   const struct lyd_node *     node,
                   cli_data_tree_visit_fn *    visit)
{
	assert(walk);
	assert(node);
	assert(visit);

	int                     ret;
	const struct lyd_node * sibling;

	LY_LIST_FOR(node, sibling) {
		ret = cli_data_tree_walk_recurs(walk, sibling, visit);
		if (ret < 0)
			return ret;
	}

	return 0;
}

static void
cli_data_fprint_node(FILE *                  stream,
                     const struct lyd_node * node,
                     int                     indent)
{
	const struct lysc_node * scn = node->schema;

	if (scn) {
		if (scn->nodetype & LYD_NODE_INNER) {
			fprintf(stream,
			        "%*s%s <%s>\n",
			        indent, "",
			        scn->name,
			        cli_schema_nodetype_str(scn->nodetype));
		}
		else if (scn->nodetype & LYD_NODE_TERM) {
			const struct lysc_type * type;

			if (scn->nodetype == LYS_LEAF)
				type = ((const struct lysc_node_leaf *)
				        scn)->type;
			else if (scn->nodetype == LYS_LEAFLIST)
				type = ((const struct lysc_node_leaflist *)
				        scn)->type;
			else
				assert(0);

			fprintf(stream,
			        "%*s%s = '%s' <%s:%s:%s>\n",
			        indent, "",
			        scn->name,
			        lyd_get_value(node),
			        cli_schema_nodetype_str(scn->nodetype),
			        type->name ? type->name : "builtin",
			        cli_schema_basetype_str(type->basetype));
		}
		else
			assert(0);
	}
	else {
		const struct lyd_node_opaq * opaq =
			(const struct lyd_node_opaq *)node;

		fprintf(stream,
		        "%*s%s:%s = '%s' <%s>\n",
		        indent, "",
		        opaq->name.prefix ? opaq->name.prefix : "",
		        opaq->name.name,
		        opaq->value,
		        "opaq");
	}
}

static int
cli_data_print_node(const struct lyd_node *           node,
                    enum cli_tree_walk_order          order,
                    const struct cli_data_tree_walk * walk)
{
	if (order == CLI_TREE_WALK_PRE_ORDER)
		cli_data_fprint_node((FILE *)cli_data_tree_walk_priv(walk),
		                     node,
		                     4 * cli_data_tree_walk_depth(walk));

	return CLI_TREE_WALK_CONT_RET;
}

static int
cli_data_load_roots(sr_session_ctx_t * session, sr_data_t ** roots)
{
	int ret;

	/*
	 * Options that may be used with sr_get_data().
	 * - SR_GET_NO_FILTER for conventional data stores, equivalent to "/*"
	 *   xpath ;
	 * - all sr_get_oper_flag_t enumerators for operational data stores.
	 */
	ret = sr_get_data(session, "/*", 0, 0, SR_OPER_DEFAULT, roots);
	if (ret != SR_ERR_OK)
		return ret;

	assert(*roots);
	assert((*roots)->tree);

	return SR_ERR_OK;
}

static void
cli_data_release_trees(sr_data_t * trees)
{
	sr_release_data(trees);
}

static int
cli_data_fprint_all(FILE * stream, sr_session_ctx_t * session)
{
	sr_data_t *               roots = NULL;
	int                       ret;
	struct cli_data_tree_walk walk = CLI_DATA_TREE_WALK_INIT(stream);

	ret = cli_data_load_roots(session, &roots);
	if (ret != SR_ERR_OK)
		return ret;

	ret = cli_data_tree_walk(&walk, roots->tree, cli_data_print_node);

	cli_data_release_trees(roots);

	return ret;
}

int
main(int argc, const char * const argv[])
{
	sr_conn_ctx_t *    conn;
	sr_session_ctx_t * sess;
	int                err;
	int                xit = EXIT_FAILURE;

	/* Enable info logging at sysrepo level. */
	sr_log_stderr(SR_LL_INF);

	/* Connect to sysrepo. */
	err = sr_connect(0, &conn);
	if (err != SR_ERR_OK)
		return EXIT_FAILURE;

	err = sr_session_start(conn, SR_DS_RUNNING, &sess);
	if (err != SR_ERR_OK)
		goto disconnect;

	if (!cli_data_fprint_all(stdout, sess))
		xit = EXIT_SUCCESS;

	if (sr_session_stop(sess) != SR_ERR_OK)
		xit = EXIT_FAILURE;

disconnect:
	sr_disconnect(conn);

	return xit;
}
