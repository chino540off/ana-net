/*
 * Lightweight Autonomic Network Architecture
 *
 * Random CPU scheduler.
 *
 * Copyright 2011 Daniel Borkmann <dborkma@tik.ee.ethz.ch>,
 * Swiss federal institute of technology (ETH Zurich)
 * Subject to the GPL.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/cache.h>
#include <linux/cpumask.h>
#include <linux/net.h>

#include "xt_sched.h"
#include "xt_engine.h"

static int ppe_rand_sched(struct sk_buff *skb, enum path_type dir)
{
	enqueue_on_engine(skb, (net_random() & (num_online_cpus() - 1)), dir);
	return PPE_SUCCESS;
}

static struct ppesched_discipline_ops ppe_rand_ops __read_mostly = {
	.discipline_sched = ppe_rand_sched,
};

static struct ppesched_discipline ppe_rand __read_mostly = {
	.name = "randcpu",
	.ops = &ppe_rand_ops,
	.owner = THIS_MODULE,
};

static int __init init_ppe_rand_module(void)
{
	return ppesched_discipline_register(&ppe_rand);
}

static void __exit cleanup_ppe_rand_module(void)
{
	return ppesched_discipline_unregister(&ppe_rand);
}

module_init(init_ppe_rand_module);
module_exit(cleanup_ppe_rand_module);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Daniel Borkmann <dborkma@tik.ee.ethz.ch>");
MODULE_DESCRIPTION("LANA random CPU scheduler");
