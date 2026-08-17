#ifndef _CLI_MATCH_H
#define _CLI_MATCH_H

#include "common.h"

struct cli_match {
	unsigned int cnt;
	unsigned int nr;
	char **      data;
};

/*
 * TODO: make sure CLI_MATCH_MAX is consistent with `rl_completion_query_items'
 *       if used.
 */
#define CLI_MATCH_MAX (1024U)

#define cli_match_assert(_match) \
	cli_assert(_match); \
	cli_assert((_match)->nr <= CLI_MATCH_MAX); \
	cli_assert((_match)->cnt <= (_match)->nr)

#define CLI_MATCH_INIT \
	{ \
		.cnt = 0, \
		.nr = 0, \
		.data = NULL \
	}

#define cli_match_foreach(_matches, _indx, _match) \
	for (_indx = 0; \
	     ((_indx) < (_matches)->cnt) && \
	     (_match = (_matches)->data[_indx]); \
	     (_indx)++)

static inline unsigned int
cli_match_count(const struct cli_match * matches)
{
	cli_match_assert(matches);

	return matches->cnt;
}

static inline char *
cli_match_get(const struct cli_match * matches, unsigned int index)
{
	cli_match_assert(matches);

	return (index < matches->cnt) ? matches->data[index] : NULL;
}

extern void
cli_match_push(struct cli_match * matches, char * string);

extern void
cli_match_init(struct cli_match * matches);

static inline void
cli_match_fini(struct cli_match * matches)
{
	cli_match_assert(matches);

	cli_free(matches->data);
}

#endif /* _CLI_MATCH_H */
