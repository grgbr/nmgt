#include "expr.h"

/******************************************************************************
 * Base command line expression handling.
 ******************************************************************************/

#define CLI_EXPR_ARGS_INIT (4U)
#define CLI_EXPR_ARGS_MAX  (CLI_LINE_MAX / 2U)

static int
cli_expr_push_arg(struct cli_expr * expression,
                  const char *      argument,
                  size_t            length)
{
	cli_expr_assert(expression);
	cli_assert(argument);
	cli_assert(length);

	if (length >= CLI_ARG_MAX) {
		char rnd[CLI_RENDER_MAX];

		cli_log("cannot parse: '%s': expression argument too long.",
		        cli_render_string(argument, rnd));
		return -ENAMETOOLONG;
	}

	cli_assert(strnlen(argument, CLI_ARG_MAX) == length);

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

	expression->args[expression->cnt++] = argument;

	return 0;
}

static int
cli_expr_parse_string(struct cli_expr * expr, char * string)
{
	cli_expr_assert(expr);
	cli_assert(string);
	cli_assert(*string != '\0');
	cli_assert(strnlen(string, CLI_LINE_MAX) < CLI_LINE_MAX);
	cli_assert(!strchr(string, '\n'));

	char * str = string;
	int    ret;

	do {
		char * tok;
		size_t len;

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

static struct cli_expr *
cli_expr_create(void)
{
	struct cli_expr * expr;

	expr = cli_malloc(sizeof(*expr));
	cli_assert(expr);

	expr->next = NULL;
	expr->cnt = 0;
	expr->nr = CLI_EXPR_ARGS_INIT;
	expr->args = cli_malloc(CLI_EXPR_ARGS_INIT * sizeof(expr->args[0]));
	cli_assert(expr->args);
}

static void
cli_expr_destroy(struct cli_expr * expression)
{
	cli_expr_assert(expression);

	cli_free(expression->args);
	cli_free(expression);
}

static int
cli_expr_createn_parse(struct cli_expr ** expression, char * string)
{
	cli_assert(expression);
	cli_assert(line);

	struct cli_expr * expr;

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
	cli_assert(cnt < CLI_EXPR_BLK_MAX);

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

static ssize_t
cli_expr_make_string(const struct cli_expr * expr,
                     char *                  string,
                     size_t                  size)
{
	cli_expr_assert(expr);
	cli_assert(string);
	cli_assert(size);

	const char * arg = expr->args[0];
	ssize_t      len = 0;

FINISH ME!!!!!

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
}

ssize_t
cli_expr_blk_make_string(const struct cli_expr_blk * block,
                         char *                      string,
                         size_t                      size)
{
	cli_expr_blk_assert(block);
	cli_assert(block->cnt);
	cli_assert(string);
	cli_assert(size);

	const struct cli_expr * expr = block->head;
	ssize_t                 len = 0;

	len = cli_expr_make_string(expr, string, size);
	cli_assert(len);
	if (len < 0)
		return len;

	for (expr = expr->next; expr; expr = expr->next) {
		cli_assert(len < size);

		ssize_t ret;

		if ((len + 3) >= size)
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

	return len;
}

void
cli_expr_blk_init(struct cli_expr_blk * block);
{
	cli_assert(block);

	block->cnt = 0;
	block->head = NULL;
	block->tail = &blk->head;
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
