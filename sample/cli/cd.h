#ifndef _CLI_CD_H
#define _CLI_CD_H

#include "common.h"

struct cli_dir;

extern void
cli_chdir_build_cmd(struct cli_dir * directory);

#endif /* _CLI_CD_H */
