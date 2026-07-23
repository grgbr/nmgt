#ifndef _CLI_COMMON_H
#define _CLI_COMMON_H

#define _GNU_SOURCE

#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <limits.h>
#include <ctype.h>
#include <errno.h>

#define CONFIG_CLI_ASSERT 1
#define CONFIG_CLI_LOG 1
#define CONFIG_CLI_LOG_LEVEL 5
#define CONFIG_CLI_DEBUG 1

/*
 * Maximum size available to store a command line including the terminating NULL
 * byte.
 */
#define CONFIG_CLI_LINE_MAX 1024
#if CONFIG_CLI_LINE_MAX > SSIZE_MAX
#error Invalid maximum input line size !
#endif /* CONFIG_CLI_LINE_MAX > SSIZE_MAX */

/*
 * Maximum size available to store a xpath including the terminating NULL byte.
 */
#define CONFIG_CLI_XPATH_MAX 128
#if CONFIG_CLI_XPATH_MAX > SSIZE_MAX
#error Invalid maximum XPATH size !
#endif /* CONFIG_CLI_XPATH_MAX > SSIZE_MAX */

#define CLI_CONCAT(_x, _y) \
	__CONCAT(_x, _y)

#define CLI_LINE_MAX  CLI_CONCAT(CONFIG_CLI_LINE_MAX, U)
#define CLI_XPATH_MAX CLI_CONCAT(CONFIG_CLI_XPATH_MAX, U)

/******************************************************************************
 * Utilities
 ******************************************************************************/

#define __cli_unused __attribute__((__unused__))

#define __cli_printf(_fmt_indx, _arg_indx) \
	__attribute__((format(printf, _fmt_indx, _arg_indx)))

#define cli_array_nr(_array) \
	(sizeof(_array) / sizeof(_array[0]))

#define cli_containerof(_ptr, _type, _member) \
	({ \
		const typeof(((_type *)0)->_member) * __ptr = (_ptr); \
		(_type *)((const char *)__ptr - offsetof(_type, _member)); \
	})

#if defined(CONFIG_CLI_ASSERT)
#include <assert.h>

#define cli_assert(...) assert(__VA_ARGS__)

#else  /* !defined(CONFIG_CLI_ASSERT) */

#define cli_assert(...)

#endif /* defined(CONFIG_CLI_ASSERT) */

#define cli_log(_format, ...) \
	fprintf(stderr, \
	        "%s: " _format "\n", \
	        program_invocation_short_name, \
	        ## __VA_ARGS__)

#if defined(CONFIG_CLI_ASSERT)

extern void
cli_assert_args(int argc, const char * const argv[]);

#else  /* !defined(CONFIG_CLI_ASSERT) */

static inline void
cli_assert_args(int argc __cli_unused, const char * const argv[] __cli_unused)
{
}

#endif /* defined(CONFIG_CLI_ASSERT) */

extern void *
cli_malloc(size_t size);

extern void *
cli_realloc(void * data, size_t size);

extern char *
cli_strdup(const char * string);

extern int
cli_asprintf(char ** string, const char * format, ...) __cli_printf(2, 3);

static inline void
cli_free(void * data)
{
	free(data);
}

static inline int
cli_render_chr(int chr)
{
	return isprint(chr) ? chr : '?';
}

#define CLI_RENDER_MAX (16U)

extern const char *
cli_render_string(const char * input, char output[CLI_RENDER_MAX]);

#endif /* _CLI_COMMON_H */
