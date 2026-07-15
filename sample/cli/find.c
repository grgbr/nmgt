#include "find.h"
#include "cli.h"
#include "cmd.h"

/******************************************************************************
 * `find' command handling.
 * List menu directory entries recursively.
 ******************************************************************************/

struct cli_find_work {
	/* Base work unit structure. */
	struct cli_work       super;
	/* Internal state of requested path search. */
	struct cli_dir_search search;
	/* Length of `norm' field, excluding the terminating NULL byte. */
	size_t                len;
	/* Validated normalized requested path. */
	char                  norm[CLI_PATH_MAX];
};

struct cli_find_show {
	const struct cli_dir * anc;
	char *                 path;
	size_t                 off;
};

static int
cli_find_show_dir(struct cli_dir *    directory,
                  enum cli_walk_event event,
                  void *              data)
{
	cli_dir_assert(directory);
	cli_assert(data);

	switch (event) {
	case CLI_WALK_PRE_EVT:
		{
			const struct cli_find_show * show = data;
			ssize_t                      ret;

			ret = cli_dir_mkrel(directory,
			                    show->anc,
			                    &show->path[show->off],
			                    CLI_PATH_MAX - show->off);

			cli_assert(ret);
			if (ret > 0)
				printf("%s\n", show->path);
			else
				cli_log("find: cannot show: %s.",
				        cli_dir_strerror(-ret));

			break;
		}

	case CLI_WALK_POST_EVT:
		break;

	default:
		cli_assert(0);
	}

	return CLI_WALK_CONT_RET;
}

static int
cli_find_exec_work(struct cli_work * work, struct cli_context * context)
{
	struct cli_find_work * wk = (struct cli_find_work *)work;
	const struct cli_dir * dir;
	int                    ret;
	struct cli_find_show   show;

	ret = cli_dir_exec_search(&wk->search, &dir, context);
	if (ret) {
		/*
		 * Searching for the current directory cannot fail. Hence,
		 * `wk->search.orig' should always exist here.
		 */
		cli_assert(wk->search.orig);

		cli_log("find: '%s': %s.",
		        wk->search.orig,
		        cli_dir_strerror(-ret));
		return ret;
	}

	/*
	 * Given the directory descriptor found above, display its descendant
	 * directory entries.
	 */
	show.anc = dir;
	show.path = wk->norm;
	show.off = wk->len;

	return cli_dir_walk((struct cli_dir *)dir, cli_find_show_dir, &show);
}

static void
cli_find_release_work(struct cli_work *    work,
                      struct cli_context * context __cli_unused)
{
	cli_dir_fini_search(&((struct cli_find_work *)work)->search);
}

static const struct cli_work_ops cli_find_work_ops = {
	.exec    = cli_find_exec_work,
	.release = cli_find_release_work
};

static int
cli_find_parse_search(struct cli_find_work * work, const char * path)
{
	cli_assert(work);

	ssize_t ret = 0;

	if (path && (*path != '\0')) {
		ret = cli_path_parse(&work->search.path, path);
		if (ret)
			return (int)ret;

		work->search.orig = path;

		if (*path != '/')
			ret = cli_path_mkrel(&work->search.path,
			                     work->norm,
			                     sizeof(work->norm));
		else
			ret = cli_path_mkabs(&work->search.path,
			                     work->norm,
			                     sizeof(work->norm));

		cli_assert(ret >= 0);
		if (ret && (work->norm[ret - 1] != '/')) {
			if ((size_t)(ret + 1) >= sizeof(work->norm))
				return -ENAMETOOLONG;

			work->norm[ret++] = '/';
		}
	}

	work->len = (size_t)ret;
	work->norm[ret] = '\0';

	return 0;
}

