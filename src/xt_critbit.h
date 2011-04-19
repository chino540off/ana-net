/*
 * Lightweight Autonomic Network Architecture
 *
 * Copyright 2011 Daniel Borkmann <dborkma@tik.ee.ethz.ch>,
 * Swiss federal institute of technology (ETH Zurich)
 * Subject to the GPL.
 */

#ifndef CRITBIT_H
#define CRITBIT_H

#include <linux/spinlock.h>

/*
 * Note that 'myname' is null-terminated and must be power of two aligned,
 * otherwise you trap into unspecified behaviour. To retrive a struct you
 * can do the following:
 *
 * struct mystruct {
 *      char myname[IFNAMSIZ];
 *      int myflag;
 *      ...
 * } __attribute__((aligned(cacheline)));
 *
 * Here, myname is at the start of the struct and cacheline aligned. Then,
 * instantiate such a struct with i.e. name "foobar" and add it to your tree
 * with critbit_insert(&mytree, foo->name).
 *
 * Later on, if your are about to retrive your struct, do a lookup like
 * struct mystruct *bar = struct_of(critbit_get(&mytree, "foobar"), struct mystruct);
 * and there you go. Not that bar points to the same location as foo.
 */

struct critbit_tree {
	void *root;
	spinlock_t wr_lock;
};

#define struct_of(ptr, type)  ({ (type *)(ptr) })

extern int critbit_insert(struct critbit_tree *tree, char *elem);
extern char *critbit_get(struct critbit_tree *tree, const char *elem);
extern int critbit_delete(struct critbit_tree *tree, const char *elem);
extern int critbit_contains(struct critbit_tree *tree, const char *elem);

extern int critbit_node_cache_init(void);
extern void critbit_node_cache_destroy(void);

static inline void critbit_init_tree(struct critbit_tree *tree)
{
	tree->wr_lock = __SPIN_LOCK_UNLOCKED(wr_lock);
}

#endif  /* CRITBIT_H */
