#include "xpath.h"
#include "yang.h"
#include "cmd.h"

/******************************************************************************
 * `xpath' command handling.
 * Print XPATH for current working directory.
 ******************************************************************************/

struct cli_xpath_work {
	struct cli_work       super;
	struct cli_dir_search search;
};

static int
cli_xpath_exec_work(struct cli_work * work, struct cli_context * context)
{
	const struct cli_xpath_work * wk = (const struct cli_xpath_work *)work;
	const struct cli_dir *        dir;
	int                           ret;
	char *                        xpath;

	/* Search for the requested directory. */
	ret = cli_dir_exec_search(&wk->search, &dir, context);
	if (ret) {
		/*
		 * Searching for the current directory cannot fail. Hence,
		 * `wk->search.orig' should always exist here.
		 */
		cli_assert(wk->search.orig);

		cli_log("xpath: '%s': %s.",
		        wk->search.orig,
		        cli_dir_strerror(-ret));

		return ret;
	}

	/*
	 * Given the directory descriptor found above, compute the corresponding
	 * XPATH and display it.
	 */
	xpath = cli_dir_xpath(dir);
	cli_assert(xpath);
	printf("%s\n", xpath);
	cli_free(xpath);

	return 0;
}

static void
cli_xpath_release_work(struct cli_work *    work,
                       struct cli_context * context __cli_unused)
{
	cli_dir_fini_search(&((struct cli_xpath_work *)work)->search);
}

static const struct cli_work_ops cli_xpath_work_ops = {
	.exec    = cli_xpath_exec_work,
	.release = cli_xpath_release_work
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
		if (argc <= 2) {
			ssize_t                ret;
			struct cli_xpath_work * wk;

			wk = (struct cli_xpath_work *)
			     cli_create_work(sizeof(*wk), &cli_xpath_work_ops);
			cli_assert(wk);
			cli_dir_init_search(&wk->search);

			ret = cli_dir_parse_search(&wk->search,
			                           (argc == 1) ? NULL
			                                       : argv[1]);
			if (ret) {
				cli_log("xpath: invalid path: %s.",
				        cli_dir_strerror(-ret));
				goto destroy;
			}

			ret = cli_sched_work(data, &wk->super);
			if (ret) {
				cli_log("xpath: cannot schedule work.");
				goto destroy;
			}

			return argc;

destroy:
			cli_destroy_work(&wk->super, data);

			return ret;
		}
		else {
			cli_log("xpath: too many argument(s).");

			return -EINVAL;
		}
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
