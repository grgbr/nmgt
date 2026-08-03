#include "dir.h"
#include "yang.h"
#include "match.h"
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
	cli_assert(len);

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
		if (cli_path_comp_kind(comp) !=
		    CLI_PATH_UPPER_COMP_KIND) {
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
		 * ...and search a child directory which name matches
		 * the current component.
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
			 * No matching child directory found: stop the
			 * search since the given path does not exist.
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

struct cli_cmd *
cli_dir_find_cmd(const struct cli_dir * directory, const char * name)
{
	cli_dir_assert(directory);
	cli_assert(name);
	cli_assert(name[0]);
	cli_assert(strnlen(name, CLI_ARG_MAX) < CLI_ARG_MAX);

	struct cli_node * cmd;

	cli_node_foreach_sibling((struct cli_node *)directory->cmds, cmd) {
		if (!strcmp(name, ((const struct cli_cmd *)cmd)->name))
			return (struct cli_cmd *)cmd;
	}

	return NULL;
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

	return ret;
}

void
cli_dir_complete_cmd(const struct cli_dir * directory,
                     struct cli_context *   context,
                     const char *           word,
                     size_t                 length,
                     int                    argc,
                     const char * const     argv[],
                     struct cli_match *     matches)
{
	cli_dir_assert(directory);
	cli_assert_context(context);
	cli_assert(word);
	cli_assert(length < CLI_ARG_MAX);
	cli_assert(strnlen(word, CLI_ARG_MAX) == length);
	cli_assert(argc >= 0);
	cli_assert(!argc || argv);
	cli_match_assert(matches);

	const struct cli_node * cmd;

	cli_node_foreach_sibling((struct cli_node *)directory->cmds, cmd)
		cli_cmd_complete((const struct cli_cmd *)cmd,
		                 directory,
		                 context,
		                 word,
		                 length,
		                 argc,
		                 argv,
		                 matches);
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

int
cli_dir_create_cmdn_add(struct cli_dir *           directory,
                        struct cli_cmd **          command,
                        const char *               name,
                        const struct cli_cmd_ops * opers)
{
	cli_dir_assert(directory);
	cli_assert(command);
	cli_assert(name);
	cli_assert(name[0] != '\0');
	cli_assert(!cli_dir_find_cmd(directory, name));
	cli_cmd_assert_ops(opers);

	int ret;

	ret = cli_cmd_create(command, name, opers);
	if (ret)
		return ret;

	cli_dir_add_cmd(directory, *command);

	return 0;
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
	else if (work->norm[0] == '/') {
		/* Search for the root directory... */
		dir = &context->root;
	}
	/* Else: search for the current working directory. */

	*directory = dir;

	return 0;
}

int
cli_dir_work_parse(struct cli_dir_work * work,
                   const char *          path,
                   bool                  mandatory)
{
	cli_assert(work);
	cli_assert(work->cmd);
	cli_assert(path);

	if (!work->orig) {
		ssize_t ret = 0;

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
	else {
		/* Tell the caller that parsing has already been compeleted. */
		return -EALREADY;
	}
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
cli_dir_work_parse_arg(const struct cli_arg * argument,
                       const struct cli_cmd * command,
                       const struct cli_dir * directory __cli_unused,
                       struct cli_context *   context __cli_unused,
                       int                    argc,
                       const char * const     argv[],
                       void *                 data)
{
	cli_arg_assert(argument);
	cli_cmd_assert(command);
	cli_dir_assert(directory);
	cli_assert_context(context);
	cli_assert(argc);
	cli_assert(argv[0]);
	cli_assert(data);

	int ret;

	ret = cli_dir_work_parse(
		(struct cli_dir_work *)data,
		argv[0],
		((const struct cli_dir_work_arg *)argument)->mand);

	/* Ignore multiple path argument. */
	return (ret != -EALREADY) ? ret : 0;
}

static void
cli_dir_push_match(struct cli_match * matches, const char * dir_name)
{
	cli_match_assert(matches);
	cli_assert(!cli_path_isok(dir_name));
	cli_assert(dir_name[0] != '\0');
	cli_assert(dir_name[strlen(dir_name) - 1] == '/');

	char * path;

	path = cli_strdup(dir_name);
	cli_assert(path);

	cli_match_push(matches, path);
}

static void
cli_dir_join_push_match(struct cli_match * matches,
                        const char *       dir_name,
                        const char *       base_name)
{
	char * path;
	int    ret;

	ret = cli_asprintf(&path, "%s%s/", dir_name, base_name);
	cli_assert(ret >= 2);
	cli_assert(path);

	cli_match_push(matches, path);
}

static void
cli_dir_generate_matches(const struct cli_dir * directory,
                         const char *           normalized,
                         struct cli_match *     matches)
{
	cli_dir_assert(directory);
	cli_assert(!normalized || !cli_path_isok(normalized));
	cli_assert(!normalized || (normalized[0] != '\0'));
	cli_assert(!normalized || (normalized[strlen(normalized) - 1] == '/'));
	cli_match_assert(matches);

	const struct cli_dir * child;

	if (cli_dir_has_child(directory)) {
		/*
		 * Here, we give ownership of newly allocated directory name to
		 * `matches'.
		 * Allocated string ownership will be transfered from `matches'
		 * to readline(3) by cli_shell_complete().
		 * Readline(3) will free(3) it at completion process termination
		 * time.
		 */
		cli_dir_foreach_child(directory, child)
			cli_dir_join_push_match(matches,
			                        normalized ? normalized : "",
			                        child->name);

		cli_shell_suppress_complete_char();
	}
	else if (normalized)
		cli_dir_push_match(matches, normalized);
}

static void
cli_dir_generate_named_matches(const struct cli_dir * directory,
                               const char *           normalized,
                               const char *           base_name,
                               size_t                 base_length,
                               struct cli_match *     matches)
{
	cli_dir_assert(directory);
	cli_assert(!normalized || !cli_path_isok(normalized));
	cli_assert(!normalized || (normalized[0] != '\0'));
	cli_assert(!normalized || (normalized[strlen(normalized) - 1] == '/'));
	cli_assert(base_name);
	cli_assert(base_length);
	cli_assert(base_length < CLI_PATH_NAME_MAX);
	cli_assert(strnlen(base_name, CLI_PATH_NAME_MAX) == base_length);
	cli_match_assert(matches);

	enum cli_path_comp_kind kind;

	if (cli_path_parse_comp(&kind, base_name, base_length + 1) > 0) {
		if (kind == CLI_PATH_REG_COMP_KIND) {
			const struct cli_dir * child;

			/*
			 * Here, we give ownership of newly allocated directory
			 * name to `matches'.
			 * Allocated string ownership will be transfered from
			 * `matches' to readline(3) by cli_shell_complete().
			 * Readline(3) will free(3) it at completion process
			 * termination time.
			 */
			if (cli_dir_has_child(directory)) {
				cli_dir_foreach_child(directory, child) {
					if (!strncmp(base_name,
					             child->name,
					             base_length))
						cli_dir_join_push_match(
							matches,
							normalized ? normalized
							           : "",
							           child->name);
				}
				cli_shell_suppress_complete_char();
			}
			else if (normalized)
				cli_dir_push_match(matches, normalized);
		}
		else {
			cli_dir_join_push_match(matches,
			                        normalized ? normalized : "",
			                        "..");
			cli_shell_suppress_complete_char();
		}
	}
}

static ssize_t
cli_dir_searchn_normalize(const struct cli_dir **     directory,
                          const char *                path,
                          char **                     normalized,
                          const struct  cli_context * context)
{
	cli_assert(directory);
	cli_dir_assert(*directory);
	cli_assert(path);
	cli_assert(normalized);
	cli_assert_context(context);

	struct cli_path        pth;
	int                    ret;
	char *                 norm;
	ssize_t                len;
	const struct cli_dir * dir = *directory;

	cli_path_init(&pth);

	ret = cli_path_parse(&pth, path);
	if (ret)
		goto fini;

	norm = cli_malloc(CLI_PATH_MAX);
	cli_assert(norm);

	if (path[0] != '/')
		len = cli_path_mkrel(&pth, norm, CLI_PATH_MAX - 1);
	else
		len = cli_path_mkabs(&pth, norm, CLI_PATH_MAX - 1);

	cli_assert(len >= 0);
	cli_assert((len + 1) < CLI_PATH_MAX);
	if (len) {
		if (norm[len - 1] != '/') {
			norm[len++] = '/';
			norm[len] = '\0';
		}

		if (norm[0] == '/')
			dir = &context->root;

		ret = cli_dir_search_from_path(&dir, &pth);
		if (ret)
			/* Directory not found: no match. */
			goto free;
	}
	else {
		cli_free(norm);
		norm = NULL;
	}

	cli_path_fini(&pth);

	*directory = dir;
	*normalized = norm;

	return len;

free:
	cli_free(norm);
fini:
	cli_path_fini(&pth);

	return ret;
}

static void
cli_dir_work_complete_arg(const struct cli_arg * argument __cli_unused,
                          const struct cli_cmd * command __cli_unused,
                          const struct cli_dir * directory __cli_unused,
                          struct cli_context *   context,
                          const char *           word,
                          size_t                 length,
                          int                    argc __cli_unused,
                          const char * const     argv[] __cli_unused,
                          struct cli_match *     matches)
{
	cli_arg_assert(argument);
	cli_cmd_assert(command);
	cli_dir_assert(directory);
	cli_assert_context(context);
	cli_match_assert(matches);

	const struct cli_dir * pdir = cli_cwd(context);

	if (length) {
		/* Base name of requested path. */
		const char * bname;
		/* Base name length. */
		size_t       blen;
		/* Directory name length of requested path. */
		ssize_t      dlen;
		/* Normalized directory name of requested path. */
		char *       norm = NULL;

		if (length >= CLI_PATH_MAX)
			return;

		/* Compute required base name. */
		bname = &word[length];
		while ((bname > word) && (bname[-1] != '/'))
		       bname--;
		blen = &word[length] - bname;
		cli_assert(bname[blen] == '\0');

		dlen = (ssize_t)(length - blen);
		if (dlen) {
			/*
			 * Compute requested directory name and search for its
			 * corresponding directory descriptor.
			 */
			char * dname;

			dname = cli_malloc(dlen + 1);
			cli_assert(dname);
			memcpy(dname, word, dlen);
			dname[dlen] = '\0';

			dlen = cli_dir_searchn_normalize(&pdir,
			                                 dname,
			                                 &norm,
			                                 context);

			cli_free(dname);

			if (dlen < 0)
				return;

			cli_assert((!dlen && !norm) || (dlen && norm));
		}

		if (blen)
			cli_dir_generate_named_matches(pdir,
			                               norm,
			                               bname,
			                               blen,
			                               matches);
		else
			cli_dir_generate_matches(pdir, norm, matches);

		/* free(3) handles NULL arguments properly... */
		cli_free(norm);
	}
	else
		cli_dir_generate_matches(pdir, NULL, matches);

	rl_filename_completion_desired = 1;
}

static const struct cli_arg_ops cli_dir_work_arg_ops = {
	.parse    = cli_dir_work_parse_arg,
	.complete = cli_dir_work_complete_arg
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
