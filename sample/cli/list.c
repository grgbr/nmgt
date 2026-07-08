#include "list.h"
#include "cli.h"
#include "cmd.h"

/******************************************************************************
 * `ls' command handling.
 * List menu directory entries.
 ******************************************************************************/

struct cli_list_work {
	/* Base work unit structure. */
	struct cli_work super;
	/* Optional (may be NULL) pointer to argv[1] path argument. */
	const char *    path;
	/* Length of path (if any) excluding terminating NULL byte. */
	size_t          len;
};

static int
cli_list_exec_work(const struct cli_work * work,
                   struct cli_context *    context)
{
	const struct cli_list_work * wk = (const struct cli_list_work *)work;
	const struct cli_dir *        cwd;
	const struct cli_dir *        child;

	cli_assert(!wk->path || wk->len);

	/* Get a pointer to the current working menu directory. */
	cwd = cli_cwd(context);

	if (wk->path) {
		int ret;

		/* Search for a menu directory matching the given path. */
		ret = cli_dir_search(&cwd, wk->path, wk->len);
		if (ret) {
			cli_log("ls: '%s': invalid path: %s.",
			        wk->path,
			        strerror(-ret));
			return ret;
		}
	}

	/*
	 * Given the directory descriptor found above, display its children
	 * directory entries.
	 */
	cli_dir_foreach_child(cwd, child)
		printf("%s\n", child->name);

	return 0;
}

static const struct cli_work_ops cli_list_work_ops = {
	.exec    = cli_list_exec_work,
	.release = cli_null_release_work
};

static int
cli_list_parse_cmd(const struct cli_cmd * command __cli_unused,
                   const struct cli_dir * dir __cli_unused,
                   int                    argc,
                   const char * const     argv[],
                   void *                 data)
{
	cli_assert(argc >= 1);

	if (!strcmp(argv[0], "ls")) {
		ssize_t                ret;
		struct cli_list_work * wk;
		struct cli_context *   ctx = data;

		if (argc == 2) {
			ret = cli_dir_ispath_valid(argv[1]);
			if (ret < 0) {
				cli_log("ls: invalid specified: %s.",
				        strerror(-ret));
				return ret;
			}
		}
		else if (argc != 1) {
			cli_log("ls: too many argument(s).");
			return -EINVAL;
		}

		wk = (struct cli_list_work *)
		     cli_create_work(sizeof(*wk), &cli_list_work_ops);
		wk->path = ret ? argv[1] : NULL;
		wk->len = (size_t)ret;
		ret = cli_sched_work(ctx, &wk->super);
		if (ret) {
			cli_destroy_work(&wk->super, ctx);
			cli_log("ls: cannot schedule work.");
			return ret;
		}

		return argc;
	}
	else
		return 0;
}

static const struct cli_cmd_ops cli_list_cmd_ops = {
	.parse = cli_list_parse_cmd,
	.fini  = cli_cmd_null_fini
};

void
cli_list_build_cmd(struct cli_dir * directory)
{
	cli_dir_assert(directory);

	cli_dir_add_cmd(directory, cli_cmd_create(&cli_list_cmd_ops));
}
