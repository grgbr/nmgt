#include "shell.h"
#include "expr.h"
#include "arg.h"
#include <readline/history.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

struct cli_shell {
	char *                       prompt;
	char *                       pref;
	void *                       data;
	cli_shell_collect_compl_fn * collect_compl;
	struct cli_match             matches;
	cli_shell_display_compl_fn * display_compl;
	void *                       display_data;
	volatile sig_atomic_t        shutdown;
	char *                       hpath;
};

#define cli_shell_assert(_shell) \
	cli_assert((_shell)->prompt); \
	cli_assert((_shell)->pref); \
	cli_assert(!(_shell)->collect_compl || (_shell)->display_compl)

/* TODO: make shell a singleton ?? */
struct cli_shell cli_the_shell;

void
cli_shell_enroll_display_compl(cli_shell_display_compl_fn * display,
                               void *                       data)
{
	cli_shell_assert(&cli_the_shell);
	cli_assert(display);

	cli_the_shell.display_compl = display;
	cli_the_shell.display_data = data;
}

void
cli_shell_shutdown(void)
{
	cli_shell_assert(&cli_the_shell);

	cli_the_shell.shutdown = 1;
}

static int
cli_shell_read_line(const struct cli_shell * shell, char ** line)
{
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

	if (ln[strspn(ln, " \t")] == '\0') {
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
cli_shell_read_expr(struct cli_expr_blk * expr_block)
{
	cli_shell_assert(&cli_the_shell);
	cli_expr_blk_assert(expr_block);

	char * ln;
	int    ret;

	ret = cli_shell_read_line(&cli_the_shell, &ln);
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
	cli_assert(shell->pref);
	cli_assert(prompt);
	cli_assert(*prompt);

	int ret;

	ret = cli_asprintf(&shell->prompt, "%s%s> ", shell->pref, prompt);
	cli_assert(ret >= 4);
}

void
cli_shell_set_prompt(const char * prompt)
{
	cli_shell_assert(&cli_the_shell);
	cli_assert(prompt);
	cli_assert(*prompt);

	cli_free(cli_the_shell.prompt);
	_cli_shell_set_prompt(&cli_the_shell, prompt);
}

static char *
cli_shell_init_hist(void)
{
	const char * bdir; /* Base directory path. */
	const char * name = program_invocation_short_name;
	char *       cdir; /* Configuration directory path. */
	char *       hpath;
	int          ret;

	bdir = secure_getenv("XDG_CONFIG_HOME");
	if (!bdir) {
		bdir = secure_getenv("HOME");
		if (!bdir) {
			ret = EINVAL;
			goto out;
		}

		cli_asprintf(&cdir, "%s/.config/%s", bdir, name);
	}
	else
		cli_asprintf(&cdir, "%s/%s", bdir, name);

	if (mkdir(cdir, S_IRUSR | S_IWUSR)) {
		cli_assert(ret != EFAULT);
		if (errno != EEXIST) {
			ret = errno;
			goto free;
		}
	}

	ret = cli_asprintf(&hpath, "%s/history", cdir);
	cli_assert(ret > 0);
	cli_assert(hpath);

	read_history(hpath);
	cli_free(cdir);

	return hpath;

free:
	cli_free(cdir);
out:
	cli_log("cannot setup history: %s", strerror(ret));

	return NULL;
}

static void
cli_shell_fini_hist(struct cli_shell * shell)
{
	cli_assert(shell->hpath);

	write_history(shell->hpath);

	cli_free(shell->hpath);
}

static void
cli_shell_display_match_list(char ** matches,
                             int     count,
                             int     max_length,
                             void *  data __cli_unused)
{
	rl_display_match_list(matches, count, max_length);
}

static void
cli_shell_display_match(char ** matches, int count, int max_length)
{
	/*
	 * Do not honor the `rl_completion_query_items' setting for now since
	 * current readline(3)'s get_y_or_n() implementation is not ready for
	 * rl_callback_handler_install() / alternate interface support yet
	 * (as of version 8.3).
	 * Readline(3) pager logic will still be thrown when the number of
	 * completion items requires it.
	 */

	cli_the_shell.display_compl(matches,
	                            count,
	                            max_length,
	                            cli_the_shell.display_data);

	rl_forced_update_display();
	extern int rl_display_fixed;
	rl_display_fixed = 1;
}

static char *
cli_shell_generate_match(const char * word __cli_unused, int state)
{
	cli_assert(word);
	cli_assert(state >= 0);

	return cli_match_get(&cli_the_shell.matches, state);
}

static char **
cli_shell_complete(const char * word, int begin, int end)
{
	cli_shell_assert(&cli_the_shell);
	cli_assert(word);
	cli_assert(begin >= 0);
	cli_assert(end >= 0);
	cli_assert(begin <= end);
	cli_assert(end <= rl_end);
	cli_assert(strlen(word) == (size_t)(end - begin));

	char ** match = NULL;

	/* Restore default completion matches display behavior. */
	cli_the_shell.display_compl = cli_shell_display_match_list;

	if (((size_t)end < CLI_LINE_MAX) &&
	    ((size_t)(end - begin) < CLI_ARG_MAX)) {
		int              first = begin;
		int              last = begin;
		struct cli_expr  expr;

		/*
		 * Probe for the begining of last command within the current
		 * line, then get rid of leading spaces from there...
		 */
		first = begin;
		while (first && (rl_line_buffer[first - 1] != ';'))
			first--;
		first += strspn(&rl_line_buffer[first], " \t");
		cli_assert(first <= begin);

		/* ...in addition, also get rid of trailing spaces. */
		while (last &&
		       (rl_line_buffer[last - 1] == ' ' ||
		        rl_line_buffer[last - 1] == '\t'))
			last--;
		cli_assert(last <= begin);
		cli_assert(last >= first);

		cli_match_init(&cli_the_shell.matches);

		if (last - first) {
			char * ln;

			/*
			 * ...then duplicate the command up and including the
			 * word preceding the word to complete into a newly
			 * allocated string.
			 */
			ln = cli_malloc((last - first) + 1);
			cli_assert(ln);
			memcpy(ln, &rl_line_buffer[first], last - first);
			ln[last - first] = '\0';

			cli_expr_init(&expr);
			if (!cli_expr_parse_string(&expr, ln))
				cli_the_shell.collect_compl(
					&cli_the_shell.matches,
					word,
					(size_t)(end - begin),
					cli_expr_arg_cnt(&expr),
					cli_expr_args(&expr),
					cli_the_shell.data);
			cli_expr_fini(&expr);

			cli_free(ln);
		}
		else
			cli_the_shell.collect_compl(&cli_the_shell.matches,
			                            word,
			                            (size_t)(end - begin),
			                            0,
			                            NULL,
			                            cli_the_shell.data);
		if (cli_match_count(&cli_the_shell.matches))
			match = rl_completion_matches(word,
			                              cli_shell_generate_match);
		cli_match_fini(&cli_the_shell.matches);
	}

	/*
	 * Give ownership of matched strings to readline(3). It will free(3)
	 * them at completion process termination time.
	 */
	return match;
}

/* Disable readline's default completion logic. */
static char *
cli_shell_null_complete(const char * word __cli_unused, int len __cli_unused)
{
	return NULL;
}

static void
cli_shell_setup_compl(struct cli_shell *           shell,
                      cli_shell_collect_compl_fn * complete)
{
	cli_assert(shell);

	if (complete) {
		/*
		 * Install our own cli_shell_complete() completion function and
		 * disable readline's default completion logic.
		 */
		rl_completion_entry_function = cli_shell_null_complete;
		shell->collect_compl = complete;
		rl_attempted_completion_function = cli_shell_complete;

		/*
		 * In addition install our own completion display function to
		 * give the command / argument completion logic an opportunity
		 * to override default readline(3) behavior.
		 */
		shell->display_compl = cli_shell_display_match_list;
		rl_completion_display_matches_hook = cli_shell_display_match;

		/* Enable completion. */
		rl_inhibit_completion = 0;
	}
	else
		/* Disable completion entirely. */
		rl_inhibit_completion = 1;
}

int
cli_shell_init(bool                         history,
               const char *                 break_chars,
               cli_shell_collect_compl_fn * complete,
               void *                       data)

{
	cli_assert(!break_chars || (break_chars[0] != '\0'));

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

	ret = cli_asprintf(&cli_the_shell.pref, "%s@%s:", user, host);
	cli_assert(ret >= 4);

	cli_free(host);

	_cli_shell_set_prompt(&cli_the_shell, "/");
	cli_the_shell.hpath = NULL;
	cli_the_shell.shutdown = 0;

	/* Setup the list of characters that signal a break between words. */
	if (break_chars)
		rl_basic_word_break_characters = break_chars;
	
	cli_shell_setup_compl(&cli_the_shell, complete);
	cli_the_shell.data = data;

	if (history)
		cli_the_shell.hpath = cli_shell_init_hist();

	rl_readline_name = program_invocation_short_name;

	return 0;
}

void
cli_shell_fini(void)
{
	cli_shell_assert(&cli_the_shell);

	if (cli_the_shell.hpath)
		cli_shell_fini_hist(&cli_the_shell);

	cli_free(cli_the_shell.pref);
	cli_free(cli_the_shell.prompt);
}
