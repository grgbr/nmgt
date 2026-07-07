#include "xpath.h"
#include "yang.h"

/******************************************************************************
 * `xpath' command handling.
 * Print XPATH for current working command line node.
 ******************************************************************************/

static int
cli_xpath_exec_work(const struct cli_work * work, struct cli_context * context)
{
	char * xpath;

	xpath = cli_lysc_xpath(cli_current_menu(context)->lysc);
	cli_assert(xpath);
	cli_log("%s\n", xpath);
	cli_free(xpath);

	return 0;
}

static const struct cli_work_ops cli_xpath_work_ops = {
	.exec    = cli_xpath_exec_work,
	.release = cli_releasen_destroy_work
};

static int
cli_xpath_sched_work(struct cli_context * context)
{
	struct cli_work * wk;
	int               ret;

	wk = cli_create_work(sizeof(*wk), &cli_xpath_work_ops);
	ret = cli_sched_work(context, wk);
	if (!ret)
		return 0;

	cli_destroy_work(wk);

	return ret;
}

static int
cli_xpath_parse_cmd(const struct cli_node * node,
                    int                     argc,
                    const char * const      argv[],
                    struct cli_context *    context)
{
	if (strcmp(argv[0], "xpath"))
		return 0;

	if (argc == 1) {
		struct cli_work * wk;
		int               ret;

		wk = cli_create_work(sizeof(*wk), &cli_xpath_work_ops);
		ret = cli_sched_work(context, wk);
		if (!ret)
			return 1;

		cli_destroy_work(wk);
		cli_log("xpath: cannot schedule work.");

		return ret;
	}
	else
		cli_log("xpath: too many argument(s).");

	return -EINVAL;
}

static const struct cli_node_ops cli_xpath_cmd_ops = {
	.parse   = cli_xpath_parse_cmd,
	.release = cli_release_node_null,
};

void
cli_xpath_build_cmd(struct cli_node * parent)
{
	cli_assert_node(parent);

	struct cli_node * cmd;

	cmd = cli_create_node(sizeof(*cmd), &cli_xpath_cmd_ops);
	cli_node_add_child(parent, cmd);
}
