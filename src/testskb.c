/*
 * Lightweight Autonomic Network Architecture
 *
 * Dummy test module.
 *
 * Copyright 2011 Daniel Borkmann <dborkma@tik.ee.ethz.ch>,
 * Swiss federal institute of technology (ETH Zurich)
 * Subject to the GPL.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/skbuff.h>
#include <linux/cpu.h>
#include <linux/jiffies.h>

#include "xt_skb.h"
#include "xt_idp.h"
#include "xt_sched.h"

#define PKTS 1400000UL

static int __init init_fbtestgen_module(void)
{
	unsigned long num = PKTS;
	unsigned long a, b;
	struct sk_buff *skb;
	ppesched_init();

	a = jiffies;
	while (num--) {
		skb = alloc_skb(96, GFP_ATOMIC);
		if (unlikely(!skb))
			return -ENOMEM;
		write_next_idp_to_skb(skb, IDP_UNKNOWN, 1 /* idp 1 */);
		ppesched_sched(skb, TYPE_EGRESS);
	}
	b = jiffies;

	printk(KERN_INFO "test done, %lu pkts in %u us!\n", PKTS,
	       jiffies_to_usecs(b - a));
	return 0;
}

static void __exit cleanup_fbtestgen_module(void)
{
}

module_init(init_fbtestgen_module);
module_exit(cleanup_fbtestgen_module);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Daniel Borkmann <dborkma@tik.ee.ethz.ch>");
MODULE_DESCRIPTION("LANA testgen module");
