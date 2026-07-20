#ifndef _CLI_SHELL_H
#define _CLI_SHELL_H

#include "common.h"
#include <stdbool.h>
#include <signal.h>

struct cli_shell {
	char *                prompt;
	char *                pref;
	volatile sig_atomic_t shutdown;
	char *                hpath;
};

#define cli_shell_assert(_shell) \
	cli_assert(_shell); \
	cli_assert((_shell)->prompt); \
	cli_assert((_shell)->pref)

static inline void
cli_shell_shutdown(struct cli_shell * shell)
{
	cli_shell_assert(shell);

	shell->shutdown = 1;
}

extern void
cli_shell_set_prompt(struct cli_shell * shell, const char * prompt);

struct cli_shell_expr {
	unsigned int  nr;
	char **       words;
	char *        ln;
};

extern int
cli_shell_read_expr(const struct cli_shell * shell,
                    struct cli_shell_expr *  expr);

extern int
cli_shell_init(struct cli_shell * shell, bool history);

extern void
cli_shell_fini(struct cli_shell * shell);

#endif  /* _CLI_SHELL_H */
