#include "path.h"
#include <stdbool.h>
#include <string.h>

#define CLI_PATH_INIT_NR (CLI_PATH_MAX / CLI_PATH_NAME_MAX)
#if CLI_PATH_INIT_NR < 4
#undef CLI_PATH_INIT_NR
#define CLI_PATH_INIT_NR (4)
#endif /* CLI_PATH_INIT_NR < 4 */

const char *
cli_path_strerror(int error)
{
	switch (error) {
	case EINVAL:
		return "unexpected path component character";
	case ENAMETOOLONG:
		return "path or component too long";
	case ENODATA:
		return "path component empty";
	case ENOBUFS:
		return "not enought buffer space";
	default:
		cli_assert(0);
	}
}

static inline bool
cli_path_iscomp_lead_char(const char * character)
{
	if (isalpha(*character) || (*character == '_'))
		return true;
	else
		return false;
}

static inline bool
cli_path_iscomp_char(const char * character)
{
	cli_assert(character);

	if (isalnum(*character) || (*character == '_') || (*character == '-'))
		return true;
	else
		return false;
}

int
cli_path_comp_isok(const char * component, size_t length)
{
	cli_assert(component);

	if (!length)
		return -ENODATA;
	if (length >= CLI_PATH_NAME_MAX)
		return -ENAMETOOLONG;

	if (cli_path_iscomp_lead_char(component)) {
		const char * ptr = component;

		while (++ptr < &component[length]) {
			if (!cli_path_iscomp_char(ptr))
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

int
cli_path_comp_ncmp(const struct cli_path_comp * component,
                   const char *                 string,
                   size_t                       length)
{
	cli_path_assert_comp(component);
	cli_assert(string);
	cli_assert(length < CLI_PATH_NAME_MAX);

	size_t len = component->len;

	if (len != length)
		return len - length;

	return strncmp(component->str, string, len);
}

int
cli_path_comp_cmp(const struct cli_path_comp * component,
                  const char *                 string)
{
	size_t len;

	len = strnlen(string, CLI_PATH_NAME_MAX);
	if (!len)
		return -ENODATA;
	if (len == CLI_PATH_NAME_MAX)
		return -ENAMETOOLONG;

	return cli_path_comp_ncmp(component, string, len);
}

/* Return the kind of a valid (i.e., already parsed) path component. */
enum cli_path_comp_kind
cli_path_comp_kind(const struct cli_path_comp * component)
{
	cli_path_assert_comp(component);

	switch (component->len) {
	case 2:
		if ((component->str[0] == '.') && (component->str[1] == '.'))
			return CLI_PATH_UPPER_COMP_KIND;
		break;

	case 1:
		if (component->str[0] == '.')
			return CLI_PATH_CURR_COMP_KIND;
		break;

	default:
		break;
	}

	return CLI_PATH_REG_COMP_KIND;
}

static size_t
cli_path_length(const struct cli_path * path)
{
	cli_path_assert(path);

	unsigned int                 c;
	unsigned int                 cnt;
	const struct cli_path_comp * comp;
	size_t                       len = 0;

	cli_path_foreach_comp(path, c, cnt, comp) {
		cli_path_assert_comp(comp);

		len += comp->len;
	}

	/*
	 * Return sum of all component lengths + space required for the number
	 * of path delimiter characters.
	 */
	return len ? (len + path->cnt - 1) : 0;
}

/* Append the content of component to string given in argument. */
static char *
cli_path_comp_strcat(const struct cli_path_comp * comp, char * string)
{
	cli_path_assert_comp(comp);
	cli_assert(string);

	memcpy(string, comp->str, comp->len);
	
	return string + comp->len;
}

/* Append the content of the entire path to string given in argument. */
static void
cli_path_strcat(const struct cli_path * path, char * string)
{
	cli_path_assert(path);
	cli_assert(string);

	char * str = string;

	if (path->cnt) {
		unsigned int                 c = path->head;
		unsigned int                 cnt = path->cnt;
		const struct cli_path_comp * comp;

		str = cli_path_comp_strcat(&path->comps[c], str);
		cli_path_foreach_comp_continue(path, c, cnt, comp) {
			*(str++) = '/';
			str = cli_path_comp_strcat(comp, str);
		}
	}

	*str = '\0';
}

ssize_t
cli_path_mkrel(struct cli_path * path, char * string, size_t size)
{
	cli_path_assert(path);
	cli_assert(string);
	cli_assert(size);
	cli_assert(size <= CLI_PATH_MAX);

	size_t len;

	len = cli_path_length(path);
	if (len >=  size)
		return -ENOBUFS;

	cli_path_strcat(path, string);

	return len;
}

ssize_t
cli_path_mkabs(struct cli_path * path, char * string, size_t size)
{
	cli_path_assert(path);
	cli_assert(string);
	cli_assert(size);
	cli_assert(size <= CLI_PATH_MAX);

	size_t len;

	len = 1 + cli_path_length(path);
	if (len >= size)
		return -ENOBUFS;

	string[0] = '/';
	cli_path_strcat(path, &string[1]);

	return len;
}

static const struct cli_path_comp *
cli_path_peek_tail(const struct cli_path * path)
{
	cli_path_assert(path);

	if (path->cnt) {
		struct cli_path_comp * comp;

		comp = &path->comps[(path->head + path->cnt - 1) % path->nr];
		cli_path_assert_comp(comp);

		return comp;
	}
	else
		return NULL;
}

static inline void
cli_path_grow(struct cli_path * path)
{
	cli_path_assert(path);

	if (path->cnt == path->nr) {
		struct cli_path_comp * comps;
		unsigned int           hcnt;  /* Count of head components. */
		unsigned int           tcnt;  /* Count of tail components. */

		hcnt = (path->nr - path->head);
		if (hcnt > path->cnt)
			hcnt = path->cnt;
		tcnt = path->cnt - hcnt;

		path->nr *= 2;
		comps = cli_malloc(path->nr * sizeof(path->comps[0]));

		/*
		 * Copy leading components first starting from the begining of
		 * the new memory area. Then, append the trailing components
		 * right after the leading components.
		 */
		memcpy(comps,
		       &path->comps[path->head],
		       hcnt * sizeof(comps[0]));
		if (tcnt)
			memcpy(&comps[hcnt],
			       &path->comps[0],
			       tcnt * sizeof(comps[0]));

		path->head = 0;

		cli_free(path->comps);
		path->comps = comps;
	}
}

void
cli_path_push_head(struct cli_path * path,
                   const char *      component,
                   size_t            length)
{
	cli_path_assert(path);
	cli_assert(component);
	cli_assert(length);
	cli_assert(!cli_path_comp_isok(component, length));

	struct cli_path_comp * comp;
	unsigned int           head;

	cli_path_grow(path);

	head = (path->head + path->nr - 1) % path->nr;

	comp = &path->comps[head];
	comp->str = component;
	comp->len = length;

	path->head = head;
	path->cnt++;
}

void
cli_path_push_tail(struct cli_path * path,
                   const char *      component,
                   size_t            length)
{
	cli_path_assert(path);
	cli_assert(component);
	cli_assert(length);
	cli_assert(!cli_path_comp_isok(component, length));

	struct cli_path_comp * comp;
	unsigned int           tail;

	cli_path_grow(path);

	tail = (path->head + path->cnt) % path->nr;

	comp = &path->comps[tail];
	comp->str = component;
	comp->len = length;

	path->cnt++;
}

static size_t
cli_path_skip_delim(const char * string, size_t size)
{
	cli_assert(string);
	cli_assert(size);
	cli_assert(size < CLI_PATH_MAX);

	const char * str = string;

	do {
		if (*str != '/')
			break;
	} while (++str < &string[size]);

	return (size_t)(str - string);
}

ssize_t
cli_path_parse_comp(enum cli_path_comp_kind * kind,
                    const char *              string,
                    size_t                    size)
{
	cli_assert(kind);
	cli_assert(string);
	cli_assert(size);

	const char * str = string;
	const char * end = &string[size];

	if (cli_path_iscomp_lead_char(str)) {
		while (++str < end) {
			if (!cli_path_iscomp_char(str))
				break;
		}

		if ((str == end) || (*str == '/') || (*str == '\0')) {
			*kind = CLI_PATH_REG_COMP_KIND;
			return (ssize_t)(str - string);
		}
	}
	else if (*str == '.') {
		if ((size == 1) || (str[1] == '/') || (str[1] == '\0')) {
			*kind = CLI_PATH_CURR_COMP_KIND;
			return 1;
		}
		else if ((str[1] == '.') &&
		    ((size == 2) || (str[2] == '/') || (str[2] == '\0'))) {
			*kind = CLI_PATH_UPPER_COMP_KIND;
			return 2;
		}
	}
	else if (*str == '\0')
		return 0;

	return -EINVAL;
}

static void
cli_push_upper_comp_tail(struct cli_path * path,
                         const char *      string,
                         size_t            length,
                         bool              abspath)
{
	const struct cli_path_comp * last;

	last = cli_path_peek_tail(path);

	if (!abspath) {
		/*
		 * We are parsing a relative path.
		 * If a previously (upper) parsed regular component exists, pop
		 * it out.
		 * Else, push the "upper directory" component along the existing
		 * path.
		 */
		if (last &&
		    (cli_path_comp_kind(last) == CLI_PATH_REG_COMP_KIND))
			path->cnt--;
		else
			cli_path_push_tail(path, string, length);
	}
	else {
		/*
		 * We are parsing an absolute path.
		 * If a previously (upper) parsed regular component exists, pop
		 * it out.
		 * In any other cases, no need to push an "upper directory"
		 * component since we are already at the root level.
		 */
		if (last) {
		    cli_assert(cli_path_comp_kind(last) ==
		               CLI_PATH_REG_COMP_KIND);
			path->cnt--;
		}
	}
}

static void
cli_push_curr_comp_tail(struct cli_path * path,
                        const char *      string,
                        size_t            length,
                        bool              abspath)
{
	if (!abspath && !path->cnt) {
		/*
		 * We are parsing the first component of a relative
		 * path.
		 * Push the "current directory" as first path component.
		 */
		cli_path_push_tail(path, string, length);
	}
}

static int
_cli_path_parse(struct cli_path * path,
                const char *      string,
                size_t            size,
                bool              abspath)
{
	cli_path_assert(path);
	cli_assert(string);
	cli_assert(size);
	cli_assert(size < CLI_PATH_MAX);
	cli_assert(strnlen(string, CLI_PATH_MAX) == (size - 1));

	const char * str = string;

	do {
		ssize_t                 len;
		enum cli_path_comp_kind kind;

		len = (size_t)cli_path_skip_delim(str, size);
		str += len;
		size -= len;
		if (!size)
			break;

		len = cli_path_parse_comp(&kind, str, size);
		if (len <= 0)
			return len;

		switch (kind) {
		case CLI_PATH_REG_COMP_KIND:
			cli_path_push_tail(path, str, len);
			break;

		case CLI_PATH_UPPER_COMP_KIND:
			cli_push_upper_comp_tail(path, str, len, abspath);
			break;

		case CLI_PATH_CURR_COMP_KIND:
			cli_push_curr_comp_tail(path, str, len, abspath);
			break;

		default:
			cli_assert(0);
		}

		str += len;
		size -= len;
	} while (size);

	return 0;
}

int
cli_path_parse(struct cli_path * path, const char * string)
{
	cli_path_assert(path);
	cli_assert(string);

	size_t len;

	len = strnlen(string, CLI_PATH_MAX);
	if (len == CLI_PATH_MAX)
		return -ENAMETOOLONG;

	return _cli_path_parse(path, string, len + 1, *string == '/');
}

void
cli_path_init(struct cli_path * path)
{
	cli_assert(path);

	path->nr = CLI_PATH_INIT_NR;
	path->head = 0;
	path->cnt = 0;
	path->comps = cli_malloc(CLI_PATH_INIT_NR * sizeof(path->comps[0]));
}

void
cli_path_fini(struct cli_path * path)
{
	cli_path_assert(path);

	cli_free(path->comps);
}

struct cli_path *
cli_path_create(void)
{
	struct cli_path * path;

	path = cli_malloc(sizeof(*path));
	cli_assert(path);

	cli_path_init(path);

	return path;
}

void
cli_path_destroy(struct cli_path * path)
{
	cli_path_assert(path);

	cli_path_fini(path);
	cli_free(path);
}

ssize_t
cli_path_normalize(const char * path, char * norm, size_t size)
{
	cli_assert(path);
	cli_assert(norm);
	cli_assert(size);
	cli_assert(size <= CLI_PATH_MAX);

	if (*path != '\0') {
		struct cli_path pth;
		ssize_t         len;

		cli_path_init(&pth);

		len = cli_path_parse(&pth, path);
		if (len)
			goto out;

		if (*path != '/')
			len = cli_path_mkrel(&pth, norm, size);
		else
			len = cli_path_mkabs(&pth, norm, size);

out:
		cli_path_fini(&pth);

		return (int)len;
	}
	else {
		norm[0] = '\0';

		return 0;
	}
}

int
cli_path_isok(const char * path)
{
	cli_assert(path);

	if (*path != '\0') {
		struct cli_path pth;
		int             ret;

		cli_path_init(&pth);
		ret = cli_path_parse(&pth, path);
		cli_path_fini(&pth);

		return ret;
	}
	else
		return 0;
}
