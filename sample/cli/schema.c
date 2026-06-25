#define _GNU_SOURCE

#include <sysrepo.h>
#include <assert.h>

static const char *
clily_basetype_str(LY_DATA_TYPE basetype)
{
	switch (basetype) {
	case LY_TYPE_BINARY:
		return "binary";
	case LY_TYPE_UINT8:
		return "8-bits unsigned integer";
	case LY_TYPE_UINT16:
		return "16-bits unsigned integer";
	case LY_TYPE_UINT32:
		return "32-bits unsigned integer";
	case LY_TYPE_UINT64:
		return "64-bits unsigned integer";
	case LY_TYPE_STRING:
		return "string";
	case LY_TYPE_BITS:
		return "bits";
	case LY_TYPE_BOOL:
		return "boolean";
	case LY_TYPE_DEC64:
		return "64-bits decimal floating-point integer";
	case LY_TYPE_EMPTY:
		return "empty";
	case LY_TYPE_ENUM:
		return "enumeration";
	case LY_TYPE_IDENT:
		return "identityref";
	case LY_TYPE_INST:
		return "instance-identifier";
	case LY_TYPE_LEAFREF:
		return "leafref";
	case LY_TYPE_UNION:
		return "union";
	case LY_TYPE_INT8:
		return "8-bits signed integer";
	case LY_TYPE_INT16:
		return "16-bits signed integer";
	case LY_TYPE_INT32:
		return "32-bits signed integer";
	case LY_TYPE_INT64:
		return "64-bits signed integer";
	default:
		return "unknown";
	}
}

static const char *
clily_lysc_type_str(const struct lysc_type * type)
{
	return clily_basetype_str(type->basetype);
}

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

/******************************************************************************/
/******************************************************************************/
/******************************************************************************/
/******************************************************************************/

typedef int cli_fill_range_part_help_fn(char *, const struct lysc_range_part *);

/*
 * Range for unsigned types, i.e.: uint8, uint16, uint32, uint64
 * min:max|min:max
 *  20:20
 */
static int
cli_fill_urange_part_help(char * string, const struct lysc_range_part * part)
{
	assert(string);
	assert(part);

	return sprintf(string,
	               "%" PRIu64 ":%" PRIu64,
	               part->min_u64,
	               part->max_u64);
}

/*
 * Range for signed types, i.e.: int8, int16, int32, int64, decimal64
 * [-]min:[-]max|[-]min:[-]max
 *  20:20
 */
static int
cli_fill_srange_part_help(char * string, const struct lysc_range_part * part)
{
	assert(string);
	assert(part);

	return sprintf(string,
	               "%" PRId64 ":%" PRId64,
	               part->min_64,
	               part->max_64);
}

static char *
cli_build_range_help(const struct lysc_range *     range,
                     cli_fill_range_part_help_fn * fill)
{
	assert(range);
	assert(LY_ARRAY_COUNT(range->parts));

	LY_ARRAY_COUNT_TYPE nr = LY_ARRAY_COUNT(range->parts);
	char *              str;
	int                 len;
	LY_ARRAY_COUNT_TYPE p;

	str = malloc((nr * (20 + 1 + 20)) + (nr - 1) + 1);
	if (!str)
		return NULL;

	len = fill(str, &range->parts[0]);
	for (p = 1; p < nr; p++)
		len = fill(&str[len], &range->parts[p]);

	return str;
}

/*
 * Leaf help string model
 *
 * <leaf_name> -- <leaf_brief>
 *                type:    <leaf_base_type>
 *                range:   <leaf_numerical_ranges>
 *                size:    <leaf_bin_size_ranges>
 *                length:  <leaf_string_length_ranges>
 *                pattern: <leaf_string_pattern>
 *                units:   <leaf_units>
 *                default: <leaf_default_value>
 *                [leaf_description]
 */

static int
cli_help_brief(const char ** string)
{
	assert(string);

	const char * brief = *string;

	if (brief) {
		brief = &brief[strspn(brief, " \t\n\r\v\f")];

		*string = brief;
		return (int)strcspn(brief, ".\n\r\v\f");
	}
	else
		return 0;
}

