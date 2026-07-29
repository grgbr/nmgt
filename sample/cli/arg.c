#include "arg.h"
#include "shell.h"
#include "cmd.h"
#include "dir.h"
#include "cli.h"
#include <string.h>

/******************************************************************************
 * Argument utilities
 ******************************************************************************/

static const char cli_arg_valid_chrs[] = "abcdefghijklmnopqrstuvwxyz"
                                         "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                         "0123456789"
                                         " /-_.=,:@";

#if 0
/* Keep this just in case we would need it. */
static bool
cli_arg_ischr_valid(int chr)
{
	return !!memchr(cli_arg_valid_chrs,
	                chr,
	                sizeof(cli_arg_valid_chrs) - 1);
}
#endif

size_t
_cli_arg_isstr_valid(const char * string, size_t length)
{
	cli_assert(string);
	cli_assert(length);
	cli_assert(length < CLI_ARG_MAX);
	cli_assert(strnlen(string, length) == length);

	size_t len = strspn(string, cli_arg_valid_chrs);

	return (string[len] == '\0') ? 0 : len;
}

/******************************************************************************
 * Base argument handling
 ******************************************************************************/

#define cli_arg_foreach_child(_arg, _child) \
	for (_child = (const struct cli_arg *)((_arg)->super.child); \
	     _child; \
	     _child = (const struct cli_arg *)((_child)->super.next))

static void
cli_arg_setup(struct cli_arg * argument, const struct cli_arg_ops * opers)
{
	cli_assert(argument);
	cli_arg_assert_ops(opers);

	cli_node_setup(&argument->super);
	argument->ops = opers;
}

struct cli_arg *
cli_arg_create(size_t size, const struct cli_arg_ops * opers)
{
	cli_assert(size >= sizeof(struct cli_arg));
	cli_arg_assert_ops(opers);

	struct cli_arg * arg;

	arg = cli_malloc(size);
	cli_assert(arg);

	cli_arg_setup(arg, opers);

	return arg;
}

/******************************************************************************
 * Parameter argument handling
 ******************************************************************************/

#define cli_arg_assert_parm(_parm) \
	cli_assert(_parm); \
	cli_arg_assert(&(_parm)->super); \
	cli_assert((_parm)->name); \
	cli_assert((_parm)->len); \
	cli_assert((_parm)->len < CLI_ARG_MAX); \
	cli_assert(strnlen((_parm)->name, CLI_ARG_MAX) == (_parm)->len)

static int
cli_arg_setup_parm(struct cli_arg_parm *      parameter,
                   const char *               name,
                   const struct cli_arg_ops * opers)
{
	cli_assert(parameter);
	cli_assert(name);
	cli_arg_assert_ops(opers);

	size_t len;

	len = strnlen(name, CLI_ARG_MAX);
	if (!len)
		return -ENODATA;
	if (len == CLI_ARG_MAX)
		return -ENAMETOOLONG;

	cli_arg_setup(&parameter->super, opers);
	parameter->name = name;
	parameter->len = len;

	return 0;
}

int
cli_arg_create_parm(struct cli_arg_parm **     parameter,
                    const char *               name,
                    const struct cli_arg_ops * opers)
{
	cli_assert(parameter);
	cli_assert(name);
	cli_arg_assert_ops(opers);

	struct cli_arg_parm * parm;
	int                   ret;

	parm = cli_malloc(sizeof(*parm));
	cli_assert(parm);

	ret = cli_arg_setup_parm(parm, name, opers);
	if (!ret) {
		*parameter = parm;
		return 0;
	}

	cli_free(parm);

	return ret;
}

/******************************************************************************
 * Choice argument handling
 ******************************************************************************/

static int
cli_arg_parse_choice(const struct cli_arg * choice,
                     const struct cli_cmd * command,
                     const struct cli_dir * directory,
                     struct cli_context *   context,
                     int                    argc,
                     const char * const     argv[],
                     void *                 data)
{
	cli_arg_assert(choice);
	cli_cmd_assert(command);
	cli_dir_assert(directory);
	cli_assert_context(context);
	cli_assert(argc >= 1);
	cli_assert(argv[0]);
	cli_assert(strnlen(argv[0], CLI_ARG_MAX) < CLI_ARG_MAX);

	const struct cli_node * opt;

	cli_node_foreach_child(&choice->super, opt) {
		int ret;

		ret = cli_arg_parse((const struct cli_arg *)opt,
		                    command,
		                    directory,
		                    context,
		                    1,
		                    &argv[0],
		                    data);
		cli_assert(ret <= 1);
		if (ret)
			return ret;
	}

	cli_cmd_log(command, "'%s': invalid argument.", argv[0]);

	return -EINVAL;
}

static void
cli_arg_complete_choice(const struct cli_arg * choice,
                        const struct cli_cmd * command,
                        const struct cli_dir * directory,
                        struct cli_context *   context,
                        const char *           word,
                        size_t                 length,
                        int                    argc,
                        const char * const     argv[],
                        struct cli_match *     matches)
{
	cli_arg_assert(choice);
	cli_cmd_assert(command);
	cli_dir_assert(directory);
	cli_assert_context(context);
	cli_match_assert(matches);

