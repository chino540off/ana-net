/*
 * Lightweight Autonomic Network Architecture
 *
 * Dummy test module.
 *
 * Copyright 2011 Daniel Borkmann <dborkma@tik.ee.ethz.ch>,
 * Swiss federal institute of technology (ETH Zurich)
 * Subject to the GPL.
 */

#include "xt_fblock.h"
#include "xt_critbit.h"

int fb_test_netrx(struct fblock *fb, struct sk_buff *skb)
{
	printk("Got skb!\n");
	return 0;
}

int fb_test_event(struct notifier_block *self, unsigned long cmd, void *args)
{
	printk("Got event!\n");
	return 0;
}



static struct fblock_ops fb_test_ops = {
	.netfb_rx = fb_test_netrx,
	.event_rx = fb_test_event,
};

static int __init init_fb_test_module(void)
{
	int ret = 0;
	struct fblock *fb_test_block;

	fb_test_block = alloc_fblock(GFP_ATOMIC);
	if (!fb_test_block)
		return -ENOMEM;
	ret = init_fblock(fb_test_block, "fb1", NULL, &ops);
	if (ret)
		goto err;
	register_fblock_namespace(fb_test_block);

	printk(KERN_INFO "[lana] Dummy/test loaded!\n");
	return ret;
err:
	kfree_fblock(fb_test_block);
	return ret;
}

static void __exit cleanup_fb_test_module(void)
{
	unregister_fblock_namespace(fb_test_block);
	printk(KERN_INFO "[lana] Dummy/test removed!\n");
}

module_init(init_fb_test_module);
module_exit(cleanup_fb_test_module);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Daniel Borkmann <dborkma@tik.ee.ethz.ch>");
MODULE_DESCRIPTION("LANA dummy/test module");
