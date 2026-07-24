#include "shell.h"
#include "expr.h"
#include "arg.h"
#include <readline/readline.h>
#include <readline/history.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

static int
cli_shell_read_line(const struct cli_shell * shell, char ** line)
{
	cli_shell_assert(shell);
	cli_assert(line);

	char * ln;
	size_t len;
	int    ret;

	ln = readline(shell->prompt);
	if (!ln) {
		/* End of stream required using ^D. */
		rl_crlf();
		return -ESHUTDOWN;
	}

	if (shell->shutdown) {
		/* We were explicitly requested to shutdown. */
		ret = -ESHUTDOWN;
		goto free;
	}

	if (!*ln) {
		/* Empty input line... */
		ret = -ENODATA;
		goto free;
	}

	len = strnlen(ln, CLI_LINE_MAX);
	if (len >= CLI_LINE_MAX) {
		cli_log("invalid input line: too long.");
		ret = -ENOBUFS;
		goto free;
	}

	if (shell->hpath)
		add_history(ln);

	*line = ln;

	return 0;

free:
	cli_free(ln);

	return ret;
}

#if 0
/* Keep this just in case we need to save parsed line into history. */
static void
cli_shell_hist_expr(const struct cli_expr_blk * block)
{
	cli_expr_blk_assert(block);

	char *  ln;
	ssize_t len;

	ln = cli_malloc(CLI_LINE_MAX);
	cli_assert(ln);

	len = cli_expr_blk_make_string(block, ln, CLI_LINE_MAX);
	if (len < 0) {
		cli_log("cannot log into history: expression too long.");
		goto free;
	}

	add_history(ln);

free:
	cli_free(ln);
}
#endif

int
cli_shell_read_expr(const struct cli_shell * shell,
                    struct cli_expr_blk *    expr_block)
{
	cli_shell_assert(shell);
	cli_expr_blk_assert(expr_block);

	char * ln;
	int    ret;

	ret = cli_shell_read_line(shell, &ln);
	if (ret < 0)
		return ret;

	ret = cli_expr_blk_parse_line(expr_block, ln);
	if (ret < 0)
		goto free;

	return 0;

free:
	cli_free(ln);

	return ret;
}

static void
_cli_shell_set_prompt(struct cli_shell * shell, const char * prompt)
{
	cli_assert(shell);
	cli_assert(shell->pref);
	cli_assert(prompt);
	cli_assert(*prompt);

	int ret;

	ret = cli_asprintf(&shell->prompt,
	                   "%s%s> ",
	                   shell->pref,
	                   prompt);
	cli_assert(ret >= 4);
}

void
cli_shell_set_prompt(struct cli_shell * shell, const char * prompt)
{
	cli_shell_assert(shell);
	cli_assert(prompt);
	cli_assert(*prompt);

	cli_free(shell->prompt);
	_cli_shell_set_prompt(shell, prompt);
}

static char *
cli_shell_init_hist(void)
{
	const char * bdir; /* Base directory path. */
	const char * name = program_invocation_short_name;
	char *       cdir; /* Configuration directory path. */
	char *       hpath;
	int          err;

	bdir = secure_getenv("XDG_CONFIG_HOME");
	if (!bdir) {
		bdir = secure_getenv("HOME");
		if (!bdir) {
			err = EINVAL;
			goto out;
		}

		cli_asprintf(&cdir, "%s/.config/%s", bdir, name);
	}
	else
		cli_asprintf(&cdir, "%s/%s", bdir, name);

	if (mkdir(cdir, S_IRUSR | S_IWUSR)) {
		cli_assert(errno != EFAULT);
		if (errno != EEXIST) {
			err = errno;
			goto free;
		}
	}

	cli_asprintf(&hpath, "%s/history", cdir);
	read_history(hpath);
	cli_free(cdir);

	return hpath;

free:
	cli_free(cdir);
out:
	cli_log("cannot setup history: %s", strerror(err));

	return NULL;
}

static void
cli_shell_fini_hist(struct cli_shell * shell)
{
	cli_shell_assert(shell);
	cli_assert(shell->hpath);

	write_history(shell->hpath);

	cli_free(shell->hpath);
}

static char **
cli_shell_build_matches(struct cli_shell *      shell,
                        const struct cli_expr * expression,
                        const char *            word,
                        size_t                  length)
{
#warning Implement me!
	return NULL;
}

static char **
cli_shell_complete(const char * word, int start, int end)
{
	cli_assert(word);
	cli_assert(start >= 0);
	cli_assert(end >= 0);
	cli_assert(start <= end);
	cli_assert(end <= rl_end);
	cli_assert(strlen(word) == (size_t)(end - start));

	struct cli_exp expr;
	char **        match = NULL;

	cli_expr_init(&expr);

	if (start) {
		int    begin;
		char * ln;

		if (((size_t)end >= CLI_LINE_MAX) ||
		    ((size_t)(end - start) >= CLI_ARG_MAX))
			return NULL;

		/*
		 * Probe for start of last command within the current command
		 * line...
		 */
		begin = end;
		while (begin && (rl_line_buffer[begin - 1] != ';'))
			begin--;
		/*
		 * ...and duplicate the command up to the word to complete into
		 * a newly allocated string.
		 */
		ln = cli_malloc((end - begin) + 1);
		cli_assert(ln);
		memcpy(ln, &rl_line_buffer[begin], end - begin);
		ln[end - begin] = '\0';

		ret = cli_expr_parse_string(&expr, ln);
		if (!ret)
			match = cli_shell_build_matches(shell,
			                                &expr,
			                                word,
			                                (size_t)(end - start));
		cli_expr_fini(&expr);
		cli_free(ln);

		return match;
	}

	cli_expr_fini(&expr);

	return match;
}

/* Disable readline's default completion logic. */
static char *
cli_shell_null_complete(const char *word __cli_unused, int len __cli_unused)
{
	return NULL;
}

static void
cli_shell_setup_compl(bool enable)
{
	if (enable) {
		/*
		 * Install our own cli_shell_complete() completion function and
		 * disable readline's default completion logic.
		 */
		rl_attempted_completion_function = cli_shell_complete;
		rl_completion_entry_function = cli_shell_null_complete;
		rl_inhibit_completion = 0;
	}
	else
		rl_inhibit_completion = 1;
}

int
cli_shell_init(struct cli_shell * shell, bool complete, bool history)
{
	const char * user;
	char *       host;
	int          ret;

	user = secure_getenv("USER");
	if (!user) {
		cli_log("cannot setup shell: %s.", strerror(EINVAL));
		return -EINVAL;
	}

	host = cli_malloc(HOST_NAME_MAX + 1);
	cli_assert(host);
	ret = gethostname(host, HOST_NAME_MAX + 1);
	cli_assert(!ret);

	ret = cli_asprintf(&shell->pref, "%s@%s:", user, host);
	cli_assert(ret >= 4);

	cli_free(host);

	_cli_shell_set_prompt(shell, "/");
	shell->hpath = NULL;
	shell->shutdown = 0;

	cli_shell_setup_compl(complete);

	if (history)
		shell->hpath = cli_shell_init_hist();

	rl_readline_name = program_invocation_short_name;

	return 0;
}

void
cli_shell_fini(struct cli_shell * shell)
{
	if (shell->hpath)
		cli_shell_fini_hist(shell);

	cli_free(shell->pref);
	cli_free(shell->prompt);
}
