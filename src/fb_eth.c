/*
 * Lightweight Autonomic Network Architecture
 *
 * Eth/PHY layer. Redirects all traffic into the LANA stack.
 * Singleton object.
 *
 * Copyright 2011 Daniel Borkmann <dborkma@tik.ee.ethz.ch>,
 * Swiss federal institute of technology (ETH Zurich)
 * Subject to the GPL.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/notifier.h>
#include <linux/if_ether.h>
#include <linux/if_arp.h>
#include <linux/if.h>
#include <linux/etherdevice.h>
#include <linux/rtnetlink.h>
#include <linux/seqlock.h>

#include "xt_idp.h"
#include "xt_engine.h"
#include "xt_skb.h"
#include "xt_fblock.h"
#include "xt_builder.h"
#include "xt_vlink.h"

#define IFF_IS_BRIDGED  0x60000

struct fb_eth_priv {
	idp_t port[2];
	seqlock_t lock;
};

static int instantiated = 0;

static struct fblock *fb;

static inline int fb_eth_dev_is_bridged(struct net_device *dev)
{
	return (dev->priv_flags & IFF_IS_BRIDGED) == IFF_IS_BRIDGED;
}

static inline void fb_eth_make_dev_bridged(struct net_device *dev)
{
	dev->priv_flags |= IFF_IS_BRIDGED;
}

static inline void fb_eth_make_dev_unbridged(struct net_device *dev)
{
	dev->priv_flags &= ~IFF_IS_BRIDGED;
}

static rx_handler_result_t fb_eth_handle_frame(struct sk_buff **pskb)
{
	unsigned int seq;
	struct sk_buff *skb = *pskb;
	struct fb_eth_priv __percpu *fb_priv_cpu;

	if (unlikely(skb->pkt_type == PACKET_LOOPBACK))
		return RX_HANDLER_PASS;
	if (unlikely(!is_valid_ether_addr(eth_hdr(skb)->h_source)))
		goto drop;
	skb = skb_share_check(skb, GFP_ATOMIC);
	if (unlikely(!skb))
		return RX_HANDLER_CONSUMED;
	fb_priv_cpu = this_cpu_ptr(rcu_dereference(fb->private_data));
	if (fb_priv_cpu->port[TYPE_INGRESS] == IDP_UNKNOWN)
		goto drop;

	do {
		seq = read_seqbegin(&fb_priv_cpu->lock);
		write_next_idp_to_skb(skb, fb->idp,
				      fb_priv_cpu->port[TYPE_INGRESS]);
	} while (read_seqretry(&fb_priv_cpu->lock, seq));

	process_packet(skb, TYPE_INGRESS);

	return RX_HANDLER_CONSUMED;
drop:
	kfree_skb(skb);
	return RX_HANDLER_CONSUMED;
}

static int fb_eth_netrx(const struct fblock * const fb,
			struct sk_buff * const skb,
			enum path_type * const dir)
{
	if (!skb->dev) {
		kfree_skb(skb);
		return PPE_DROPPED;
	}
	write_next_idp_to_skb(skb, fb->idp, IDP_UNKNOWN);
	dev_queue_xmit(skb);
	return PPE_DROPPED;
}

static int fb_eth_event(struct notifier_block *self, unsigned long cmd,
			void *args)
{
	int ret = NOTIFY_OK;
	unsigned int cpu;
	struct fb_eth_priv __percpu *fb_priv;

	rcu_read_lock();
	fb_priv = (struct fb_eth_priv __percpu *) rcu_dereference_raw(fb->private_data);
	rcu_read_unlock();

	switch (cmd) {
	case FBLOCK_BIND_IDP: {
		int bound = 0;
		struct fblock_bind_msg *msg = args;
		get_online_cpus();
		for_each_online_cpu(cpu) {
			struct fb_eth_priv *fb_priv_cpu;
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
			struct fb_eth_priv *fb_priv_cpu;
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
	default:
		break;
	}

	return ret;
}

static void cleanup_fb_eth(void)
{
	struct net_device *dev;
	rtnl_lock();
	for_each_netdev(&init_net, dev)	{
		if (fb_eth_dev_is_bridged(dev)) {
			netdev_rx_handler_unregister(dev);
			fb_eth_make_dev_unbridged(dev);
		}
	}
	rtnl_unlock();
}

static int init_fb_eth(void)
{
	int ret = 0, err = 0;
	struct net_device *dev;
	rtnl_lock();
	for_each_netdev(&init_net, dev)	{
		ret = netdev_rx_handler_register(dev, fb_eth_handle_frame,
						 NULL);
		if (ret) {
			err = 1;
			break;
		}
		fb_eth_make_dev_bridged(dev);
	}
	rtnl_unlock();
	if (err) {
		cleanup_fb_eth();
		return ret;
	}
	return 0;
}

static struct fblock *fb_eth_ctor(char *name)
{
	int ret = 0;
	unsigned int cpu;
	struct fb_eth_priv __percpu *fb_priv;

	if (instantiated)
		return NULL;
	fb = alloc_fblock(GFP_ATOMIC);
	if (!fb)
		return NULL;

	fb_priv = alloc_percpu(struct fb_eth_priv);
	if (!fb_priv)
		goto err;

	get_online_cpus();
	for_each_online_cpu(cpu) {
		struct fb_eth_priv *fb_priv_cpu;
		fb_priv_cpu = per_cpu_ptr(fb_priv, cpu);
		seqlock_init(&fb_priv_cpu->lock);
		fb_priv_cpu->port[0] = IDP_UNKNOWN;
		fb_priv_cpu->port[1] = IDP_UNKNOWN;
	}
	put_online_cpus();

	ret = init_fblock(fb, name, fb_priv);
	if (ret)
		goto err2;
	fb->netfb_rx = fb_eth_netrx;
	fb->event_rx = fb_eth_event;
	ret = register_fblock_namespace(fb);
	if (ret)
		goto err3;
	ret = init_fb_eth();
	if (ret)
		goto err4;
	__module_get(THIS_MODULE);
	instantiated = 1;
	smp_wmb();
	return fb;
err4:
	unregister_fblock_namespace(fb);
	return NULL;
err3:
	cleanup_fblock_ctor(fb);
err2:
	free_percpu(fb_priv);
err:
	kfree_fblock(fb);
	fb = NULL;
	return NULL;
}

static void fb_eth_dtor(struct fblock *fb)
{
	free_percpu(rcu_dereference_raw(fb->private_data));
	module_put(THIS_MODULE);
	instantiated = 0;
}

static void fb_eth_dtor_outside_rcu(struct fblock *fb)
{
	cleanup_fb_eth();
}

static struct fblock_factory fb_eth_factory = {
	.type = "eth",
	.mode = MODE_SOURCE,
	.ctor = fb_eth_ctor,
	.dtor = fb_eth_dtor,
	.dtor_outside_rcu = fb_eth_dtor_outside_rcu,
	.owner = THIS_MODULE,
};

static struct vlink_subsys fb_eth_sys __read_mostly = {
	.name = "eth",
	.type = VLINKNLGRP_ETHERNET,
	.rwsem = __RWSEM_INITIALIZER(fb_eth_sys.rwsem),
};

static int fb_eth_start_hook_dev(struct vlinknlmsg *vhdr, struct nlmsghdr *nlh)
{
	return NETLINK_VLINK_RX_NXT;
}

static int fb_eth_stop_hook_dev(struct vlinknlmsg *vhdr, struct nlmsghdr *nlh)
{
	return NETLINK_VLINK_RX_NXT;
}

static struct vlink_callback fb_eth_start_hook_dev_cb =
	VLINK_CALLBACK_INIT(fb_eth_start_hook_dev, NETLINK_VLINK_PRIO_HIGH);
static struct vlink_callback fb_eth_stop_hook_dev_cb =
	VLINK_CALLBACK_INIT(fb_eth_stop_hook_dev, NETLINK_VLINK_PRIO_HIGH);

static int __init init_fb_eth_module(void)
{
	int ret = 0;
	ret = vlink_subsys_register(&fb_eth_sys);
	if (ret)
		return ret;

	vlink_add_callback(&fb_eth_sys, &fb_eth_start_hook_dev_cb);
        vlink_add_callback(&fb_eth_sys, &fb_eth_stop_hook_dev_cb);

	ret = register_fblock_type(&fb_eth_factory);
	if (ret)
		vlink_subsys_unregister_batch(&fb_eth_sys);
	return ret;
}

static void __exit cleanup_fb_eth_module(void)
{
	unregister_fblock_type(&fb_eth_factory);
		vlink_subsys_unregister_batch(&fb_eth_sys);
	vlink_subsys_unregister_batch(&fb_eth_sys);
}

module_init(init_fb_eth_module);
module_exit(cleanup_fb_eth_module);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Daniel Borkmann <dborkma@tik.ee.ethz.ch>");
MODULE_DESCRIPTION("Ethernet/PHY link layer bridge driver");

