#include "cmd.h"

void
cli_cmd_null_fini(struct cli_cmd * command __cli_unused)
{
	cli_cmd_assert(command);
}

struct cli_cmd *
cli_cmd_build(size_t size, const struct cli_cmd_ops * opers)
{
	cli_assert(size >= sizeof(struct cli_cmd));

	struct cli_cmd * cmd;

	cmd = cli_malloc(size);
	cli_cmd_init(cmd, opers);

	return cmd;
}
