#include "expr.h"
#include "arg.h"

/******************************************************************************
 * Expression utilities
 ******************************************************************************/

static ssize_t
cli_expr_unquote_arg(char ** argument, size_t length)
{
	cli_assert(argument);
	cli_assert(*argument);
	cli_assert(length);
	cli_assert(strnlen(*argument, CLI_ARG_MAX) == length);

	char * arg = *argument;
	
	if ((arg[0] != '"') && (arg[0] != '\''))
		return (ssize_t)length;

	cli_assert(arg[length - 1] == arg[0]);
	arg[length - 1] = '\0';
	*argument = &arg[1];

	return length - 2;
}

/******************************************************************************
 * Base command line expression handling.
 ******************************************************************************/

#define CLI_EXPR_ARGS_INIT (4U)

static int
cli_expr_push_arg(struct cli_expr * expression,
                  char *            argument,
                  size_t            length)
{
	cli_expr_assert(expression);
	cli_assert(argument);
	cli_assert(length);

	char * arg;
	size_t inval;
	char   rnd[CLI_RENDER_MAX];

	if (length >= CLI_ARG_MAX) {
		cli_log("cannot parse: '%s': expression argument too long.",
		        cli_render_string(argument, rnd));
		return -ENAMETOOLONG;
	}

	cli_assert(strnlen(argument, CLI_ARG_MAX) == length);

	arg = argument;
	length = cli_expr_unquote_arg(&arg, length);
	inval = _cli_arg_isstr_valid(arg, length);
	if (inval) {
		cli_log("cannot parse: '%s': "
		        "invalid expression argument character '%c'.",
		        cli_render_string(arg, rnd),
		        cli_render_chr(arg[inval]));

		return -EINVAL;
	}

	if (expression->cnt == expression->nr) {
		unsigned int nr = expression->nr * 2;

		if (nr >= CLI_EXPR_ARGS_MAX) {
			cli_log("cannot parse: too many expression arguments.");
			return -ENOBUFS;
		}

		expression->nr = nr;
		expression->args = cli_realloc(expression->args,
		                               nr * sizeof(argument));
	}

	expression->args[expression->cnt++] = arg;

	return 0;
}

int
cli_expr_parse_string(struct cli_expr * expression, char * string)
{
	cli_expr_assert(expression);
	cli_assert(string);
	cli_assert(*string != '\0');
	cli_assert(strnlen(string, CLI_LINE_MAX) < CLI_LINE_MAX);
	cli_assert(!strchr(string, '\n'));

	char * str = string;
	int    ret;

	do {
		char * tok;

		str += strspn(str, " \t");
		if (*str == '\0')
			return 0;

		tok = str;
		str += strcspn(str, " \t\"'");
		if ((*str == '"') || (*str == '\'')) {
			/* Search for closing delimiter. */
			char * delim;

			delim = strchr(str + 1, *str);
			if (!delim) {
				char rnd[CLI_RENDER_MAX];

				cli_log("cannot parse: '%s': "
				        "missing '%c' closing delimiter.",
				        cli_render_string(str, rnd),
				        *str);
				return -EINVAL;
			}

			str = delim + 1;
		}

		switch (*str) {
		case ' ':
		case '\t':
			*str = '\0';
			ret = cli_expr_push_arg(expression, tok, str - tok);
			if (ret)
				return ret;
			str++;
			break;

		case '\0':
			return cli_expr_push_arg(expression, tok, str - tok);

		default:
			return -EINVAL;
		}
	} while (*str != '\0');

	return 0;
}

#if 0
/* Keep this just in case we need to save parsed line into history. */
static ssize_t
cli_expr_make_string(const struct cli_expr * expression,
                     char *                  string,
                     size_t                  size)
{
	cli_expr_assert(expression);
	cli_assert(string);
	cli_assert(size);
	cli_assert(size <= SSIZE_MAX);

	ssize_t      tlen; /* Total length. */
	unsigned int a;

	tlen = strlen(expression->args[0]);
	cli_assert(tlen < CLI_ARG_MAX);
	if ((size_t)tlen >= size)
		return -ENOBUFS;
	memcpy(string, expression->args[0], tlen);

	for (a = 1; a < expression->cnt; a++) {
		const char * arg = expression->args[a]; /* Argument to join. */
		size_t       alen;                      /* Argument length. */

		alen = strlen(arg);
		cli_assert(alen < CLI_ARG_MAX);
		if (((size_t)tlen + 1 + alen) >= size)
			return -ENOBUFS;

		string[tlen++] = ' ';
		memcpy(&string[tlen], arg, alen);

		tlen += alen;
		cli_assert((size_t)tlen < size);
	}

	string[tlen] = '\0';

	return tlen;
}
#endif

