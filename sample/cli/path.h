#ifndef _CLI_PATH_H
#define _CLI_PATH_H

#include "common.h"

#define CLI_PATH_NAME_MAX (128)
#if CLI_PATH_NAME_MAX <= 64
#error Path name length MUST conform to section 6.2 of RFC 7950 !
#endif

#define CLI_PATH_MAX (512)
#if CLI_PATH_MAX > CLI_LINE_MAX
/* The user would not be able to enter such a long path anymay... */
#undef CLI_PATH_MAX
#define CLI_PATH_MAX CLI_LINE_MAX
#endif

struct cli_path_comp {
	const char * str;
	size_t       len;
};

struct cli_path_stack {
	unsigned int           cnt;
	unsigned int           nr;
	struct cli_path_comp * comps;
};

extern void
cli_path_push_comp(struct cli_path_stack * stack,
                   const char *            component,
                   size_t                  length);

extern ssize_t
cli_path_mkrel_from_stack(struct cli_path_stack * stack,
                          char *                  path,
                          size_t                  length);

extern ssize_t
cli_path_mkabs_from_stack(struct cli_path_stack * stack,
                          char *                  path,
                          size_t                  length);

extern void
cli_path_init_stack(struct cli_path_stack * stack);

extern void
cli_path_fini_stack(struct cli_path_stack * stack);

extern ssize_t
cli_path_isok(const char * path);

#endif /* _CLI_PATH_H */