static int
cli_help_desc(const char ** string)
{
	assert(string);

	const char * desc = *string;

	if (desc) {
		const char * last;
		const char * toskip = " \t\n\r\v\f";
		size_t       tlen = sizeof(toskip) - 1;

		desc = &desc[strspn(desc, " \t\n\r\v\f")];
		last = &desc[strlen(desc)];

		while (last > desc) {
			if (!memchr(toskip, last[-1], tlen))
				break;
			last--;
		}

		*string = desc;
		return (int)(last - desc);
	}
	else
		return 0;
}

static void
cli_help_briefn_desc(const char *  string,
                     const char ** brief,
                     int *         brief_len,
                     const char ** desc,
                     int *         desc_len)
{
	static const char * unspec = "??";

	if (string) {
		const char * bstr = string;
		int          blen;
		const char * dstr;
		int          dlen;

		blen = cli_help_brief(&bstr);
		if (!blen)
			goto nobrief;
		*brief = bstr;
		*brief_len = blen;

		if (bstr[blen] == '.')
			blen++;
		dstr = &bstr[blen];
		dlen = cli_help_desc(&dstr);
		if (!dlen)
			goto nodesc;
		*desc = dstr;
		*desc_len = dlen;

		return;
	}

nobrief:
	*brief = unspec;
	*brief_len = sizeof(unspec) - 1;
nodesc:
	*desc_len = 0;
}

static const char *
cli_value_str(const struct lysc_value * default_value)
{
	return default_value->str ? default_value->str : "none";
}

static const char *
cli_leaf_units_str(const struct lysc_node_leaf * leaf)
{
	return leaf->units ? leaf->units : "none";
}

static char *
cli_build_bin_leaf_help(const struct lysc_node_leaf * leaf)
{
	assert(leaf);
	assert(leaf->nodetype == LYS_LEAF);
	assert(leaf->type->basetype == LY_TYPE_BINARY);

	const struct lysc_type_bin * type = (const struct lysc_type_bin *)
	                                    leaf->type;
	char *                       rng;
	int                          blen;
	const char *                 brief;
	int                          dlen;
	const char *                 desc;
	int                          ilen;
	const char *                 dflt;
	char *                       help;

	if (type->length) {
		rng = cli_build_range_help(type->length,
		                           cli_fill_urange_part_help);
		if (!rng)
			return NULL;
	}
	else
		rng = NULL;
	cli_help_briefn_desc(leaf->dsc, &brief, &blen, &desc, &dlen);
	ilen = strlen(leaf->name) + sizeof(" -- ") - 1;
	dflt = cli_value_str(&leaf->dflt);
	if (dlen) {
		if (asprintf(&help,
		             "%s -- %*.*s\n"
		             "%*stype:    %s\n"
		             "%*ssize:    %s\n"
		             "%*sdefault: %s\n"
		             "%*s%*.*s\n",
		             leaf->name, blen, blen, brief,
		             ilen, "", clily_basetype_str(LY_TYPE_BINARY),
		             ilen, "", rng ? rng : "none",
		             ilen, "", dflt,
		             ilen, "", dlen, dlen, desc) < 0)
			help = NULL;
	}
	else {
		if (asprintf(&help,
		             "%s -- %*.*s\n"
		             "%*stype:    %s\n"
		             "%*ssize:    %s\n"
		             "%*sdefault: %s\n",
		             leaf->name, blen, blen, brief,
		             ilen, "", clily_basetype_str(LY_TYPE_BINARY),
		             ilen, "", rng ? rng : "none",
		             ilen, "", dflt) < 0)
			help = NULL;
	}

	free(rng);

	return help;
}

