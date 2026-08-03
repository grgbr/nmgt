#ifndef _CLI_CONFIG_H
#define _CLI_CONFIG_H

#include "common.h"

struct cli_dir;
struct lysc_node_leaf;
struct cli_context;

extern struct cli_cmd *
cli_config_make_cmd(struct cli_dir *              directory,
                    const struct lysc_node_leaf * leaf,
                    const struct cli_context *    context);

#endif /* _CLI_CONFIG_H */
