#include "shell.h"
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

	*line = ln;

	return 0;

free:
	cli_free(ln);

	return ret;
}

static int
cli_shell_break_expr(char *** words, char * line)
{
	cli_assert(words);
	cli_assert(line);

	char **      toks;
	unsigned int nr;
	unsigned int pos = 0;
	unsigned int cnt = 0;
	bool         done = false;
	int          ret;

	nr = 8;
	toks = cli_malloc((nr + 1) * sizeof(toks[0]));
	cli_assert(toks);

	do {
		size_t wlen;

		pos += strspn(&line[pos], " \t\n\r\f\v");
		if (line[pos] == '\0')
			/* End of input line. */
			break;

		wlen = strcspn(&line[pos], " \t\n\r\f\v");
		cli_assert(wlen);

		if (line[pos + wlen] == '\0')
			done = true;
		else
			line[pos + wlen] = '\0';

		cli_assert(cnt <= nr);
		if (cnt == nr) {
			nr *= 2;
			toks = cli_realloc(toks, (nr * sizeof(toks[0])));
			cli_assert(toks);
		}

		toks[cnt++] = &line[pos];

		pos += wlen + 1;
	} while (!done);

	if (!cnt) {
		ret = 0;
		goto free;
	}

	toks[cnt] = NULL;
	*words = toks;

	return cnt;

free:
	cli_free(toks);

	return ret;
}

static void
cli_shell_hist_expr(const struct cli_shell_expr * expr)
{
	cli_assert(expr);
	cli_assert(expr->nr);
	cli_assert(expr->words);
	cli_assert(expr->ln);

	unsigned int   w;
	char         * ln;
	char         * ptr;

	ln = cli_malloc(CLI_LINE_MAX);
	cli_assert(ln);

	ptr = stpcpy(ln, expr->words[0]);
	cli_assert((size_t)(ptr - ln) < CLI_LINE_MAX);

	for (w = 1; w < expr->nr; w++) {
		*ptr++ = ' ';
		cli_assert((size_t)(ptr - ln) < CLI_LINE_MAX);

		ptr = stpcpy(ptr, expr->words[w]);
		cli_assert((size_t)(ptr - ln) < CLI_LINE_MAX);
	}

	add_history(ln);

	cli_free(ln);
}

int
cli_shell_read_expr(const struct cli_shell * shell,
                    struct cli_shell_expr *  expr)
{
	cli_shell_assert(shell);
	cli_assert(expr);

	char * ln;
	int    ret;

	ret = cli_shell_read_line(shell, &ln);
	if (ret < 0)
		return ret;

	ret = cli_shell_break_expr(&expr->words, ln);
	if (ret <= 0) {
		ret = !ret ? -ENODATA : ret;
		goto free;
	}

	expr->nr = ret;
	expr->ln = ln;

	if (shell->hpath)
		cli_shell_hist_expr(expr);

	return 0;

free:
	cli_free(ln);

	return ret;
}

void
cli_shell_release_expr(const struct cli_shell_expr * expr)
{
	cli_assert(expr);
	cli_assert(expr->nr);
	cli_assert(expr->words);
	cli_assert(expr->ln);

	cli_free(expr->words);
	cli_free(expr->ln);
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

int
cli_shell_init(struct cli_shell * shell, bool history)
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

	rl_inhibit_completion = 1;

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
