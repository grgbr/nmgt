#include "match.h"
#include "arg.h"
#include <string.h>

#define CLI_MATCH_INIT_NR (4U)

void
cli_match_push(struct cli_match * matches, char * string)
{
	cli_match_assert(matches);
	cli_assert(!string || (string[0] != '\0'));
	cli_assert(!string || (strnlen(string, CLI_MATCH_MAX) < CLI_MATCH_MAX));

	if (matches->cnt >= matches->nr) {
		unsigned int nr = matches->nr ? matches->nr * 2
		                              : CLI_MATCH_INIT_NR;

		if (nr >= CLI_MATCH_MAX)
			/*
			 * Since we are in the middle of a completion process,
			 * silently ignore unexpected loads of matches...
			 */
			return;

		matches->data = cli_realloc(matches->data,
		                            nr * sizeof(matches->data[0]));
		matches->nr = nr;
		cli_assert(matches->data);
	}

	matches->data[matches->cnt++] = string;
}

void
cli_match_init(struct cli_match * matches)
{
	cli_assert(matches);

	matches->cnt = 0;
	matches->nr = 0;
	matches->data = NULL;
}
