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

struct cli_data_tree_walk;

typedef int cli_data_tree_visit_fn(const struct lyd_node *,
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

/* TODO: remove recursion, add pre/in/post order events, refine invocation of visit(). */
static int
cli_data_tree_walk(struct cli_data_tree_walk * walk,
                   const struct lyd_node *     root,
                   cli_data_tree_visit_fn *    visit)
{
	assert(walk);
	assert(root);
	assert(visit);

	const struct lyd_node * node;

	LY_LIST_FOR(root, node) {
		int               ret;
		struct lyd_node * child;

		ret = visit(node, walk);
		if (ret < 0)
			return ret;

		child = lyd_child(node);
		if (child) {
			walk->depth++;
			cli_data_tree_walk(walk, child, visit);
		}
	}

	walk->depth--;

	return 0;
}

/* TODO: REVIEW ME!! */
static int
cli_data_print_node(const struct lyd_node *           node,
                    const struct cli_data_tree_walk * walk)
{
	const struct lysc_node * scn = node->schema;
	int                      indent = cli_data_tree_walk_depth(walk) * 4;
	
	if (scn) {
		if (scn->nodetype & LYD_NODE_INNER) {
			printf("%*s%s <%s>\n",
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

			printf("%*s%s = '%s' <%s:%s:%s>\n",
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

		printf("%*s%s:%s = '%s' <%s>\n",
		       indent, "",
		       opaq->name.prefix ? opaq->name.prefix : "",
		       opaq->name.name,
		       opaq->value,
		       "opaq");
	}

	return 0;
}

static int
clid_print(sr_session_ctx_t * session)
{
	sr_data_t *               root = NULL;
	int                       ret;
	struct cli_data_tree_walk walk = CLI_DATA_TREE_WALK_INIT(NULL);

        ret = sr_get_data(session, "/*", 0, 0, SR_OPER_DEFAULT, &root);
	if (ret != SR_ERR_OK)
		return ret;

	ret = cli_data_tree_walk(&walk, root->tree, cli_data_print_node);

	sr_release_data(root);

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

	if (!clid_print(sess))
		xit = EXIT_SUCCESS;

	if (sr_session_stop(sess) != SR_ERR_OK)
		xit = EXIT_FAILURE;

disconnect:
	sr_disconnect(conn);

	return xit;
}
