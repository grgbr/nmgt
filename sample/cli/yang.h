#ifndef _CLI_YANG_H
#define _CLI_YANG_H

#include "cli.h"

/******************************************************************************
 * Libyang utils
 ******************************************************************************/

extern const char *
cli_ly_basetype_str(LY_DATA_TYPE basetype);

static inline const char *
cli_ly_nodetype_str(uint16_t nodetype)
{
	return lys_nodetype2str(nodetype);
}

/******************************************************************************
 * Libyang (compiled) schema handling
 ******************************************************************************/

/**
 * Iterate over extension instances.
 *
 * @param[in]    _ext_array  a libyang @ref sizedarrays of ::lysc_ext_instance
 *                           extension instance structures
 * @param[inout] _ext        pointer to the current ::lysc_ext_instance
 *                           extension instance structure
 */
#define cli_lysc_foreach_extension(_ext_array, _ext) \
	LY_ARRAY_FOR(_ext_array, struct lysc_ext_instance, _ext)

/**
 * Tell wether the given extension instance is one of our own `cli-extension`
 * extension directives or not.
 *
 * @param[in] extension   pointer to the extension instance to test
 * @param[in] identifier  extension directive
 *
 * When installed and enabled, the cli extension plugin implements support for 
 * YANG syntax extensions defined into the `cli-extension.yang` module.
 *
 * This function tests wether or not the extension instance given as @p
 * extension is a `cli-extension` defined statement and which identifier is
 * given as @p identifier.
 */
extern bool
cli_lysc_is_extension(const struct lysc_ext_instance * extension,
                      const char *                     identifier);

static inline uint16_t
cli_lysc_conf_flags(const struct lysc_node * node)
{
	cli_assert(node);

	return node->flags & LYS_CONFIG_MASK;
}

static inline uint16_t
cli_lysc_status_flags(const struct lysc_node * node)
{
	cli_assert(node);

	return node->flags & LYS_STATUS_MASK;
}

static inline const struct lysc_node *
cli_lysc_parent(const struct lysc_node * node)
{
	cli_assert(node);

	return lysc_data_parent(node);
}

static inline const struct lysc_node *
cli_lysc_child(const struct lysc_node * node)
{
	cli_assert(node);

	return lysc_node_child(node);
}

extern char *
cli_lysc_node_xpath(const struct lysc_node * node);

static inline const struct lysc_node *
cli_lysc_find_node(const struct cli_context * context,
                   const struct lysc_node *   subtree,
                   const char *               xpath)
{
	cli_assert_context(context);
	cli_assert(xpath);

	return lys_find_path(context->lyctx, subtree, xpath, false);
}

static inline struct ly_set *
cli_lysc_find_nodeset(const struct cli_context * context,
                      const struct lysc_node *    subtree,
                      const char *                xpath)
{
	cli_assert_context(context);
	cli_assert(xpath);

	struct ly_set * nodes;

	return (lys_find_xpath(context->lyctx,
	                       subtree,
	                       xpath,
	                       LYS_FIND_NO_MATCH_ERROR,
	                       &nodes) == LY_SUCCESS) ? nodes
	                                              : NULL;
}

#define cli_lysc_foreach_child(_node, _child) \
	LY_LIST_FOR(cli_lysc_child(_node), _child)

typedef int cli_lysc_visit_fn(struct cli_context *,
                              const struct lysc_node *,
                              enum cli_walk_event,
                              void *);

/*
 * Perform a depth first traversal for the Libyang (compiled) schema subtree
 * given in argument.
 */
extern int
cli_lysc_walk_node(struct cli_context *      context,
                   const struct lysc_node *  node,
                   cli_lysc_visit_fn *       visit,
                   void *                    data);

/*
 * Print YANG specification for the libyang schema node given in argument
 * according to RFC 7950.
 *
 * When given as `true', the `nosub' argument requests the printing of the
 * YANG top-level `module' statement only, i.e., do not print the module's
 * sub-statements.
 */
static inline LY_ERR
cli_lysc_print_node_yang(const struct cli_context * context,
                         const struct lysc_node *   node,
                         bool                       nosub)
{
	/*
	 * Note: line width argument is ignored when outputting node YANG
	 * specification.
	 */
	return lys_print_node(context->lyout,
	                      node,
	                      LYS_OUT_YANG_COMPILED,
	                      0,
	                      nosub ? LYS_PRINT_NO_SUBSTMT : 0);
}

/*
 * Print the YANG specification for the set of libyang schema nodes given in
 * argument according to RFC 7950.
 *
 * When given as `true', the `nosub' argument requests the printing of the
 * YANG top-level `module' statement only, i.e., do not print the module's
 * sub-statements.
 */
extern LY_ERR
cli_lysc_print_nodeset_yang(const struct cli_context * context,
                            const struct ly_set *      nodeset,
                            bool                       nosub);

