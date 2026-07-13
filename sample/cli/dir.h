#ifndef _CLI_DIR_H
#define _CLI_DIR_H

#include "path.h"
#include <stdbool.h>
#include <string.h>

struct cli_cmd;

#define CLI_DIR_PATH_MAX (512)
#if CLI_DIR_PATH_MAX > CLI_LINE_MAX
/* The user would not be able to enter such a long path anymay... */
#undef CLI_DIR_NAME_MAX
#define CLI_DIR_NAME_MAX CLI_LINE_MAX
#endif

struct cli_dir {
	/* This directory path component. */
	char                     name[CLI_PATH_NAME_MAX];
	/* Next directory entry sibling. */
	struct cli_dir *         next;
	/* Previous directory entry sibling. */
	struct cli_dir *         prev;
	/* First child directory entry. */
	struct cli_dir *         child;
	/* Parent directory entry. */
	struct cli_dir *         parent;
	/* Command singly linked list head. */
	struct cli_cmd *         hcmd;
	/* Command singly linked list tail. */
	struct cli_cmd *         tcmd;
	/* Libyang schema node related to this directory entry. */
	const struct lysc_node * lysc;
};

#define cli_dir_assert(_dir) \
	cli_assert(_dir); \
	cli_assert(((_dir)->name[0] != '\0') && \
	           (strnlen((_dir)->name, CLI_PATH_NAME_MAX) < \
	            CLI_PATH_NAME_MAX)); \
	cli_assert(!(_dir)->hcmd || (_dir)->tcmd)

/*
 * TODO:
 * static_assert(sizeof(_name) <= CLI_PATH_NAME_MAX)
 * see static_assert(3)
 */
#define CLI_DIR_INIT(_dir, _name) \
	{ \
		.name   = _name, \
		.next   = NULL, \
		.prev   = _dir, \
		.child  = NULL, \
		.parent = NULL, \
		.hcmd   = NULL, \
		.tcmd   = NULL, \
		.lysc   = NULL, \
	}

static inline const char *
cli_dir_strerror(int error)
{
	return (error == ENOENT) ? "no such directory"
	                         : cli_path_strerror(error);
}

static inline bool
cli_dir_has_child(const struct cli_dir * directory)
{
	cli_dir_assert(directory);

	return !!directory->child;
}

#define cli_dir_foreach_child(_dir, _child) \
	for (_child = (_dir)->child; _child; _child = (_child)->next)

#define cli_dir_foreach_child_safe(_dir, _child, _tmp) \
	for (_child = (_dir)->child; \
	     _child && (_tmp = (_child)->next, 1); \
	     _child = _tmp)

/*
 * For the directory given in argument, compute a path relavite to an ancestor
 * directory.
 *
 * @directory:  Directory to compute the path for.
 * @ancestor:   An ancestor directory of @directory.
 * @path:       Pre-allocated string where to put the computed absolute path.
 * @size:       Size of @path including the terminating NULL byte.
 *
 * @return: Length of computed relative path, excluding the terminating NULL
 *          byte.
 */
extern ssize_t
cli_dir_mkrel(const struct cli_dir * directory,
              const struct cli_dir * ancestor,
              char *                 path,
              size_t                 size);

/*
 * Compute absolute path for the directory given in argument.
 *
 * @directory:  Directory to compute the path for.
 * @path:       Pre-allocated string where to put the computed absolute path.
 * @size:       Size of @path including the terminating NULL byte.
 *
 * @return: Length of computed absolute path, excluding the terminating NULL
 *          byte.
 */
extern ssize_t
cli_dir_mkabs(const struct cli_dir * directory, char * path, size_t size);

extern char *
cli_dir_xpath(const struct cli_dir * directory);


typedef int cli_dir_visit_fn(struct cli_dir *, enum cli_walk_event, void *);

/*
 * Perform a depth-first traversal of directory tree which root is given as the
 * `directory' argument.
 * 
 * Warning ! The visit() function is not called for the root directory passed in
 *           argument.
 */
extern int
cli_dir_walk(struct cli_dir *   directory,
             cli_dir_visit_fn * visit,
             void *             data);

extern int
cli_dir_walk_safe(struct cli_dir *   directory,
                  cli_dir_visit_fn * visit,
                  void *             data);

extern int
cli_dir_search_from_path(const struct cli_dir ** directory,
                         const struct cli_path * path);

extern int
cli_dir_search(const struct cli_dir ** directory, const char * path);

extern int
cli_dir_parse_cmd(const struct cli_dir * directory,
                  int                    argc,
                  const char * const     argv[],
                  void *                 data);

extern void
cli_dir_add_child(struct cli_dir * directory, struct cli_dir * child);

extern void
cli_dir_add_cmd(struct cli_dir * directory, struct cli_cmd * command);

extern void
_cli_dir_init(struct cli_dir * directory, const char * name, size_t length);

extern int
cli_dir_init(struct cli_dir * directory, const char * name);

extern void
cli_dir_fini(struct cli_dir * directory);

extern struct cli_dir *
cli_dir_create(const char * name);

extern void
cli_dir_destroy(struct cli_dir * directory);

/******************************************************************************
 * Directory search logic for commands usage.
 ******************************************************************************/

struct cli_context;

struct cli_dir_search {
	struct cli_path path;
	const char *    orig;
};

extern int
cli_dir_exec_search(const struct cli_dir_search * search,
                    const struct cli_dir **       directory,
                    const struct cli_context *    context);

extern int
cli_dir_parse_search(struct cli_dir_search * search, const char * path);

static inline void
cli_dir_init_search(struct cli_dir_search * search)
{
	cli_assert(search);

	cli_path_init(&search->path);
	search->orig = NULL;
}

static inline void
cli_dir_fini_search(struct cli_dir_search * search)
{
	cli_assert(search);

	cli_path_fini(&((struct cli_dir_search *)search)->path);
}

#endif  /* _CLI_DIR_H */
