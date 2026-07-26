#ifndef _CLI_MATCH_H
#define _CLI_MATCH_H

#include "common.h"

struct cli_match {
	unsigned int cnt;
	unsigned int nr;
	char **      data;
};

#define CLI_MATCH_MAX (1024U)

#define cli_match_assert(_match) \
	cli_assert(_match); \
	cli_assert((_match)->nr <= CLI_MATCH_MAX); \
	cli_assert((_match)->cnt <= (_match)->nr)

#define CLI_MATCH_INIT(_match) \
	{ \
		match->cnt = 0, \
		match->nr = 0, \
		match->data = NULL \
	}

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
