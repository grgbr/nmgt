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

static const struct cli_cmd_ops cli_find_cmd_ops = {
	.parse = cli_find_parse_cmd,
	.fini  = cli_cmd_null_fini
};

void
cli_find_build_cmd(struct cli_dir * directory)
{
	cli_dir_assert(directory);

	cli_dir_add_cmd(directory, cli_cmd_create(&cli_find_cmd_ops));
}
