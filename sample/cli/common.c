#include "common.h"
#include <sys/ioctl.h>

void *
cli_malloc(size_t size)
{
	cli_assert(size);

	void * data;

	data = malloc(size);
	if (!data)
		abort();

	return data;
}

unsigned int
cli_term_cols(const struct cli_context * context)
{
	cli_assert_context(context);

	struct winsize wsz;

#warning TODO: plug in a SIGWINCH signal handler instead
	if (context->isatty && !ioctl(STDOUT_FILENO, TIOCGWINSZ, &wsz))
		return (unsigned int)wsz.ws_col;
	else
		return 0;
}
