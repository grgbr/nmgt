#ifndef _CLI_SHOW_H
#define _CLI_SHOW_H

#include "lyd_table.h"

struct cli_dir;
struct cli_context;
struct lysc_node_container;
struct lysc_node_list;

extern int
cli_show_make_config_cmd(struct cli_dir *                   directory,
                         const struct lysc_node_container * container,
                         const char *                       name,
                         const struct cli_context *         context);

extern int
cli_show_make_oper_cmd(struct cli_dir *                   directory,
                       const struct lysc_node_container * container,
                       const char *                       name,
                       const struct cli_context *         context);

extern int
cli_show_make_config_list_cmd(struct cli_dir *         directory,
                         const struct lysc_node_list * list,
                         const char *                  name,
                         const struct cli_context *    context);
extern int
cli_show_make_oper_list_cmd(struct cli_dir *              directory,
                            const struct lysc_node_list * list,
                            const char *                  name,
                            const struct cli_context *    context);

#endif /* _CLI_SHOW_H */
