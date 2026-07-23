#include "common.h"
#include "arg.h"
#include <stdarg.h>

#if defined(CONFIG_CLI_ASSERT)

#include "arg.h"
#include <stdbool.h>

void
cli_assert_args(int argc, const char * const argv[])
{
	cli_assert(argc > 0);
	cli_assert(argv);

	int    a;
	size_t len;

	for (a = 0, len = 0; a < argc; a++) {
		cli_assert(argv[a]);

		size_t       alen = strnlen(argv[a], CLI_ARG_MAX);

		cli_assert(alen < CLI_ARG_MAX);

		len += alen;
		cli_assert(len < CLI_LINE_MAX);

		cli_assert(!_cli_arg_isstr_valid(argv[a], alen));
	}
}

#endif /* !defined(CONFIG_CLI_ASSERT) */

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

void *
cli_realloc(void * data, size_t size)
{
	cli_assert(size);

	data = realloc(data, size);
	if (!data)
		abort();

	return data;
}

char *
cli_strdup(const char * string)
{
	cli_assert(string);

	char * str;

	str = strdup(string);
	if (!str)
		abort();

	return str;
}

int
cli_asprintf(char ** string, const char * format, ...)
{
	va_list args;
	char *  str;
	int     ret;

	va_start(args, format);
	ret = vasprintf(&str, format, args);
	va_end(args);

	if (ret >= 0) {
		*string = str;
		return ret;
	}

	if (errno == ENOMEM)
		abort();

	return -errno;
}

const char *
cli_render_string(const char * input, char output[CLI_RENDER_MAX])
{
	cli_assert(output);
	cli_assert(input);

	const char * in = input;
	char *       out = output;

	while ((*in != '\0') && (out < &output[CLI_RENDER_MAX - 1])) {
		if (isprint(*in))
			*out = *in;
		else
			*out = '?';

		in++;
		out++;
	}

	if (*in == '\0')
		*out = '\0';
	else
		memcpy(out - 3, "...", sizeof("..."));

	return output;
}
