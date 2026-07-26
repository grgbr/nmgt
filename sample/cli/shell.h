#ifndef _CLI_SHELL_H
#define _CLI_SHELL_H

#include "match.h"
#include <readline/readline.h>
#include <stdbool.h>
#include <signal.h>

struct cli_shell;

typedef void cli_shell_complete_fn(struct cli_shell *,
                                   const char *,
                                   size_t,
                                   int,
                                   const char * const [],
                                   struct cli_match *);

struct cli_shell {
	char *                  prompt;
	char *                  pref;
	cli_shell_complete_fn * complete;
	struct cli_match        matches;
	volatile sig_atomic_t   shutdown;
	char *                  hpath;
};

#define cli_shell_assert(_shell) \
	cli_assert(_shell); \
	cli_assert((_shell)->prompt); \
	cli_assert((_shell)->pref)

extern void
cli_shell_set_prompt(struct cli_shell * shell, const char * prompt);

struct cli_expr_blk;

extern int
cli_shell_read_expr(const struct cli_shell * shell,
                    struct cli_expr_blk *    expr_block);

/*
 * Request the completion logic to prevent from appending the completion
 * character once it has appended a selected match at the current command line
 * cursor position.
 * When required, this MUST be called each time the completion logic is
 * invoked, i.e., this behavior is always resetted before calling any
 * readline(3) custom completion function.
 */
static inline void
cli_shell_suppress_complete_char(void)
{
	rl_completion_suppress_append = 1;
}

static inline void
cli_shell_shutdown(struct cli_shell * shell)
{
	cli_shell_assert(shell);

	shell->shutdown = 1;
}

struct cli_context;

extern int
cli_shell_init(struct cli_shell *      shell,
               bool                    history,
               const char *            word_break_chars,
               cli_shell_complete_fn * complete);

extern void
cli_shell_fini(struct cli_shell * shell);

#endif  /* _CLI_SHELL_H */
