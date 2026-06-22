#define _GNU_SOURCE

#include <sysrepo.h>

int
main(int argc, const char * const argv[])
{
	sr_conn_ctx_t *           conn;
	struct ly_out *           out;
	const struct ly_ctx *     ctx;
	const struct lys_module * mod;
	uint32_t                  m;
	int                       ret;

	/* Connect to sysrepo. */
	ret = sr_connect(0, &conn);
	if (ret != SR_ERR_OK)
		return EXIT_FAILURE;

	if (ly_out_new_file(stdout, &out) != LY_SUCCESS)
		goto disconnect;

	/* Acquire libyang context. */
	ctx = sr_acquire_context(conn);

	/* Iterate over modules registered to libyang context. */
	for (m = 0, mod = ly_ctx_get_module_iter(ctx, &m);
	     mod;
	     mod = ly_ctx_get_module_iter(ctx, &m)) {
		/*
		 * Skip sysrepo / libyang internal modules.
		 * Use compiled and (features) implemented, i.e. completely
		 * resolved modules only.
		 */
		if (!sr_is_module_internal(mod) && mod->implemented) {
			ret = lys_print_module(out,
			                       mod,
			                       LYS_OUT_YANG_COMPILED,
			                       0,
			                       0);
			if (ret != LY_SUCCESS)
				break;
		}
	}

free_out:
	ly_out_free(out, NULL, 0);
release_ctx:
	sr_release_context(conn);
disconnect:
	sr_disconnect(conn);

	return (ret == SR_ERR_OK) ? EXIT_SUCCESS : EXIT_FAILURE;
}
