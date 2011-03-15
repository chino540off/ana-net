/*
 * Lightweight Autonomic Network Architecture
 *
 * Copyright 2011 Daniel Borkmann <dborkma@tik.ee.ethz.ch>,
 * Swiss federal institute of technology (ETH Zurich)
 * Subject to the GPL.
 */

#ifndef NL_VLINK
#define NL_VLINK

#ifdef __KERNEL__

#include <linux/types.h>
#include <linux/rwsem.h>

#define NETLINK_VLINK_RX_OK     0  /* Receive went okay, notify next     */
#define NETLINK_VLINK_RX_BAD    1  /* Receive failed, notify next        */
#define NETLINK_VLINK_RX_EMERG  2  /* Receive failed, do not notify next */

#define NETLINK_VLINK_PRIO_LOW  0  /* Low priority callbacks             */
#define NETLINK_VLINK_PRIO_NORM 1  /* Normal priority callbacks          */
#define NETLINK_VLINK_PRIO_HIGH 2  /* High priority callbacks            */

#endif /* __KERNEL__ */

#define NETLINK_VLINK          23  /* Netlink hook type                  */

enum nl_vlink_groups {
	VLINKNLGRP_ALL = NLMSG_MIN_TYPE, /* To all vlink types           */
#define VLINKNLGRP_ALL          VLINKNLGRP_ALL
	VLINKNLGRP_ETHERNET,       /* To vlink Ethernet type             */
#define VLINKNLGRP_ETHERNET     VLINKNLGRP_ETHERNET
	VLINKNLGRP_BLUETOOTH,      /* To vlink Bluetooth type            */
#define VLINKNLGRP_BLUETOOTH    VLINKNLGRP_BLUETOOTH
	VLINKNLGRP_INFINIBAND,     /* To vlink InfiniBand type           */
#define VLINKNLGRP_INFINIBAND   VLINKNLGRP_INFINIBAND
	VLINKNLGRP_I2C,            /* To vlink I^2C type                 */
#define VLINKNLGRP_I2C          VLINKNLGRP_I2C
	__VLINKNLGRP_MAX
};
#define VLINKNLGRP_MAX          (__VLINKNLGRP_MAX - 1)

enum nl_vlink_cmd {
	VLINKNLCMD_ADD_DEVICE,
	VLINKNLCMD_RM_DEVICE,
	VLINKNLCMD_BIND_DEVICE,
	/* ... */
};

struct vlinknlmsg {
	uint8_t cmd;
	uint8_t flags;
	uint8_t type;
	uint8_t virt_name[IFNAMSIZ];
	uint8_t real_name[IFNAMSIZ];
	/* ... */
};

#ifdef __KERNEL__

#define MAX_VLINK_SUBSYSTEMS  256

struct nl_vlink_callback {
	int priority;
	int (*rx)(struct sk_buff *skb, struct vlinknlmsg *vhdr,
		  struct nlmsghdr *nlh);
	struct nl_vlink_callback *next;
};

#define NL_VLINK_CALLBACK_INIT(fct, prio) {		\
	.rx = (fct),					\
	.priority = (prio),				\
	.next = NULL, }

struct nl_vlink_subsys {
	char *name;
	u32 type:16,
	    id:8,
	    count:8;
	struct rw_semaphore rwsem;
	struct nl_vlink_callback *head;
};

#define NL_VLINK_SUBSYS_INIT(varname, sysname, type) {	\
	.name = (sysname),				\
	.type = (type),					\
	.count = 0,					\
	.rwsem = __RWSEM_INITIALIZER((varname).rwsem),	\
	.head = NULL, }

extern void nl_vlink_lock(void);
extern void nl_vlink_unlock(void);

extern int nl_vlink_subsys_register(struct nl_vlink_subsys *n);
extern int nl_vlink_subsys_unregister(struct nl_vlink_subsys *n);
extern struct nl_vlink_subsys *nl_vlink_subsys_find(u16 type);

#endif /* __KERNEL__ */
#endif /* NL_VLINK */

