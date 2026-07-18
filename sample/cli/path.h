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

#define cli_path_assert_comp(_comp) \
	cli_assert(_comp); \
	cli_assert((_comp)->str); \
	cli_assert((_comp)->len); \
	cli_assert(!cli_path_comp_isok((_comp)->str, (_comp)->len))

extern const char *
cli_path_strerror(int error);

enum cli_path_comp_kind {
	CLI_PATH_REG_COMP_KIND,   /* Regular path component. */
	CLI_PATH_UPPER_COMP_KIND, /* Upper directory, i.e., `..' */
	CLI_PATH_CURR_COMP_KIND,  /* Current directory, i.e., `.' */
	CLI_PATH_COMP_KIND_NR     /* Invalid component type. */
};

extern enum cli_path_comp_kind
cli_path_comp_kind(const struct cli_path_comp * component);

/*
 * Validate a component over the length given in argument.
 *
 * To keep compliant with YANG identifiers, path component name :
 * - starts with a [a-zA-Z_] character ;
 * - is followed by zero or more [a-zA-Z0-9_-] characters ;
 * - and its entire length may be composed of up to (CLI_PATH_NAME_MAX - 1)
 *   characters.
 * See section 6.2 of RFC 7950 for more informations.
 */
extern int
cli_path_comp_isok(const char * component, size_t length);

extern int
cli_path_comp_ncmp(const struct cli_path_comp * component,
                   const char *                 string,
                   size_t                       length);

extern int
cli_path_comp_cmp(const struct cli_path_comp * component,
                  const char *                 string);

struct cli_path {
	unsigned int           head;
	unsigned int           cnt;
	unsigned int           nr;
	struct cli_path_comp * comps;
};

#define cli_path_assert(_stk) \
	cli_assert(_stk); \
	cli_assert((_stk)->nr); \
	cli_assert((_stk)->head < (_stk)->nr); \
	cli_assert((_stk)->cnt <= (_stk)->nr); \
	cli_assert((_stk)->comps)

#define cli_path_foreach_comp(_path, _indx, _cnt, _comp) \
	for (_indx = (_path)->head, \
	     _cnt = (_path)->cnt, \
	     _comp = &(_path)->comps[_indx]; \
	     \
	     _cnt; \
	     \
	     _indx = ((_indx) + 1) % (_path)->nr, \
	     _cnt = (_cnt) - 1, \
	     _comp = &(_path)->comps[_indx])

#define cli_path_foreach_comp_continue(_path, _indx, _cnt, _comp) \
	for (_indx = ((_indx) + 1) % (_path)->nr, \
	     _cnt = (_cnt) - 1, \
	     _comp = &(_path)->comps[_indx]; \
	     \
	     _cnt; \
	     \
	     _indx = ((_indx) + 1) % (_path)->nr, \
	     _cnt = (_cnt) - 1, \
	     _comp = &(_path)->comps[_indx])

#define cli_path_foreach_comp_from(_path, _indx, _cnt, _comp) \
	for (; \
	     _cnt; \
	     \
	     _indx = ((_indx) + 1) % (_path)->nr, \
	     _cnt = (_cnt) - 1, \
	     _comp = &(_path)->comps[_indx])

static inline unsigned int
cli_path_comp_count(const struct cli_path * path)
{
	cli_path_assert(path);

	return path->cnt;
}

extern ssize_t
cli_path_mkrel(struct cli_path * path, char * string, size_t size);

extern ssize_t
cli_path_mkabs(struct cli_path * path, char * string, size_t size);

extern void
cli_path_push_head(struct cli_path * path,
                   const char *      component,
                   size_t            length);

extern void
cli_path_push_tail(struct cli_path * path,
                   const char *      component,
                   size_t            length);

extern int
cli_path_parse(struct cli_path * path, const char * string);

extern void
cli_path_init(struct cli_path * path);

extern void
cli_path_fini(struct cli_path * path);

extern struct cli_path *
cli_path_create(void);

extern void
cli_path_destroy(struct cli_path * path);

extern ssize_t
cli_path_normalize(const char * path, char * norm, size_t size);

#warning Remove calls to cli_path_isok() and replace calls with cli_path_parse()
extern int
cli_path_isok(const char * string);

#endif /* _CLI_PATH_H */