static char *
cli_build_patterns_help(const struct lysc_pattern * const * patterns)
{
	assert(patterns);

	LY_ARRAY_COUNT_TYPE nr = LY_ARRAY_COUNT(patterns);
	LY_ARRAY_COUNT_TYPE p;
	size_t              plens[nr];
	size_t              slen = 0;
	char *              str;

	for (p = 0; p < nr; p++) {
		plens[p] = strlen(patterns[p]->expr);
		slen += plens[p];
	}

	str = malloc(slen +           /* length of all pattern strings */
	             (nr * 2) +       /* + 2 single quotes per pattern */
	             ((nr - 1) * 3) + /* + " && " between and'ed patterns */
	             1);              /* + terminating NULL byte. */
	if (!str)
		return NULL;

	slen = 0;
	str[slen++] = '\'';
	memcpy(&str[slen], patterns[0]->expr, plens[0]);
	slen += plens[0];
	str[slen++] = '\'';
	for (p = 1; p < nr; p++) {
		str[slen++] = ' ';
		str[slen++] = '&';
		str[slen++] = '&';
		str[slen++] = ' ';
		str[slen++] = '\'';
		memcpy(&str[slen], patterns[p]->expr, plens[p]);
		slen += plens[p];
		str[slen++] = '\'';
	}
	str[slen] = '\0';

	return str;
}

static char *
cli_build_str_leaf_help(const struct lysc_node_leaf * leaf)
{
	assert(leaf);
	assert(leaf->nodetype == LYS_LEAF);
	assert(leaf->type->basetype == LY_TYPE_STRING);

	const struct lysc_type_str * type = (const struct lysc_type_str *)
	                                    leaf->type;
	char *                       rng;
	char *                       ptrns;
	int                          blen;
	const char *                 brief;
	int                          dlen;
	const char *                 desc;
	int                          ilen;
	const char *                 dflt;
	char *                       help;

	if (type->length) {
		rng = cli_build_range_help(type->length,
		                           cli_fill_urange_part_help);
		if (!rng)
			return NULL;
	}
	else
		rng = NULL;
	if (type->patterns) {
		ptrns = cli_build_patterns_help(
			(const struct lysc_pattern * const *)type->patterns);
		if (!ptrns) {
			help = NULL;
			goto free_rng;
		}
	}
	else
		ptrns = NULL;
	cli_help_briefn_desc(leaf->dsc, &brief, &blen, &desc, &dlen);
	ilen = strlen(leaf->name) + sizeof(" -- ") - 1;
	dflt = cli_value_str(&leaf->dflt);
	if (dlen) {
		if (asprintf(&help,
		             "%s -- %*.*s\n"
		             "%*stype:    %s\n"
		             "%*slength:  %s\n"
		             "%*spattern: %s\n"
		             "%*sdefault: %s\n"
		             "%*s%*.*s\n",
		             leaf->name, blen, blen, brief,
		             ilen, "", clily_basetype_str(LY_TYPE_STRING),
		             ilen, "", rng ? rng : "none",
		             ilen, "", ptrns ? ptrns : "none",
		             ilen, "", dflt,
		             ilen, "", dlen, dlen, desc) < 0)
			help = NULL;
	}
	else {
		if (asprintf(&help,
		             "%s -- %*.*s\n"
		             "%*stype:    %s\n"
		             "%*slength:  %s\n"
		             "%*spattern: %s\n"
		             "%*sdefault: %s\n",
		             leaf->name, blen, blen, brief,
		             ilen, "", clily_basetype_str(LY_TYPE_STRING),
		             ilen, "", rng ? rng : "none",
		             ilen, "", ptrns ? ptrns : "none",
		             ilen, "", dflt) < 0)
			help = NULL;
	}

	free(ptrns);

free_rng:
	free(rng);

	return help;
}

