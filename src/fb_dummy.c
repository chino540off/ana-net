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
#include <linux/spinlock.h>
#include <linux/notifier.h>
#include <linux/rcupdate.h>
#include <linux/seqlock.h>
#include <linux/percpu.h>
#include <linux/prefetch.h>

#include "xt_fblock.h"
#include "xt_builder.h"
#include "xt_idp.h"
#include "xt_skb.h"
#include "xt_engine.h"
#include "xt_builder.h"

struct fb_dummy_priv {
	idp_t port[2];
	seqlock_t lock;
};

static int fb_dummy_netrx(const struct fblock * const fb,
			  struct sk_buff * const skb,
			  enum path_type * const dir)
{
	int drop = 0;
	unsigned int seq;
	struct fb_dummy_priv __percpu *fb_priv_cpu;

	fb_priv_cpu = this_cpu_ptr(rcu_dereference_raw(fb->private_data));
#ifdef __DEBUG
	printk("Got skb on %p on ppe%d!\n", fb, smp_processor_id());
#endif
	prefetchw(skb->cb);
	do {
		seq = read_seqbegin(&fb_priv_cpu->lock);
		write_next_idp_to_skb(skb, fb->idp, fb_priv_cpu->port[*dir]);
		if (fb_priv_cpu->port[*dir] == IDP_UNKNOWN)
			drop = 1;
	} while (read_seqretry(&fb_priv_cpu->lock, seq));
	if (drop) {
		kfree_skb(skb);
		return PPE_DROPPED;
	}
	return PPE_SUCCESS;
}

static int fb_dummy_event(struct notifier_block *self, unsigned long cmd,
			  void *args)
{
	int ret = NOTIFY_OK;
	unsigned int cpu;
	struct fblock *fb;
	struct fb_dummy_priv __percpu *fb_priv;

	rcu_read_lock();
	fb = rcu_dereference_raw(container_of(self, struct fblock_notifier, nb)->self);
	fb_priv = (struct fb_dummy_priv __percpu *) rcu_dereference_raw(fb->private_data);
	rcu_read_unlock();

#ifdef __DEBUG
	printk("Got event %lu on %p!\n", cmd, fb);
#endif

	switch (cmd) {
	case FBLOCK_BIND_IDP: {
		int bound = 0;
		struct fblock_bind_msg *msg = args;
		get_online_cpus();
		for_each_online_cpu(cpu) {
			struct fb_dummy_priv *fb_priv_cpu;
			fb_priv_cpu = per_cpu_ptr(fb_priv, cpu);
			if (fb_priv_cpu->port[msg->dir] == IDP_UNKNOWN) {
				write_seqlock(&fb_priv_cpu->lock);
				fb_priv_cpu->port[msg->dir] = msg->idp;
				write_sequnlock(&fb_priv_cpu->lock);
				bound = 1;
			} else {
				ret = NOTIFY_BAD;
				break;
			}
		}
		put_online_cpus();
		if (bound)
			printk(KERN_INFO "[%s::%s] port %s bound to IDP%u\n",
			       fb->name, fb->factory->type,
			       path_names[msg->dir], msg->idp);
		} break;
	case FBLOCK_UNBIND_IDP: {
		int unbound = 0;
		struct fblock_bind_msg *msg = args;
		get_online_cpus();
		for_each_online_cpu(cpu) {
			struct fb_dummy_priv *fb_priv_cpu;
			fb_priv_cpu = per_cpu_ptr(fb_priv, cpu);
			if (fb_priv_cpu->port[msg->dir] == msg->idp) {
				write_seqlock(&fb_priv_cpu->lock);
				fb_priv_cpu->port[msg->dir] = IDP_UNKNOWN;
				write_sequnlock(&fb_priv_cpu->lock);
				unbound = 1;
			} else {
				ret = NOTIFY_BAD;
				break;
			}
		}
		put_online_cpus();
		if (unbound)
			printk(KERN_INFO "[%s::%s] port %s unbound\n",
			       fb->name, fb->factory->type,
			       path_names[msg->dir]);
		} break;
	case FBLOCK_SET_OPT: {
		struct fblock_opt_msg *msg = args;
		printk("Set option %s to %s!\n", msg->key, msg->val);
		} break;
	default:
		break;
	}

	return ret;
}

static struct fblock *fb_dummy_ctor(char *name)
{
	int ret = 0;
	unsigned int cpu;
	struct fblock *fb;
	struct fb_dummy_priv __percpu *fb_priv;

	fb = alloc_fblock(GFP_ATOMIC);
	if (!fb)
		return NULL;

	fb_priv = alloc_percpu(struct fb_dummy_priv);
	if (!fb_priv)
		goto err;

	get_online_cpus();
	for_each_online_cpu(cpu) {
		struct fb_dummy_priv *fb_priv_cpu;
		fb_priv_cpu = per_cpu_ptr(fb_priv, cpu);
		seqlock_init(&fb_priv_cpu->lock);
		fb_priv_cpu->port[0] = IDP_UNKNOWN;
		fb_priv_cpu->port[1] = IDP_UNKNOWN;
	}
	put_online_cpus();

	ret = init_fblock(fb, name, fb_priv);
	if (ret)
		goto err2;
	fb->netfb_rx = fb_dummy_netrx;
	fb->event_rx = fb_dummy_event;
	ret = register_fblock_namespace(fb);
	if (ret)
		goto err3;
	__module_get(THIS_MODULE);
	return fb;
err3:
	cleanup_fblock_ctor(fb);
err2:
	free_percpu(fb_priv);
err:
	kfree_fblock(fb);
	return NULL;
}

static void fb_dummy_dtor(struct fblock *fb)
{
	free_percpu(rcu_dereference_raw(fb->private_data));
	module_put(THIS_MODULE);
}

static struct fblock_factory fb_dummy_factory = {
	.type = "dummy",
	.mode = MODE_DUAL,
	.ctor = fb_dummy_ctor,
	.dtor = fb_dummy_dtor,
	.owner = THIS_MODULE,
};

static int __init init_fb_dummy_module(void)
{
	return register_fblock_type(&fb_dummy_factory);
}

static void __exit cleanup_fb_dummy_module(void)
{
	synchronize_rcu();
	unregister_fblock_type(&fb_dummy_factory);
}

module_init(init_fb_dummy_module);
module_exit(cleanup_fb_dummy_module);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Daniel Borkmann <dborkma@tik.ee.ethz.ch>");
MODULE_DESCRIPTION("LANA dummy/test module");
