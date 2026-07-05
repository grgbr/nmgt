#ifndef _CLI_YANG_H
#define _CLI_YANG_H

#include "common.h"

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
 * Libyang (compiled) schema node handling
 ******************************************************************************/

extern char *
cli_lysc_xpath(const struct lysc_node * node);

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

typedef int cli_lysc_visit_fn(struct cli_context *,
                              const struct lysc_node *,
                              enum cli_walk_event,
                              void *);

/*
 * Perform a depth first traversal for the Libyang (compiled) schema subtree
 * given in argument.
 */
extern int
cli_lysc_walk_node(struct cli_context *     context,
                   const struct lysc_node * node,
                   cli_lysc_visit_fn *      visit,
                   void *                   data);

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
 * only.
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

#endif /* _CLI_YANG_H */
