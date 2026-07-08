#include "dir.h"
#include "cmd.h"
#include "yang.h"
#include <ctype.h>
#include <errno.h>

ssize_t
cli_dir_ispath_valid(const char * path)
{
	cli_assert(path);

	size_t len;

	len = strnlen(path, CLI_DIR_PATH_MAX);
	if (len >= CLI_DIR_PATH_MAX)
		return -ENAMETOOLONG;

	return len;
}

static int
cli_dir_walk_recurs(struct cli_dir *   directory,
                    cli_dir_visit_fn * visit,
                    void *             data)
{
#warning Remove recursion
	cli_dir_assert(directory);
	cli_assert(visit);

	int ret;

	ret = visit(directory, CLI_WALK_PRE_EVT, data);
	if (ret == CLI_WALK_CONT_RET) {
		struct cli_dir * child;

		cli_dir_foreach_child(directory, child) {
			ret = cli_dir_walk_recurs(child, visit, data);
			if (ret < 0)
				return ret;
		}

		ret = visit(directory, CLI_WALK_POST_EVT, data);
		cli_assert(ret <= 0);
	}

	return (ret >= 0) ? 0 : ret;
}

int
cli_dir_walk(struct cli_dir *   directory,
             cli_dir_visit_fn * visit,
             void *             data)
{
	cli_dir_assert(directory);
	cli_assert(visit);

	struct cli_dir * child;
	int              ret = 0;

	cli_dir_foreach_child(directory, child) {
		ret = cli_dir_walk_recurs(child, visit, data);
		if (ret < 0)
			return ret;
	}

	cli_assert(!ret);
	return 0;
}

static int
cli_dir_walk_recurs_safe(struct cli_dir *   directory,
                         cli_dir_visit_fn * visit,
                         void *             data)
{
#warning Remove recursion
	cli_dir_assert(directory);
	cli_assert(visit);

	int ret;

	ret = visit(directory, CLI_WALK_PRE_EVT, data);
	if (ret == CLI_WALK_CONT_RET) {
		struct cli_dir * child;
		struct cli_dir * tmp;

		cli_dir_foreach_child_safe(directory, child, tmp) {
			ret = cli_dir_walk_recurs_safe(child, visit, data);
			if (ret < 0)
				return ret;
		}

		ret = visit(directory, CLI_WALK_POST_EVT, data);
		cli_assert(ret <= 0);
	}

	return (ret >= 0) ? 0 : ret;
}

int
cli_dir_walk_safe(struct cli_dir *   directory,
                  cli_dir_visit_fn * visit,
                  void *             data)
{
	cli_dir_assert(directory);
	cli_assert(visit);

	struct cli_dir * child;
	struct cli_dir * tmp;
	int              ret = 0;

	cli_dir_foreach_child_safe(directory, child, tmp) {
		ret = cli_dir_walk_recurs_safe(child, visit, data);
		if (ret < 0)
			return ret;
	}

	cli_assert(!ret);
	return 0;
}

char *
cli_dir_xpath(const struct cli_dir * directory)
{
	return (directory->lysc) ? cli_lysc_xpath(directory->lysc)
	                         : cli_strdup("/");
}

/*
 * To keep compliant with YANG identifiers, path component name :
 * - starts with a [a-zA-Z_] character ;
 * - is followed by zero or more [a-zA-Z0-9_-] characters ;
 * - and its entire length may be composed of up to (CLI_DIR_NAME_MAX - 1)
 *   characters.
 * See section 6.2 of RFC 7950 for more informations.
 */
static ssize_t
cli_dir_next_path_comp(char ** path)
{
	cli_assert(path);
	cli_assert(*path);

	char * ptr = *path;
	char * start;
	size_t len;

	/* Skip leading '/' duplicates. */
	while (ptr[0] == '/')
		ptr++;

	if (ptr[0] == '\0')
		return 0;

	if (isalpha(ptr[0]) || (ptr[0] != '_')) {
		/*
		 * Found a valid first component character: parse the component :
		 * - save a pointer to the begining of the component and
		 * - initialize its length computation.
		 */
		start = ptr++;
		len = 1;

		/* Probe component last character. */
		while ((len < CLI_DIR_NAME_MAX) &&
		       (isalnum(ptr[0]) ||
		        (ptr[0] == '_') ||
		        (ptr[0] == '-'))) {
			ptr++;
			len++;
		}

		if (len == CLI_DIR_NAME_MAX)
			/* Component too long... */
			return -ENAMETOOLONG;

		if (ptr[0] == '/')
			goto delim;
		else if (ptr[0] == '\0')
			goto valid;
	}
	else if (ptr[0] == '.') {
		start = ptr++;
		len = 1;

		if (ptr[0] == '.') {
			ptr++;
			len = 2;
			if (ptr[0] == '/')
				goto delim;
			else if (ptr[0] == '\0')
				goto valid;
		}
		else if (ptr[0] == '/')
			goto delim;
		else if (ptr[0] == '\0')
			goto valid;
	}

	return -EINVAL;

delim:
	/* Component delimiter found: overwrite with a terminating NULL byte */
	ptr[0] = '\0';
	ptr++;
	len++;
valid:
	/*
	 * Valid component found:
	 * - make path point to the component start and
	 * - return its length.
	 */
	*path = start;
	return (ssize_t)len;
}