	const struct cli_node * opt;

	cli_node_foreach_child(&choice->super, opt)
		cli_arg_complete((const struct cli_arg *)opt,
		                 command,
		                 directory,
		                 context,
		                 word,
		                 length,
		                 argc,
		                 argv,
		                 matches);
}

static const struct cli_arg_ops cli_arg_choice_ops = {
	.parse    = cli_arg_parse_choice,
	.complete = cli_arg_complete_choice
};

struct cli_arg *
cli_arg_create_choice(void)
{
	return cli_arg_create(sizeof(struct cli_arg), &cli_arg_choice_ops);
}

struct cli_arg *
cli_arg_createn_add_choice(struct cli_node * cmd_or_arg)
{
	cli_node_assert(cmd_or_arg);

	struct cli_arg * choice;

	choice = cli_arg_create_choice();
	cli_assert(choice);
	cli_node_add_child(cmd_or_arg, &choice->super);

	return choice;
}

/******************************************************************************
 * Keyword parameter argument handling
 ******************************************************************************/

#define cli_arg_assert_kword_term(_term) \
	cli_assert(_term); \
	cli_assert((_term)->value); \
	cli_assert((_term)->len); \
	cli_assert((_term)->len < CLI_ARG_MAX); \
	cli_assert(strnlen((_term)->value, CLI_ARG_MAX) == (_term)->len)

#define cli_arg_assert_kword_parm(_parm) \
	cli_assert(_parm); \
	cli_arg_assert_parm(&(_parm)->super); \
	cli_assert((_parm)->nr); \
	cli_assert(&(_parm)->super); \
	cli_arg_assert_terms((_parm)->terms, (_parm)->nr)

#if defined(CONFIG_CLI_ASSERT)

static void
cli_arg_assert_terms(const struct cli_arg_kword_term * terminals,
                     unsigned int                      nr)
{
	cli_assert(terminals);
	cli_assert(nr);

	unsigned int t;

	for (t = 0; t < nr; t++) {
		cli_arg_assert_kword_term(&terminals[t]);
	}
}

#endif /* defined(CONFIG_CLI_ASSERT) */

static int
cli_arg_parse_kword_term(const struct cli_arg_kword_term * terminal,
                         const struct cli_arg_kword_parm * parameter,
                         const struct cli_cmd *            command,
                         const struct cli_dir *            directory,
                         struct cli_context *              context,
                         const char *                      string,
                         void *                            data)
{
	cli_arg_assert_kword_term(terminal);

	size_t len;

	len = strnlen(string, terminal->len + 1);
	if ((len != terminal->len) ||
	    memcmp(string, terminal->value, terminal->len))
		return -EINVAL;

	return terminal->on_match((const struct cli_arg *)parameter,
	                          command,
	                          directory,
	                          context,
	                          1,
	                          &string,
	                          data);
}

static int
cli_arg_parse_kword_parm(const struct cli_arg * argument,
                         const struct cli_cmd * command,
                         const struct cli_dir * directory,
                         struct cli_context *   context,
                         int                    argc,
                         const char * const     argv[],
                         void *                 data)
{
	cli_arg_assert_kword_parm((const struct cli_arg_kword_parm *)argument);
	cli_cmd_assert(command);
	cli_dir_assert(directory);
	cli_assert_context(context);
	cli_assert(argc == 1);
	cli_assert(argv[0]);
	cli_assert(strnlen(argv[0], CLI_ARG_MAX) < CLI_ARG_MAX);

	const struct cli_arg_kword_parm * parm =
		(const struct cli_arg_kword_parm *)argument;
	const char *                      str = argv[0];
	size_t                            len;
	unsigned int                      t;

	len = strcspn(str, "=");
	if ((len != parm->super.len) ||
	    (str[len] != '=') ||
	    memcmp(str, parm->super.name, parm->super.len))
		return 0;

	str = &str[len + 1];
	if (*str == '\0') {
		cli_cmd_log(command,
		            "'%s': missing parameter keyword.",
		            parm->super.name);
		return -EINVAL;
	}

	for (t = 0; t < parm->nr; t++) {
		int ret;

		ret = cli_arg_parse_kword_term(&parm->terms[t],
		                               parm,
		                               command,
		                               directory,
		                               context,
		                               str,
		                               data);
		cli_assert(ret <= 0);
		if (!ret)
			/* Matched successfully. */
			return 1;
	}

	cli_cmd_log(command, "'%s': invalid %s parameter keyword.",
	            str,
	            parm->super.name);

	return -EINVAL;
}

