#include "dir.h"
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
	cli_dir_assert(directory);

	switch (directory->type) {
	case CLI_DIR_NODE_TYPE:
		return cli_lysc_node_xpath(directory->sch_node);
	case CLI_DIR_MOD_TYPE:
		return cli_lys_module_xpath(directory->sch_mod);
	case CLI_DIR_NONE_TYPE:
		return cli_strdup("/");
	default:
		cli_assert(0);
	}
}

int
cli_dir_show_yang(const struct cli_dir *     directory,
                  const struct cli_context * context)
{
	cli_dir_assert(directory);
	cli_assert_context(context);

	int ret;

	switch (directory->type) {
	case CLI_DIR_NODE_TYPE:
		ret = cli_lysc_print_node_yang(context, directory->sch_node, 0);
		if (ret != LY_SUCCESS)
			ret = -EBADR;
		break;

	case CLI_DIR_MOD_TYPE:
		ret = cli_lys_print_module_yang(context, directory->sch_mod, 0);
		if (ret != LY_SUCCESS)
			ret = -EBADR;
		break;

	case CLI_DIR_NONE_TYPE:
		ret = -EBADR;
		break;

	default:
		cli_assert(0);
	}

	return ret;
}

int
cli_dir_show_diag(const struct cli_dir *     directory,
                  const struct cli_context * context)
{
	cli_dir_assert(directory);
	cli_assert_context(context);

	int ret;

	switch (directory->type) {
	case CLI_DIR_NODE_TYPE:
		ret = cli_lysc_print_node_diag(context, directory->sch_node);
		if (ret != LY_SUCCESS)
			ret = -EBADR;
		break;

	case CLI_DIR_MOD_TYPE:
		ret = cli_lys_print_module_diag(context, directory->sch_mod);
		if (ret != LY_SUCCESS)
			ret = -EBADR;
		break;

	case CLI_DIR_NONE_TYPE:
		ret = -EBADR;
		break;

	default:
		cli_assert(0);
	}

	return ret;
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
                  struct cli_context *   context,
                  int                    argc,
                  const char * const     argv[])
{
	cli_dir_assert(directory);
	cli_assert_args(argc, argv);

	const struct cli_node * cmd;
	int                     ret = 0;

	cli_node_foreach_sibling((struct cli_node *)directory->cmds, cmd) {
		ret = cli_cmd_parse((const struct cli_cmd *)cmd,
		                    directory,
		                    context,
		                    argc,
		                    argv);
		if (ret)
			break;
	}

	if (ret == argc)
		return 0;

	if (!ret) {
		cli_log("'%s': no such command.", argv[0]);
		return -EINVAL;
	}

	cli_assert(ret < 0);

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

static void
_cli_dir_init(struct cli_dir *  directory,
              const char *      name,
              size_t            length,
              enum cli_dir_type type,
              const void *      schema)
{
	cli_assert(directory);
	cli_assert(name);
	cli_assert(length);
	cli_assert(length < CLI_PATH_NAME_MAX);
	cli_assert((type == CLI_DIR_NONE_TYPE) ||
	           (type == CLI_DIR_MOD_TYPE) ||
	           (type == CLI_DIR_NODE_TYPE));

	memcpy(directory->name, name, length + 1);
	directory->next = NULL;
	directory->prev = directory;
	directory->child = NULL;
	directory->parent = NULL;
	directory->cmds = NULL;
	directory->type = type;
	directory->sch_void = schema;
}

static int
cli_dir_init(struct cli_dir *  directory,
             const char *      name,
             enum cli_dir_type type,
             const void *      schema)
{
	cli_assert(directory);
	cli_assert(name);
	cli_assert((type == CLI_DIR_NONE_TYPE) ||
	           (type == CLI_DIR_MOD_TYPE) ||
	           (type == CLI_DIR_NODE_TYPE));
	cli_assert((type == CLI_DIR_NONE_TYPE) || schema);

	size_t len;
	int    ret;

	len = strnlen(name, CLI_PATH_NAME_MAX);
	ret = cli_path_comp_isok(name, len);
	if (ret)
		return ret;

	_cli_dir_init(directory, name, len, type, schema);

	return 0;
}

static void
cli_dir_fini(struct cli_dir * directory)
{
	cli_dir_assert(directory);

	struct cli_node * cmd;
	struct cli_node * tmp;

	cli_node_foreach_sibling_safe((struct cli_node *)directory->cmds,
	                              cmd,
	                              tmp)
		cli_cmd_destroy((struct cli_cmd *)cmd);
}

void
cli_dir_init_root(struct cli_dir * root)
{
	cli_assert(root);

	_cli_dir_init(root, "/", sizeof("/") - 1, CLI_DIR_NONE_TYPE, NULL);
}

void
cli_dir_fini_root(struct cli_dir * root)
{
	cli_dir_assert(root);

	cli_dir_fini(root);
}

struct cli_dir *
cli_dir_create(const char * name, enum cli_dir_type type, const void * schema)
{
	cli_assert(name);

	struct cli_dir * dir;

	dir = cli_malloc(sizeof(*dir));
	if (!cli_dir_init(dir, name, type, schema))
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
cli_dir_work_search(const struct cli_dir_work * work,
                    struct cli_context *        context,
                    const struct cli_dir **     directory)
{
	cli_assert(work);
	cli_assert(work->cmd);
	cli_assert(strnlen(work->norm, CLI_PATH_MAX) == work->len);

	const struct cli_dir * dir = cli_cwd(context);

	if (cli_path_comp_count(&work->path)) {
		int ret;

		cli_assert(work->orig);
		if (work->orig[0] == '/')
			dir = &context->root;

		ret = cli_dir_search_from_path(&dir, &work->path);
		if (ret) {
			/*
			 * Searching for the current directory cannot fail.
			 * Hence, `work->orig' should always be a non-empty
			 * string.
			 */
			cli_assert(work->orig && (work->orig[0] != '\0'));
			cli_cmd_log(work->cmd,
			            "'%s': %s.",
			            work->orig,
			            cli_dir_strerror(-ret));

			return ret;
		}
	}

	*directory = dir;

	return 0;
}

int
cli_dir_work_parse(struct cli_dir_work * work,
                   int                   argc,
                   const char * const    argv[],
                   bool                  mandatory)
{
	cli_assert(work);
	cli_assert(work->cmd);
	cli_assert(argc >= 1);
	cli_assert(argv[0]);

	const char * path = argv[0];
	ssize_t      ret = 0;

	if (*path != '\0') {
		ret = cli_path_parse(&work->path, path);
		if (ret)
			goto out;

		if (*path != '/')
			ret = cli_path_mkrel(&work->path,
			                     work->norm,
			                     sizeof(work->norm));
		else
			ret = cli_path_mkabs(&work->path,
			                     work->norm,
			                     sizeof(work->norm));

		cli_assert(ret >= 0);
		if (ret && (work->norm[ret - 1] != '/')) {
			if ((size_t)(ret + 1) >= sizeof(work->norm)) {
				ret = -ENAMETOOLONG;
				goto out;
			}

			work->norm[ret++] = '/';
		}
	}

	work->orig = path;
	work->len = (size_t)ret;
	work->norm[ret] = '\0';

	return 1;

out:
	if (!mandatory)
		return 0;

	cli_cmd_log(work->cmd,
	            "'%s': invalid path argument: %s.",
	            path,
	            cli_path_strerror(-ret));

	return (int)ret;
}

struct cli_dir_work *
cli_dir_work_create(size_t                      size,
                    const struct cli_cmd *      command,
                    const struct cli_work_ops * opers)
{
	cli_assert(size >= sizeof(struct cli_dir_work));
	cli_cmd_assert(command);
	cli_assert_work_ops(opers);

	struct cli_dir_work * wk;

	wk = (struct cli_dir_work *)cli_create_work(size, opers);
	cli_assert(wk);

	wk->cmd = command;
	cli_path_init(&wk->path);
	wk->orig = NULL;
	wk->len = 0;
	wk->norm[0] = '\0';

	return wk;
}

static void
cli_dir_work_fini(struct cli_dir_work * work)
{
	cli_assert(work);

	cli_path_fini(&work->path);
}

void
cli_dir_work_release(struct cli_work * work)
{
	cli_dir_work_fini((struct cli_dir_work *)work);
}

struct cli_dir_work_arg {
	struct cli_arg super;
	bool           mand;
};

static int
cli_dir_work_parse_arg(const struct cli_arg *     argument,
                       const struct cli_cmd *     command,
                       const struct cli_dir *     directory __cli_unused,
                       const struct cli_context * context __cli_unused,
                       int                        argc,
                       const char * const         argv[],
                       void *                     data)
{
	cli_arg_assert(argument);
	cli_cmd_assert(command);
	cli_dir_assert(directory);
	cli_assert_context(context);
	cli_assert(argc);
	cli_assert(argv[0]);
	cli_assert(data);

	struct cli_dir_work * wk = (struct cli_dir_work *)data;

	/* Ignore multiple path arguments. */
	if (!wk->orig)
		return cli_dir_work_parse(
			wk,
			argc,
			argv,
			((const struct cli_dir_work_arg *)argument)->mand);
	else
		return 0;
}

static const struct cli_arg_ops cli_dir_work_arg_ops = {
	.parse = cli_dir_work_parse_arg
};

struct cli_dir_work_arg *
cli_dir_work_create_arg(bool mandatory)
{
	struct cli_dir_work_arg * arg;

	arg = (struct cli_dir_work_arg *)
	      cli_arg_create(sizeof(*arg), &cli_dir_work_arg_ops);
	cli_assert(arg);

	arg->mand = mandatory;

	return arg;
}

struct cli_dir_work_arg *
cli_dir_work_createn_add_arg(bool mandatory, struct cli_node * cmd_or_arg)
{
	struct cli_dir_work_arg * arg;

	arg = cli_dir_work_create_arg(mandatory);
	cli_assert(arg);

	cli_node_add_child(cmd_or_arg, (struct cli_node *)arg);

	return arg;
}