static char *
cli_build_num_leaf_help(const struct lysc_node_leaf * leaf,
                        cli_fill_range_part_help_fn * fill_range)
{
	assert(leaf);
	assert((leaf->type->basetype == LY_TYPE_UINT8) ||
	       (leaf->type->basetype == LY_TYPE_UINT16) ||
	       (leaf->type->basetype == LY_TYPE_UINT32) ||
	       (leaf->type->basetype == LY_TYPE_UINT64) ||
	       (leaf->type->basetype == LY_TYPE_DEC64) ||
	       (leaf->type->basetype == LY_TYPE_INT8) ||
	       (leaf->type->basetype == LY_TYPE_INT16) ||
	       (leaf->type->basetype == LY_TYPE_INT32) ||
	       (leaf->type->basetype == LY_TYPE_INT64));
	assert(leaf->nodetype == LYS_LEAF);
	assert(fill_range);

	const struct lysc_type_num * type = (const struct lysc_type_num *)
	                                    leaf->type;
	char *                       rng;
	int                          blen;
	const char *                 brief;
	int                          dlen;
	const char *                 desc;
	int                          ilen;
	const char *                 base;
	const char *                 units;
	const char *                 dflt;
	char *                       help;

	if (type->range) {
		rng = cli_build_range_help(type->range, fill_range);
		if (!rng)
			return NULL;
	}
	else
		rng = NULL;
	cli_help_briefn_desc(leaf->dsc, &brief, &blen, &desc, &dlen);
	ilen = strlen(leaf->name) + sizeof(" -- ") - 1;
	base = clily_basetype_str(type->basetype);
	units = cli_leaf_units_str(leaf);
	dflt = cli_value_str(&leaf->dflt);
	if (dlen) {
		if (asprintf(&help,
		             "%s -- %*.*s\n"
		             "%*stype:    %s\n"
		             "%*srange:   %s\n"
		             "%*sunits:   %s\n"
		             "%*sdefault: %s\n"
		             "%*s%*.*s\n",
		             leaf->name, blen, blen, brief,
		             ilen, "", base,
		             ilen, "", rng ? rng : "none",
		             ilen, "", units,
		             ilen, "", dflt,
		             ilen, "", dlen, dlen, desc) < 0)
			help = NULL;
	}
	else {
		if (asprintf(&help,
		             "%s -- %*.*s\n"
		             "%*stype:    %s\n"
		             "%*srange:   %s\n"
		             "%*sunits:   %s\n"
		             "%*sdefault: %s\n",
		             leaf->name, blen, blen, brief,
		             ilen, "", base,
		             ilen, "", rng ? rng : "none",
		             ilen, "", units,
		             ilen, "", dflt) < 0)
			help = NULL;
	}

	free(rng);

	return help;
}

static char *
cli_build_bool_leaf_help(const struct lysc_node_leaf * leaf)
{
	assert(leaf);
	assert(leaf->nodetype == LYS_LEAF);

	int          blen;
	const char * brief;
	int          dlen;
	const char * desc;
	int          ilen = strlen(leaf->name) + sizeof(" -- ") - 1;
	const char * dflt = cli_value_str(&leaf->dflt);
	char *       help;

	cli_help_briefn_desc(leaf->dsc, &brief, &blen, &desc, &dlen);
	if (dlen) {
		if (asprintf(&help,
		             "%s -- %*.*s\n"
		             "%*stype:    %s\n"
		             "%*sdefault: %s\n"
		             "%*s%*.*s\n",
		             leaf->name, blen, blen, brief,
		             ilen, "", clily_basetype_str(LY_TYPE_BOOL),
		             ilen, "", dflt,
		             ilen, "", dlen, dlen, desc) < 0)
			return NULL;
	}
	else {
		if (asprintf(&help,
		             "%s -- %*.*s\n"
		             "%*stype:    %s\n"
		             "%*sdefault: %s\n",
		             leaf->name, blen, blen, brief,
		             ilen, "", clily_basetype_str(LY_TYPE_BOOL),
		             ilen, "", dflt) < 0)
			return NULL;
	}

	return help;
}

