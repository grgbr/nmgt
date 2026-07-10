#include "xpath.h"
#include "yang.h"
#include "cmd.h"

/******************************************************************************
 * `xpath' command handling.
 * Print XPATH for current working directory.
 ******************************************************************************/

struct cli_xpath_work {
	/* Base work unit structure. */
	struct cli_work super;
	/* Optional (may be NULL) pointer to argv[1] path argument. */
	const char *    path;
	/* Length of path (if any) excluding terminating NULL byte. */
	size_t          len;
};

static int
cli_xpath_exec_work(const struct cli_work * work,
                    struct cli_context *    context)
{
	const struct cli_xpath_work * wk = (const struct cli_xpath_work *)work;
	const struct cli_dir *        cwd;
	char *                        xpath;

	cli_assert(!wk->path || wk->len);

	/* Get a pointer to the current working menu directory. */
	cwd = cli_cwd(context);

	if (wk->path) {
		int ret;

		/* Search for a menu directory matching the given path. */
		ret = cli_dir_search(&cwd, wk->path, wk->len);
		if (ret) {
			cli_log("xpath: '%s': invalid path: %s.",
			        wk->path,
			        strerror(-ret));
			return ret;
		}
	}

	/*
	 * Given the directory descriptor found above, compute the corresponding
	 * XPATH and display it.
	 */
	xpath = cli_dir_xpath(cwd);
	cli_assert(xpath);
	printf("%s\n", xpath);
	cli_free(xpath);

	return 0;
}

static const struct cli_work_ops cli_xpath_work_ops = {
	.exec    = cli_xpath_exec_work,
	.release = cli_null_release_work
};

static int
cli_xpath_parse_cmd(const struct cli_cmd * command __cli_unused,
                    const struct cli_dir * dir __cli_unused,
                    int                    argc,
                    const char * const     argv[],
                    void *                 data)
{
	cli_assert(argc >= 1);

	if (!strcmp(argv[0], "xpath")) {
		ssize_t                 ret;
		struct cli_xpath_work * wk;
		struct cli_context *    ctx = data;

		if (argc == 2) {
			ret = cli_path_isok(argv[1]);
			if (ret < 0) {
				cli_log("xpath: invalid path: %s.",
				        strerror(-ret));
				return ret;
			}
		}
		else if (argc != 1) {
			cli_log("xpath: too many argument(s).");
			return -EINVAL;
		}

		wk = (struct cli_xpath_work *)
		     cli_create_work(sizeof(*wk), &cli_xpath_work_ops);
		wk->path = ret ? argv[1] : NULL;
		wk->len = (size_t)ret;
		ret = cli_sched_work(ctx, &wk->super);
		if (ret) {
			cli_destroy_work(&wk->super, ctx);
			cli_log("xpath: cannot schedule work.");
			return ret;
		}

		return argc;
	}
	else
		return 0;
}

static const struct cli_cmd_ops cli_xpath_cmd_ops = {
	.parse = cli_xpath_parse_cmd,
	.fini  = cli_cmd_null_fini
};

void
cli_xpath_build_cmd(struct cli_dir * directory)
{
	cli_dir_assert(directory);

	cli_dir_add_cmd(directory, cli_cmd_create(&cli_xpath_cmd_ops));
}
