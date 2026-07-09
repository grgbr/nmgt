#include "find.h"
#include "cli.h"
#include "cmd.h"

#warning Refactor with list and xpath commands

/******************************************************************************
 * `find' command handling.
 * List menu directory entries recursively.
 ******************************************************************************/

struct cli_find_work {
	/* Base work unit structure. */
	struct cli_work super;
	/* Optional (may be NULL) pointer to argv[1] path argument. */
	const char *    path;
	/* Length of path (if any) excluding terminating NULL byte. */
	size_t          len;
};

static void
cli_find_show_relpath(const struct cli_dir * directory,
                      const struct cli_dir * ancestor)
{
	char * path;

	path = cli_dir_relpath(directory, ancestor);
	cli_assert(path);

	printf("%s\n", path);

	cli_free(path);
}

static void
cli_find_show_abspath(const struct cli_dir * directory)
{
	char * path;

	path = cli_dir_abspath(directory);
	cli_assert(path);

	printf("%s\n", path);

	cli_free(path);
}

static int
cli_find_show_dir(struct cli_dir *    dir,
                  enum cli_walk_event event,
                  void *              data)
{
	cli_dir_assert(dir);

	switch (event) {
	case CLI_WALK_PRE_EVT:
		if (data)
			cli_find_show_relpath(dir, data);
		else
			cli_find_show_abspath(dir);
		break;

	case CLI_WALK_POST_EVT:
		break;

	default:
		cli_assert(0);
	}

	return CLI_WALK_CONT_RET;
}

struct cli_find_show {
	const struct cli_dir * ancestor;
	char *                 path;
	size_t                 avail;
};

static int
cli_find_exec_work(const struct cli_work * work,
                   struct cli_context *    context)
{
	const struct cli_find_work * wk = (const struct cli_find_work *)work;
	const struct cli_dir *       dir;
	char *                       path;
	struct cli_find_show         show;

	cli_assert(!wk->path || wk->len);

	/*
	 * Allocate a memory region large enough to hold the longest path
	 * possible.
	 */
	path = cli_malloc(CLI_PATH_MAX);
	cli_assert(path);

	/* Get a pointer to the current working menu directory. */
	dir = cli_cwd(context);

	if (wk->path) {
		cli_assert(wk->path[0] != '\0');

		int ret;

		FINISH ME

		if (wk->path[0] != '/') {
			memcpy(path, wk->path, wk->len);
			path[wk->len] = '/';
			show.anc = dir;
			show.path = path;
			show.off = wk->len + 1;
		}
		else {
			show.anc = NULL;
			show.path = path;
			show.off = 0;
		}

		/* Search for a menu directory matching the given path. */
		ret = cli_dir_search(&dir, wk->path, wk->len);
		if (ret) {
			cli_log("find: '%s': invalid path: %s.",
			        wk->path,
			        strerror(-ret));
			return ret;
		}
	}
	else {
		show.anc = dir;
		show.path = path;
		show.left = CLI_PATH_MAX;
	}

	/*
	 * Given the directory descriptor found above, display its children
	 * directory entries.
	 */
	cli_dir_walk((struct cli_dir *)dir, cli_find_show_dir, &show);

	return 0;
}

static const struct cli_work_ops cli_find_work_ops = {
	.exec    = cli_find_exec_work,
	.release = cli_null_release_work
};

static int
cli_find_parse_cmd(const struct cli_cmd * command __cli_unused,
                   const struct cli_dir * dir __cli_unused,
                   int                    argc,
                   const char * const     argv[],
                   void *                 data)
{
	cli_assert(argc >= 1);

	if (!strcmp(argv[0], "find")) {
		ssize_t                ret;
		struct cli_find_work * wk;
		struct cli_context *   ctx = data;

		if (argc == 2) {
			ret = cli_path_isok(argv[1]);
			if (ret < 0) {
				cli_log("find: invalid specified: %s.",
				        strerror(-ret));
				return ret;
			}
		}
		else if (argc != 1) {
			cli_log("find: too many argument(s).");
			return -EINVAL;
		}

		wk = (struct cli_find_work *)
		     cli_create_work(sizeof(*wk), &cli_find_work_ops);
		wk->path = ret ? argv[1] : NULL;
		wk->len = (size_t)ret;
		ret = cli_sched_work(ctx, &wk->super);
		if (ret) {
			cli_destroy_work(&wk->super, ctx);
			cli_log("find: cannot schedule work.");
			return ret;
		}

		return argc;
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