void
cli_expr_init(struct cli_expr * expression)
{
	cli_assert(expression);

	expression->next = NULL;
	expression->cnt = 0;
	expression->nr = CLI_EXPR_ARGS_INIT;
	expression->args = cli_malloc(CLI_EXPR_ARGS_INIT *
	                              sizeof(expr->args[0]));
	cli_assert(expr->args);
}

static struct cli_expr *
cli_expr_create(void)
{
	struct cli_expr * expr;

	expr = cli_malloc(sizeof(*expr));
	cli_assert(expr);

	cli_expr_init(expr);

	return expr;
}

void
cli_expr_fini(struct cli_expr * expression)
{
	cli_expr_assert(expression);

	cli_free(expression->args);
}

static void
cli_expr_destroy(struct cli_expr * expression)
{
	cli_expr_assert(expression);

	cli_expr_fini(expression);

	cli_free(expression);
}

static int
cli_expr_createn_parse(struct cli_expr ** expression, char * string)
{
	cli_assert(expression);
	cli_assert(string);

	struct cli_expr * expr;
	int               ret;

	expr = cli_expr_create();
	cli_assert(expr);

	ret = cli_expr_parse_string(expr, string);
	if (ret) {
		cli_expr_destroy(expr);
		return ret;
	}

	*expression = expr;

	return 0;
}

/******************************************************************************
 * Command line expression block / sequence handling.
 ******************************************************************************/

static void
cli_expr_blk_push(struct cli_expr_blk * block, struct cli_expr * expression)
{
	cli_expr_blk_assert(block);
	cli_expr_assert(expression);
	cli_assert(!expression->next);
	cli_assert(block->cnt < CLI_EXPR_BLK_MAX);

	block->cnt++;
	*block->tail = expression;
	block->tail = &expression->next;
}

int
cli_expr_blk_parse_line(struct cli_expr_blk * block, char * line)
{
	cli_expr_blk_assert(block);
	cli_assert(!block->line);
	cli_assert(line);
	cli_assert(*line != '\0');
	cli_assert(strnlen(line, CLI_LINE_MAX) < CLI_LINE_MAX);
	cli_assert(!strchr(line, '\n'));

	char * ln = line;

	do {
		char *            str;
		struct cli_expr * expr;
		int               ret;

		ln += strspn(ln, " \t;");
		if (*ln == '\0')
			break;

		if (block->cnt == CLI_EXPR_BLK_MAX) {
			cli_log("cannot parse: too many expressions.");
			return -ENOBUFS;
		}

		str = strsep(&ln, ";");
		cli_assert(str);
		cli_assert(*str != '\0');

		ret = cli_expr_createn_parse(&expr, str);
		if (ret)
			return ret;

		cli_expr_blk_push(block, expr);
	} while (ln);

	if (!block->cnt)
		return -ENODATA;

	block->line = line;

	return 0;
}

#if 0
/* Keep this just in case we need to save parsed line into history. */
ssize_t
cli_expr_blk_make_string(const struct cli_expr_blk * block,
                         char *                      string,
                         size_t                      size)
{
	cli_expr_blk_assert(block);
	cli_assert(block->cnt);
	cli_assert(string);
	cli_assert(size);
	cli_assert(size <= SSIZE_MAX);

	const struct cli_expr * expr = block->head;
	ssize_t                 len = 0;

	len = cli_expr_make_string(expr, string, size);
	cli_assert(len);
	if (len < 0)
		return len;

	for (expr = expr->next; expr; expr = expr->next) {
		cli_assert((size_t)len < size);

		ssize_t ret;

		if (((size_t)len + 3) >= size)
			/*
			 * No more room to store at least a semicolon followed
			 * by a space and a single character.
			 */
			return -ENOBUFS;

		string[len++] = ';';
		string[len++] = ' ';

		ret = cli_expr_make_string(expr, &string[len], size - len);
		cli_assert(ret);
		if (ret < 0)
			return ret;

		len += ret;
	}

	cli_assert(string[len] == '\0');

	return len;
}
#endif

void
cli_expr_blk_init(struct cli_expr_blk * block)
{
	cli_assert(block);

	block->cnt = 0;
	block->head = NULL;
	block->tail = &block->head;
	block->line = NULL;
}

void
cli_expr_blk_fini(struct cli_expr_blk * block)
{
	cli_expr_blk_assert(block);

	struct cli_expr * expr;
	struct cli_expr * tmp;

	cli_expr_blk_foreach_safe(block, expr, tmp)
		cli_expr_destroy(expr);

	cli_free(block->line);
}