int
cli_dir_search(const struct cli_dir ** directory,
               const char *            path,
               size_t                  length)
{
	cli_assert(directory);
	cli_dir_assert(*directory);
	cli_assert(path);
	cli_assert(length);
	cli_assert(length < CLI_DIR_PATH_MAX);
	cli_assert(cli_dir_ispath_valid(path) == (ssize_t)length);

	const struct cli_dir * dir = *directory;
	char *                 tmp;
	char *                 comp;
	ssize_t                cnt;

	/* Find directory root if absolute path search is requested. */
	if (path[0] == '/') {
		while (dir->parent)
			dir = dir->parent;
	}

	/* Allocate a temporary duplicate of `path' string. */
	tmp = cli_malloc(length + 1);
	memcpy(tmp, path, length + 1);

	/*
	 * Now iterate over components and locate matching directories along the
	 * path.
	 */
	comp = tmp;
	cnt = cli_dir_next_path_comp(&comp);
	while (cnt > 0) {
		cli_assert(comp >= tmp);
		cli_assert(&comp[cnt] <= &tmp[length]);

		const struct cli_dir * child;

		if (comp[0] != '.') {
			bool found = false;

			/*
			 * Search a child directory which name matches the
			 * current component.
			 */
			cli_dir_foreach_child(dir, child) {
				if (!strcmp(comp, child->name)) {
					dir = child;
					found = true;
					break;
				}
			}

			if (!found) {
				/*
				 * No matching child directory found: stop the
				 * search since the given path does not exist.
				 */
				cnt = -ENOENT;
				goto free;
			}
		}
		else if ((cnt >= 2) && (comp[1] == '.')) {
			if (dir->parent)
				dir = dir->parent;
		}

		/*
		 * Jump to character right after valid component end and parse
		 * next one.
		 */
		comp = &comp[cnt];
		cnt = cli_dir_next_path_comp(&comp);
	}

	if (!cnt)
		/* End of path parsing and directory found. */
		*directory = dir;

free:
	cli_free(tmp);

	return cnt;
}

int
cli_dir_parse_cmd(const struct cli_dir * directory,
                  int                    argc,
                  const char * const     argv[],
                  void *                 data)
{
	cli_dir_assert(directory);
	cli_assert_args(argc, argv);

	const struct cli_cmd * cmd;
	int                    ret = -EINVAL;

	cli_cmd_foreach(directory->hcmd, cmd) {
		cli_cmd_assert(cmd);

		ret = cli_cmd_parse(cmd, directory, argc, argv, data);
		if (ret)
			break;
	}

	if (ret == argc)
		return 0;

	return (ret < 0) ? ret : -EINVAL;
}

void
cli_dir_add_child(struct cli_dir * directory, struct cli_dir * child)
{
	cli_dir_assert(directory);
	cli_dir_assert(child);

	struct cli_dir * head = directory->child;

	if (head) {
		struct cli_dir * tail = head->prev;

		child->prev = tail;
		tail->next = child;
		head->prev = child;
	}
	else {
		child->prev = child;
		directory->child = child;
	}

	child->parent = directory;
}

void
cli_dir_add_cmd(struct cli_dir * directory, struct cli_cmd * command)
{
	cli_dir_assert(directory);
	cli_cmd_assert(command);
	cli_assert(!command->next);

	if (directory->hcmd) {
		directory->tcmd->next = command;
		directory->tcmd = command;
	}
	else {
		directory->hcmd = command;
		directory->tcmd = command;
	}
}

int
cli_dir_init(struct cli_dir * directory, const char * name)
{
	cli_assert(directory);
	cli_assert(name);

	size_t len;

	len = strnlen(name, CLI_DIR_NAME_MAX);
	if (len >= CLI_DIR_NAME_MAX)
		return -ENAMETOOLONG;

	memcpy(directory->name, name, len + 1);
	directory->next = NULL;
	directory->prev = directory;
	directory->child = NULL;
	directory->parent = NULL;
	directory->hcmd = NULL;
	directory->tcmd = NULL;
	directory->lysc = NULL;

	return 0;
}

void
cli_dir_fini(struct cli_dir * directory)
{
	cli_dir_assert(directory);

	struct cli_cmd * cmd;
	struct cli_cmd * tmp;

	cli_cmd_foreach_safe(directory->hcmd, cmd, tmp)
		cli_cmd_destroy(cmd);
}

struct cli_dir *
cli_dir_create(const char * name)
{
	cli_assert(name);

	struct cli_dir * dir;

	dir = cli_malloc(sizeof(*dir));
	if (!cli_dir_init(dir, name))
		return dir;

	cli_free(dir);

	return NULL;
}

void
cli_dir_destroy(struct cli_dir * directory)
{
	cli_dir_fini(directory);
	cli_free(directory);
}
