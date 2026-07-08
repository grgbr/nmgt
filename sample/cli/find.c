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

static int
cli_find_show_dir(struct cli_dir *    dir,
                  enum cli_walk_event event,
                  void *              data)
{
	cli_dir_assert(dir);

	int * depth = data;

	switch (event) {
	case CLI_WALK_PRE_EVT:
#warning TODO: print whole path !
		printf("%*.*s%s\n", *depth * 4, *depth * 4, "", dir->name);
		*depth = *depth + 1;
		return CLI_WALK_CONT_RET;

	case CLI_WALK_POST_EVT:
		*depth = *depth - 1;
		return CLI_WALK_CONT_RET;

	default:
		cli_assert(0);
	}
}

static int
cli_find_exec_work(const struct cli_work * work,
                   struct cli_context *    context)
{
	const struct cli_find_work * wk = (const struct cli_find_work *)work;
	struct cli_dir *             dir;
	int                          depth = 0;

	cli_assert(!wk->path || wk->len);

	/* Get a pointer to the current working menu directory. */
	dir = cli_cwd(context);

	if (wk->path) {
		int ret;

		/* Search for a menu directory matching the given path. */
		ret = cli_dir_search((const struct cli_dir **)&dir,
		                     wk->path,
		                     wk->len);
		if (ret) {
			cli_log("find: '%s': invalid path: %s.",
			        wk->path,
			        strerror(-ret));
			return ret;
		}
	}

	/*
	 * Given the directory descriptor found above, display its children
	 * directory entries.
	 */
	cli_dir_walk(dir, cli_find_show_dir, &depth);

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
			ret = cli_dir_ispath_valid(argv[1]);
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