static char *
cli_build_leaf_help(const struct lysc_node_leaf * leaf,
                    void *                        data)
{
	switch (leaf->type->basetype) {
	case LY_TYPE_BINARY:
		return cli_build_bin_leaf_help(leaf);

	case LY_TYPE_UINT8:
	case LY_TYPE_UINT16:
	case LY_TYPE_UINT32:
	case LY_TYPE_UINT64:
		return cli_build_num_leaf_help(leaf, cli_fill_urange_part_help);

	case LY_TYPE_STRING:
		return cli_build_str_leaf_help(leaf);

	case LY_TYPE_BITS:
		goto impl;

	case LY_TYPE_BOOL:
		return cli_build_bool_leaf_help(leaf);

	case LY_TYPE_EMPTY:
	case LY_TYPE_ENUM:
	case LY_TYPE_IDENT:
	case LY_TYPE_INST:
	case LY_TYPE_LEAFREF:
	case LY_TYPE_UNION:
		goto impl;

	case LY_TYPE_INT8:
	case LY_TYPE_INT16:
	case LY_TYPE_INT32:
	case LY_TYPE_INT64:
	case LY_TYPE_DEC64:
		return cli_build_num_leaf_help(leaf, cli_fill_srange_part_help);

	default:
		assert(0);
	}

impl:
#warning remove me
	/* Implement me ! */
	{
		char * str;

		if (asprintf(&str,
		             "IMPLEMENT %s support for '%s' node.\n",
		             clily_basetype_str(leaf->type->basetype),
		             leaf->name) < 0)
			return NULL;
		return str;
	}
}

#if 0
static void
cli_build_bin_leaf_help(const struct lysc_type * type, char * string, size_t size)
{
	switch (type->basetype) {
    case LY_TYPE_BITS:
    case LY_TYPE_ENUM: {
        /* bits and enums structures are compatible */
        struct lysc_type_bits *bits = (struct lysc_type_bits *)type;

        yprc_bits_enum(pctx, bits->bits, type->basetype, &flag);
        break;
    }
    case LY_TYPE_EMPTY:
        /* nothing to do */
        break;
    case LY_TYPE_IDENT: {
        struct lysc_type_identityref *ident = (struct lysc_type_identityref *)type;

        LY_ARRAY_FOR(ident->bases, u) {
            ypr_open(pctx->out, &flag);
            ypr_substmt(pctx, LY_STMT_BASE, u, ident->bases[u]->name, 0, type->exts);
        }
        break;
    }
    case LY_TYPE_INST: {
        struct lysc_type_instanceid *inst = (struct lysc_type_instanceid *)type;

        ypr_open(pctx->out, &flag);
        ypr_substmt(pctx, LY_STMT_REQUIRE_INSTANCE, 0, inst->require_instance ? "true" : "false", 0, inst->exts);
        break;
    }
    case LY_TYPE_LEAFREF: {
        struct lysc_type_leafref *lr = (struct lysc_type_leafref *)type;

        ypr_open(pctx->out, &flag);
        ypr_substmt(pctx, LY_STMT_PATH, 0, lr->path->expr, 0, lr->exts);
        ypr_substmt(pctx, LY_STMT_REQUIRE_INSTANCE, 0, lr->require_instance ? "true" : "false", 0, lr->exts);
        yprc_type(pctx, lr->realtype);
        break;
    }
    case LY_TYPE_UNION: {
        struct lysc_type_union *un = (struct lysc_type_union *)type;

        LY_ARRAY_FOR(un->types, u) {
            ypr_open(pctx->out, &flag);
            yprc_type(pctx, un->types[u]);
        }
        break;
    }
    default:
        LOGINT(pctx->module->ctx);
    }

    LEVEL--;
    ypr_close(pctx, flag);
}
#endif

/******************************************************************************/
/******************************************************************************/
/******************************************************************************/
/******************************************************************************/

//lysc_node_child()
static LY_ERR
clily_print_command_clb(struct lysc_node * node, void * data, ly_bool * goon)
{
	char * str;

	switch (node->nodetype) {
	case LYS_CONTAINER:
		/* TODO: make sure that LYSC_PATH_DATA is the adequate type. */
		str = lysc_path(node, LYSC_PATH_DATA, NULL, 0);
		if (!str)
			return LY_EMEM;
		printf("%s\n", str);
		free(str);
		break;

	case LYS_CHOICE:
		printf("%s\n", node->name);
		break;

	case LYS_LEAF:
		str = cli_build_leaf_help((const struct lysc_node_leaf *)node,
		                          data);
		if (!str)
			return LY_EMEM;
		fputs(str, stdout);
		free(str);
		break;

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

		ret = clily_lysc_module_dfs(mod, clily_print_command_clb, NULL);
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

		ret = lysc_module_dfs_full(mod, clily_print_command_clb, NULL);
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
