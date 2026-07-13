#include "dir.h"
#include "cmd.h"
#include "yang.h"
#include <errno.h>

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

ssize_t
cli_dir_mkrel(const struct cli_dir * directory,
              const struct cli_dir * ancestor,
              char *                 path,
              size_t                 size)
{
	cli_dir_assert(directory);
	cli_dir_assert(ancestor);
	cli_assert(directory != ancestor);
	cli_assert(path);
	cli_assert(size);
	cli_assert(size <= CLI_PATH_MAX);

	struct cli_path pth;
	ssize_t         len;

	cli_path_init(&pth);

	do {
		cli_dir_assert(directory);

		cli_path_push_head(&pth,
		                   directory->name,
		                   strnlen(directory->name, CLI_PATH_NAME_MAX));
		directory = directory->parent;
	} while (directory != ancestor);

	len = cli_path_mkrel(&pth, path, size);

	cli_path_fini(&pth);

	return len;
}

ssize_t
cli_dir_mkabs(const struct cli_dir * directory, char * path, size_t size)
{
	cli_dir_assert(directory);
	cli_assert(path);
	cli_assert(size);
	cli_assert(size <= CLI_PATH_MAX);

	struct cli_path pth;
	ssize_t         len;

	cli_path_init(&pth);

	while (directory->parent) {
		cli_dir_assert(directory);

		cli_path_push_head(&pth,
		                   directory->name,
		                   strnlen(directory->name, CLI_PATH_NAME_MAX));
		directory = directory->parent;
	}

	len = cli_path_mkabs(&pth, path, size);

	cli_path_fini(&pth);

	return len;
}

char *
cli_dir_xpath(const struct cli_dir * directory)
{
	return (directory->lysc) ? cli_lysc_xpath(directory->lysc)
	                         : cli_strdup("/");
}

int
cli_dir_search_from_path(const struct cli_dir ** directory,
                         const struct cli_path * path)
{
	cli_assert(directory);
	cli_dir_assert(*directory);
	cli_assert(path);

	const struct cli_dir *       dir = *directory;
	unsigned int                 c;
	unsigned int                 cnt;
	const struct cli_path_comp * comp;

	cli_path_foreach_comp(path, c, cnt, comp) {
		if (cli_path_comp_kind(comp) != CLI_PATH_UPPER_COMP_KIND) {
			cli_assert(cli_path_comp_kind(comp) ==
			           CLI_PATH_REG_COMP_KIND);
			break;
		}

		if (dir->parent)
			dir = dir->parent;
	}

	cli_path_foreach_comp_from(path, c, cnt, comp) {
		/* Iterate over each path component... */
		const struct cli_dir * child;
		bool                   found = false;

		/*
		 * ... and search a child directory which name matches the
		 * current component.
		 */
		cli_dir_foreach_child(dir, child) {
			if (!cli_path_comp_ncmp(comp,
			                        child->name,
			                        strlen(child->name))) {
				dir = child;
				found = true;
				break;
			}
		}

		if (!found) {
			/*
			 * No matching child directory found: stop the search
			 * since the given path does not exist.
			 */
			return -ENOENT;
		}
	}

	*directory = dir;

	return 0;
}

int
cli_dir_search(const struct cli_dir ** directory, const char * path)
{
	cli_assert(directory);
	cli_dir_assert(*directory);
	cli_assert(path);

	struct cli_path        pth;
	const struct cli_dir * dir = *directory;
	int                    ret;

	cli_path_init(&pth);

	ret = cli_path_parse(&pth, path);
	if (ret)
		goto fini;

	if (path[0] == '/') {
		while (dir->parent)
			dir = dir->parent;
	}

	ret = cli_dir_search_from_path(&dir, &pth);
	if (ret)
		goto fini;

	*directory = dir;

fini:
	cli_path_fini(&pth);

	return ret;
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
	int                    ret = 0;

	cli_cmd_foreach(directory->hcmd, cmd) {
		cli_cmd_assert(cmd);

		ret = cli_cmd_parse(cmd, directory, argc, argv, data);
		if (ret)
			break;
	}

	if (ret == argc)
		return 0;

	if (!ret) {
		cli_log("'%s': no such command.", argv[0]);
		return -EINVAL;
	}

	return ret;
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

void
_cli_dir_init(struct cli_dir * directory, const char * name, size_t length)
{
	cli_assert(directory);
	cli_assert(name);
	cli_assert(length);
	cli_assert(length < CLI_PATH_NAME_MAX);

	memcpy(directory->name, name, length + 1);
	directory->next = NULL;
	directory->prev = directory;
	directory->child = NULL;
	directory->parent = NULL;
	directory->hcmd = NULL;
	directory->tcmd = NULL;
	directory->lysc = NULL;
}

int
cli_dir_init(struct cli_dir * directory, const char * name)
{
	cli_assert(directory);
	cli_assert(name);

	size_t len;
	int    ret;

	len = strnlen(name, CLI_PATH_NAME_MAX);
	ret = cli_path_comp_isok(name, len);
	if (ret)
		return ret;

	_cli_dir_init(directory, name, len);

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

/******************************************************************************
 * Directory search logic for commands usage.
 ******************************************************************************/

int
cli_dir_exec_search(const struct cli_dir_search * search,
                    const struct cli_dir **       directory,
                    const struct cli_context *    context)
{
	cli_assert(search);
	cli_assert(directory);
	cli_assert(context);

	const struct cli_dir * dir = cli_cwd(context);

	if (cli_path_comp_count(&search->path)) {
		int ret;

		cli_assert(search->orig);
		if (search->orig[0] == '/')
			dir = &context->root;

		ret = cli_dir_search_from_path(&dir, &search->path);
		if (ret)
			return ret;
	}

	*directory = dir;

	return 0;
}

int
cli_dir_parse_search(struct cli_dir_search * search, const char * path)
{
	cli_assert(search);

	ssize_t ret = 0;

	if (path && (*path != '\0')) {
		ret = cli_path_parse(&search->path, path);
		if (ret)
			return ret;
	}

	search->orig = path;

	return 0;
}