static void
cli_arg_display_kword_list(char ** matches,
                           int     count,
                           int     max_length,
                           void *  data)
{
	cli_arg_assert_kword_parm((const struct cli_arg_kword_parm *)data);

	char **                           list;
	int                               c;
	const struct cli_arg_kword_parm * parm =
		(const struct cli_arg_kword_parm *)data;

	list = cli_malloc((count + 2) * sizeof(list[0]));
	cli_assert(list);

	for (c = 0; c <= count; c++)
		list[c] = &matches[c][parm->super.len + 1];
	list[count + 1] = NULL;

	rl_display_match_list(list,
	                      count,
	                      max_length - (parm->super.len + 1));

	cli_free(list);
}

static void
cli_arg_complete_kword_term(const struct cli_arg_kword_term * terminal,
                            const struct cli_arg_kword_parm * parameter,
                            const char *                      word,
                            size_t                            length,
                            struct cli_match *                matches)
{
	cli_arg_assert_kword_term(terminal);

	if (!length ||
	    ((length <= terminal->len) &&
	     !memcmp(word, terminal->value, length))) {
		char * kw;
		int    ret;

		ret = cli_asprintf(&kw,
		                   "%s=%s",
		                   parameter->super.name,
		                   terminal->value);
		cli_assert((size_t)ret ==
		           (parameter->super.len + 1 + terminal->len));

		/*
		 * Give ownership of `kw' to `matches'. Allocated string
		 * ownership will be transfered from `matches' to readline(3) by
		 * cli_shell_complete().
		 * Readline(3) will free(3) it at completion process termination
		 * time.
		 */
		cli_match_push(matches, kw);
	}
}

static void
cli_arg_complete_kword_parm(const struct cli_arg * argument,
                            const struct cli_cmd * command __cli_unused,
                            const struct cli_dir * directory __cli_unused,
                            struct cli_context *   context __cli_unused,
                            const char *           word,
                            size_t                 length,
                            int                    argc __cli_unused,
                            const char * const     argv[] __cli_unused,
                            struct cli_match *     matches)
{
	cli_arg_assert_kword_parm((const struct cli_arg_kword_parm *)argument);
	cli_cmd_assert(command);
	cli_dir_assert(directory);
	cli_assert_context(context);
	cli_match_assert(matches);

	const struct cli_arg_kword_parm * parm =
		(const struct cli_arg_kword_parm *)argument;

	if (length <= parm->super.len) {
		if (!length || !memcmp(word, parm->super.name, length)) {
			char * name;
			int    ret;

			ret = cli_asprintf(&name, "%s=", parm->super.name);
			cli_assert((size_t)ret == (parm->super.len + 1));

			/*
			 * Give ownership of `name' to `matches'. Allocated
			 * string ownership will be transfered from `matches' to
			 * readline(3) by cli_shell_complete().
			 * Readline(3) will free(3) it at completion process
			 * termination time.
			 */
			cli_match_push(matches, name);

			/*
			 * Disable appending the completion character after a
			 * selected match.
			 */
			cli_shell_suppress_complete_char();
		}
	}
	else if (!memcmp(word, parm->super.name, parm->super.len) &&
		 (word[parm->super.len] == '=')) {
		unsigned int t;

		word = &word[parm->super.len + 1];
		length -= parm->super.len + 1;

		for (t = 0; t < parm->nr; t++)
			cli_arg_complete_kword_term(&parm->terms[t],
			                            parm,
			                            word,
			                            length,
			                            matches);

		if (cli_match_count(matches))
			cli_shell_enroll_display_compl(
				cli_arg_display_kword_list,
				(void *)parm);
	}
}

static const struct cli_arg_ops cli_arg_kword_parm_ops = {
	.parse    = cli_arg_parse_kword_parm,
	.complete = cli_arg_complete_kword_parm
};

int
cli_arg_create_kword_parm(struct cli_arg_kword_parm **      parameter,
                          const char *                      name,
                          const struct cli_arg_kword_term * terminals,
                          unsigned int                      nr)
{
	cli_assert(parameter);
	cli_assert(name);
	cli_arg_assert_terms(terminals, nr);

	struct cli_arg_kword_parm * parm;
	int                         ret;

	parm = cli_malloc(sizeof(*parm));
	cli_assert(parm);

	ret = cli_arg_setup_parm(&parm->super, name, &cli_arg_kword_parm_ops);
	if (!ret) {
		parm->nr = nr;
		parm->terms = terminals;
		*parameter = parm;

		return 0;
	}

	cli_free(parm);

	return ret;
}

int
cli_arg_createn_add_kword_parm(struct cli_arg_kword_parm **      parameter,
                               const char *                      name,
                               const struct cli_arg_kword_term * terminals,
                               unsigned int                      nr,
                               struct cli_node *                 cmd_or_arg)
{
	cli_assert(parameter);
	cli_assert(name);
	cli_arg_assert_terms(terminals, nr);
	cli_node_assert(cmd_or_arg);

	int ret;

	ret = cli_arg_create_kword_parm(parameter, name, terminals, nr);
	if (!ret) {
		cli_node_add_child(cmd_or_arg,
		                   *(struct cli_node **)parameter);
		return 0;
	}

	return ret;
}
