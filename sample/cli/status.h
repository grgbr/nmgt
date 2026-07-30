#ifndef _CLI_STATUS_H
#define _CLI_STATUS_H

#include "common.h"

struct cli_dir;

extern struct cli_cmd *
cli_status_make_cmd(struct cli_dir * directory);

#endif /* _CLI_STATUS_H */