/*
 * Print the YANG tree diagram for the libyang schema node given in argument
 * according to RFC 8340.
 */
static inline LY_ERR
cli_lysc_print_node_diag(const struct cli_context * context,
                         const struct lysc_node *   node)
{
	/*
	 * Note: the LYS_PRINT_NO_SUBSTMT option is ignored when outputting
	 * node YANG tree diagram.
	 */
	return lys_print_node(context->lyout,
	                      node,
	                      LYS_OUT_TREE,
	                      cli_term_cols(context),
	                      0);
}

/*
 * Print the YANG tree diagram for the set of libyang schema nodes given in
 * argument according to RFC 8340.
 */
extern LY_ERR
cli_lysc_print_nodeset_diag(const struct cli_context * context,
                            const struct ly_set *      nodeset);

/******************************************************************************
 * Libyang module handling
 ******************************************************************************/

struct lys_module;

extern char *
cli_lys_module_xpath(const struct lys_module * module);

/*
 * Return YANG implemented module given by name, excluding sysrepo / libyang
 * internal ones.
 */
extern const struct lys_module *
cli_lys_find_module(const struct cli_context * context, const char * module);

/*
 * Iterate over YANG implemented modules, skipping sysrepo / libyang internal
 * ones.
 * Return compiled and (features) implemented, i.e. completely resolved modules
 * with top-level data node or extension instances only.
 */
#define cli_lys_foreach_module(_context, _index, _module) \
	for ((_index) = 0, \
	     (_module) = cli_lys_next_module(_context, &(_index)); \
	     _module; \
	     (_module) = cli_lys_next_module(_context, &(_index)))

extern const struct lys_module *
cli_lys_next_module(const struct cli_context * context, unsigned int * index);

/*
 * Perform a depth first traversal for the Libyang (compiled) schema tree of the
 * module given in argument.
 */
extern int
cli_lys_walk_module(struct cli_context *      context,
                    const struct lys_module * module,
                    cli_lysc_visit_fn *       visit,
                    void *                    data);

/*
 * Print YANG specification for the libyang module which name is given in
 * argument according to RFC 7950.
 *
 * When given as `true', the `nosub' argument requests the printing of the
 * YANG top-level `module' statement only, i.e., do not print the module's
 * sub-statements.
 */
static inline LY_ERR
cli_lys_print_module_yang(const struct cli_context * context,
                          const struct lys_module *  module,
                          bool                       nosub)
{
	/*
	 * Note: line width argument is ignored when outputting module YANG
	 * specification.
	 */
	return lys_print_module(context->lyout,
	                        module,
	                        LYS_OUT_YANG_COMPILED,
	                        0,
	                        nosub ? LYS_PRINT_NO_SUBSTMT : 0);
}

/*
 * Print the YANG tree diagram for the libyang module which name is given in
 * argument according to RFC 8340.
 */
extern LY_ERR
cli_lys_print_module_diag(const struct cli_context * context,
                          const struct lys_module *  module);

/******************************************************************************
 * Libyang data handling
 ******************************************************************************/

#define cli_lyd_foreach(_data, _node) \
	LY_LIST_FOR((_data)->tree, _node)

#define cli_lyd_foreach_child(_data, _child) \
	LY_LIST_FOR(lyd_child((_data)->tree), child)

extern int
cli_lyd_load(const struct cli_context * context,
             const char *               xpath,
             unsigned int               depth,
             sr_get_oper_flag_t         flags,
             sr_data_t **               data);

extern int
cli_lyd_load_from_schema(const struct cli_context * context,
                         const struct lysc_node *   node,
                         unsigned int               depth,
                         sr_get_oper_flag_t         flags,
                         sr_data_t **               data);

/* Get value of a single data node. */
static inline const char *
cli_lyd_node_value(const sr_data_t * data)
{
	cli_assert(data);
	cli_assert(data->tree);
	cli_assert(LYD_NODE_IS_ALONE(data->tree));
	cli_assert(!lyd_child_any(data->tree));

	return lyd_get_value(data->tree);
}

/* Indicate if the value of a single data node is the default value. */
static inline bool
cli_lyd_node_is_default(const sr_data_t * data)
{
	cli_assert(data);
	cli_assert(data->tree);
	cli_assert(LYD_NODE_IS_ALONE(data->tree));
	cli_assert(!lyd_child_any(data->tree));

	return !!lyd_is_default(data->tree);
}

/* Load a single data node identified by XPATH. */
extern int
cli_lyd_load_node(const struct cli_context * context,
                  const char *               xpath,
                  sr_data_t **               data);

/* Load a single data node identified thanks to a schema node. */
extern int
cli_lyd_load_node_from_schema(const struct cli_context * context,
                              const struct lysc_node *   node,
                              sr_data_t **               data);

static inline void
cli_lyd_unload(sr_data_t * data)
{
	/* data may be NULL here. */
	sr_release_data(data);
}

#endif /* _CLI_YANG_H */