static int
cli_find_parse_cmd(const struct cli_cmd * command __cli_unused,
                   const struct cli_dir * dir __cli_unused,
                   int                    argc,
                   const char * const     argv[],
                   void *                 data)
{
	cli_assert(argc >= 1);

	if (!strcmp(argv[0], "find")) {
		if (argc <= 2) {
			struct cli_find_work * wk;
			int                    ret;

			wk = (struct cli_find_work *)
			     cli_create_work(sizeof(*wk), &cli_find_work_ops);
			cli_assert(wk);
			cli_dir_init_search(&wk->search);

			ret = cli_find_parse_search(wk, (argc == 1) ? NULL
			                                            : argv[1]);
			if (ret) {
				cli_log("find: invalid path: %s.",
				        cli_path_strerror(-ret));
				goto destroy;
			}

			ret = cli_sched_work(data, &wk->super);
			if (ret) {
				cli_log("find: cannot schedule work.");
				goto destroy;
			}

			return argc;

destroy:
			cli_destroy_work(&wk->super, data);

			return ret;
		}
		else {
			cli_log("find: too many argument(s).");

			return -EINVAL;
		}
	}
	else
		return 0;
}

static int
cli_find_parse_cmd(const struct cli_cmd * command,
                   const struct cli_dir * directory,
                   struct cli_context *   context,
                   int                    argc,
                   const char * const     argv[])
{
	int ret;

	ret = cli_cmd_parse_args(command,
	                         directory,
	                         context,
	                         argc,
	                         argv,
	                         work);

}

static const struct cli_cmd_ops cli_find_cmd_ops = {
	.parse = cli_find_parse_cmd,
};

void
cli_find_build_cmd(struct cli_dir * directory)
{
	cli_dir_assert(directory);

	struct cli_cmd * cmd;
	struct cli_arg * arg;

	/*
	 * No need to check for returned code since cli_cmd_create() cannot fail
	 * with the "find" name argument.
	 */
	cli_assert(sizeof("find") <= CLI_ARG_MAX);
	cli_cmd_create(&cmd, "find", &cli_find_cmd_ops);
	cli_assert(cmd);

	/* Cannot fail either. */
	arg = cli_arg_create(&cli_path_arg_ops);
	cli_assert(arg);
	cli_cmd_add_arg(cmd, arg);

	cli_dir_add_cmd(directory, cmd);
}

/******************************************************************************/

TODO: del cli_dir_search logic and replace by this one !!

struct cli_dir_work {
	/* Base work unit structure. */
	struct cli_work        super;
	/* Internal state of requested path search. */
	struct cli_path        path;
	/* Pointer to original path argument. */
	const char *           orig;
	/* Length of `norm' field, excluding the terminating NULL byte. */
	size_t                 len;
	/* Validated normalized requested path. */
	char                   norm[CLI_PATH_MAX];
	/* The command that initiated this work. */
	const struct cli_cmd * cmd;
};

int
cli_dir_work_search(struct cli_dir_work *   work,
                    struct cli_context *    context,
                    const struct cli_dir ** directory)
{
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
			 * Hence, `wk->orig' should always be a non-empty
			 * string.
			 */
			cli_assert(wk->orig && (wk->orig[0] != '\0'));
			cli_cmd_log(wk->cmd,
			            "'%s': %s.",
			            wk->orig,
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
	cli_assert(!work->orig);
	cli_assert(!work->len);
	cli_assert(work->norm[0] == '\0');
	cli_assert(work->cmd);
	cli_assert(argc >= 1);
	cli_assert(argv[0]);

	const char * path = argv[0];
	ssize_t      ret = 0;

	if (*path != '\0') {
		ret = cli_path_parse(&work->path, path);
		if (ret)
			goto out;

		if (*path == '/') {
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
		else {
			ret = cli_path_mkrel(&work->path,
			                     work->norm,
			                     sizeof(work->norm));
			cli_assert(ret >= 0);
		}

		work->len = (size_t)ret;
		work->norm[ret] = '\0';
	}

	work->orig = path;

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

	cli_path_init(&wk->path);
	wk->orig = NULL;
	wk->len = 0;
	wk->norm[0] = '\0';
	wk->cmd = command;

	return wk;
}

static void
cli_dir_work_fini(struct cli_dir_work * work)
{
	cli_assert(work);

	cli_path_fini(&work->path);
}

void
cli_dir_work_destroy(struct cli_dir_work * work)
{
	cli_dir_work_fini(work);
	cli_destroy_work(&work->super);
}

void
cli_dir_work_release(struct cli_work *    work,
                     struct cli_context * context __cli_unused)
{
	cli_dir_work_fini((struct cli_dir_work *)work);
}
