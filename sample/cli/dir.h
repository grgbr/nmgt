#ifndef _CLI_DIR_H
#define _CLI_DIR_H

#include "path.h"
#include "cmd.h"
#include <stdbool.h>
#include <string.h>

struct cli_context;

#define CLI_DIR_PATH_MAX (512)
#if CLI_DIR_PATH_MAX > CLI_LINE_MAX
/* The user would not be able to enter such a long path anymay... */
#undef CLI_DIR_NAME_MAX
#define CLI_DIR_NAME_MAX CLI_LINE_MAX
#endif

enum cli_dir_type {
	CLI_DIR_NONE_TYPE,
	CLI_DIR_NODE_TYPE,
	CLI_DIR_MOD_TYPE,
	CLI_DIR_TYPE_NR
};

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
	/* Command linked list head. */
	struct cli_cmd *         cmds;
	enum cli_dir_type        type;
	/* Libyang schema module or node related to this directory entry. */
	union {
		const struct lysc_node *  sch_node;
		const struct lys_module * sch_mod;
		const void *              sch_void;
	};
};

#define cli_dir_assert(_dir) \
	cli_assert(_dir); \
	cli_assert(((_dir)->name[0] != '\0') && \
	           (strnlen((_dir)->name, CLI_PATH_NAME_MAX) < \
	            CLI_PATH_NAME_MAX)); \
	cli_assert(((_dir)->type == CLI_DIR_NONE_TYPE) || \
	           ((_dir)->type == CLI_DIR_MOD_TYPE) || \
	           ((_dir)->type == CLI_DIR_NODE_TYPE)); \
	cli_assert(((_dir)->type == CLI_DIR_NONE_TYPE) || (_dir)->sch_void);

static inline const char *
cli_dir_strerror(int error)
{
	switch (error) {
	case ENOENT:
		return "no such directory";
	case EBADR:
		return "internal YANG error";
	default:
		return cli_path_strerror(error);
	}
}

static inline enum cli_dir_type
cli_dir_type(const struct cli_dir * directory)
{
	cli_dir_assert(directory);

	return directory->type;
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

extern int
cli_dir_show_yang(const struct cli_dir *     directory,
                  const struct cli_context * context);

extern int
cli_dir_show_diag(const struct cli_dir *     directory,
                  const struct cli_context * context);

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
                  struct cli_context *   context,
                  int                    argc,
                  const char * const     argv[]);

extern void
cli_dir_add_child(struct cli_dir * directory, struct cli_dir * child);

static inline void
cli_dir_add_cmd(struct cli_dir * directory, struct cli_cmd * command)
{
	cli_dir_assert(directory);
	cli_cmd_assert(command);

	cli_node_add_sibling((struct cli_node **)&directory->cmds,
	                     &command->super);
}

extern void
cli_dir_init_root(struct cli_dir * root);

extern void
cli_dir_fini_root(struct cli_dir * root);

extern struct cli_dir *
cli_dir_create(const char * name, enum cli_dir_type type, const void * schema);

static inline struct cli_dir *
cli_dir_create_none(const char * name)
{
	cli_assert(name);

	return cli_dir_create(name, CLI_DIR_NONE_TYPE, NULL);
}

static inline struct cli_dir *
cli_dir_create_node(const char * name, const struct lysc_node * node)
{
	cli_assert(name);

	return cli_dir_create(name, CLI_DIR_NODE_TYPE, node);
}

static inline struct cli_dir *
cli_dir_create_module(const char * name, const struct lys_module * module)
{
	cli_assert(name);

	return cli_dir_create(name, CLI_DIR_MOD_TYPE, module);
}

extern void
cli_dir_destroy(struct cli_dir * directory);

/******************************************************************************
 * Directory search logic for commands usage.
 ******************************************************************************/

#include "work.h"

struct cli_dir_work {
	/* Base work structure. */
	struct cli_work        super;
	/* The command that initiated this work. */
	const struct cli_cmd * cmd;
	/* Internal state of requested path search. */
	struct cli_path        path;
	/* Pointer to original path argument. */
	const char *           orig;
	/* Length of `norm' field, excluding the terminating NULL byte. */
	size_t                 len;
	/* Validated normalized requested path. */
	char                   norm[CLI_PATH_MAX];
};

extern int
cli_dir_work_search(struct cli_dir_work *   work,
                    struct cli_context *    context,
                    const struct cli_dir ** directory);

extern int
cli_dir_work_parse(struct cli_dir_work *   work,
                   int                     argc,
                   const char * const      argv[],
                   bool                    mandatory);

extern struct cli_dir_work *
cli_dir_work_create(size_t                      size,
                    const struct cli_cmd *      command,
                    const struct cli_work_ops * opers);

extern void
cli_dir_work_destroy(struct cli_dir_work * work);

extern void
cli_dir_work_release(struct cli_work * work);

extern struct cli_arg *
cli_dir_work_create_arg(bool mandatory);

#endif /* _CLI_DIR_H */
