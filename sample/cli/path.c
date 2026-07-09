#include "path.h"
#include <ctype.h>
#include <string.h>

#define CLI_PATH_INIT_STACK_NR (CLI_PATH_MAX / CLI_PATH_NAME_MAX)
#if CLI_PATH_INIT_STACK_NR < 4
#undef CLI_PATH_INIT_STACK_NR
#define CLI_PATH_INIT_STACK_NR (4)
#endif /* CLI_PATH_INIT_STACK_NR < 4 */

#define cli_path_assert_comp(_comp) \
	cli_assert(_comp); \
	cli_assert((_comp)->str); \
	cli_assert((_comp)->len); \
	cli_assert(cli_path_iscomp_valid((_comp)->str) == \
	           (ssize_t)((_comp)->len))

#define cli_path_assert_stack(_stk) \
	cli_assert(_stk); \
	cli_assert((_stk)->nr); \
	cli_assert((_stk)->cnt <= (_stk)->nr); \
	cli_assert((_stk)->comps)

static int
_cli_path_iscomp_valid(const char * component, size_t length)
{
	cli_assert(component);

	if (!length)
		return -ENODATA;
	if (length >= CLI_PATH_NAME_MAX)
		return -ENAMETOOLONG;

	if (isalpha(*component) || (*component == '_')) {
		const char * ptr = component;

		while (++ptr < &component[length]) {
			if (!isalnum(*ptr) && (*ptr != '_') && (*ptr != '-'))
				return -EINVAL;
		}

		return 0;
	}
	else if (*component == '.') {
		if ((length == 1) ||
		    ((length == 2) && (component[1] == '.')))
			return 0;
	}

	return -EINVAL;
}

static ssize_t
cli_path_iscomp_valid(const char * component)
{
	cli_assert(component);

	size_t len;
	int    err;

	len = strnlen(component, CLI_PATH_NAME_MAX);
	err = _cli_path_iscomp_valid(component, len);
	if (err)
		return (ssize_t)err;

	return (ssize_t)len;
}

static size_t
cli_path_stack_length(const struct cli_path_stack * stack)
{
	cli_path_assert_stack(stack);

	unsigned int c;
	size_t       len;

	for (c = 0, len = 0; c < stack->cnt; c++) {
		cli_path_assert_comp(&stack->comps[c]);

		len += stack->comps[c].len;
	}

	/*
	 * Return sum of all component lengths + space required for the number
	 * of path delimiter characters.
	 */
	return len ? (len + stack->cnt - 1) : 0;
}

void
cli_path_push_comp(struct cli_path_stack * stack,
                   const char *            component,
                   size_t                  length)
{
	cli_path_assert_stack(stack);
	cli_assert(component);
	cli_assert(length);
	cli_assert(cli_path_iscomp_valid(component) == (ssize_t)length);

	struct cli_path_comp * comp;

	if (stack->cnt == stack->nr) {
		stack->nr *= 2;
		stack->comps = cli_realloc(stack->comps,
		                           stack->nr * sizeof(stack->comps[0]));
	}

	comp = &stack->comps[stack->cnt++];
	comp->str = component;
	comp->len = length;
}

static const struct cli_path_comp *
cli_path_pop_comp(struct cli_path_stack * stack)
{
	cli_path_assert_stack(stack);

	if (!stack->cnt)
		return NULL;

	stack->cnt--;
	cli_path_assert_comp(&stack->comps[stack->cnt]);

	return &stack->comps[stack->cnt];
}

static char *
cli_path_join_comp(const struct cli_path_comp * comp, char * path)
{
	cli_path_assert_comp(comp);
	cli_assert(path);

	memcpy(path, comp->str, comp->len);
	
	return path + comp->len;
}

static char *
cli_path_join_from_stack(struct cli_path_stack * stack,
                         char *                  path)
{
	cli_path_assert_stack(stack);
	cli_assert(path);

	unsigned int c = stack->cnt;
	char *       ptr = path;

	while (c--) {
		*(ptr++) = '/';
		ptr = cli_path_join_comp(&stack->comps[c], ptr);
	}

	*ptr = '\0';

	stack->cnt = 0;

	return ptr;
}

ssize_t
cli_path_mkrel_from_stack(struct cli_path_stack * stack,
                          char *                  path,
                          size_t                  size)
{
	cli_path_assert_stack(stack);
	cli_assert(path);
	cli_assert(size);
	cli_assert(size <= CLI_PATH_MAX);

	size_t len;

	len = cli_path_stack_length(stack);
	if (len >=  size)
		return -ENOBUFS;

	if (stack->cnt) {
		path = cli_path_join_comp(cli_path_pop_comp(stack), path);
		path = cli_path_join_from_stack(stack, path);
	}

	*path = '\0';

	return len;
}

ssize_t
cli_path_mkabs_from_stack(struct cli_path_stack * stack,
                          char *                  path,
                          size_t                  size)
{
	cli_path_assert_stack(stack);
	cli_assert(path);
	cli_assert(size);
	cli_assert(size <= CLI_PATH_MAX);

	size_t len;

	len = 1 + cli_path_stack_length(stack);
	if (len >= size)
		return -ENOBUFS;

	path = cli_path_join_from_stack(stack, path);

	*path = '\0';

	return len;
}

void
cli_path_init_stack(struct cli_path_stack * stack)
{
	cli_assert(stack);

	stack->nr = CLI_PATH_INIT_STACK_NR;
	stack->cnt = 0;
	stack->comps = cli_malloc(CLI_PATH_INIT_STACK_NR *
	                          sizeof(stack->comps[0]));
}

void
cli_path_fini_stack(struct cli_path_stack * stack)
{
	cli_path_assert_stack(stack);

	cli_free(stack->comps);
}

#warning Implement internal component validation !
ssize_t
cli_path_isok(const char * path);
{
	cli_assert(path);

	size_t len;

	len = strnlen(path, CLI_PATH_MAX);

	return (len < CLI_PATH_MAX) ? len : -ENAMETOOLONG;
}
