/*
 * Lightweight Autonomic Network Architecture
 *
 * Functional block userspace configuration tool for LANA.
 *
 * strlcpy taken from the Linux kernel.
 * Copyright 1991, 1992 Linus Torvalds <torvalds@linux-foundation.org>
 *
 * Copyright 2011 Daniel Borkmann <dborkma@tik.ee.ethz.ch>,
 * Swiss federal institute of technology (ETH Zurich)
 * Subject to the GPL.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <linux/netlink.h>
#include <linux/types.h>
#include <linux/if.h>

#include "xt_user.h"

#ifndef likely
# define likely(x) __builtin_expect(!!(x), 1)
#endif
#ifndef unlikely
# define unlikely(x) __builtin_expect(!!(x), 0)
#endif
#ifndef bug
# define bug() __builtin_trap()
#endif

#define PROGNAME "fbctl"
#define VERSNAME "0.9"

size_t strlcpy(char *dest, const char *src, size_t size)
{
	size_t ret = strlen(src);

	if (size) {
		size_t len = (ret >= size) ? size - 1 : ret;
		memcpy(dest, src, len);
		dest[len] = '\0';
	}

	return ret;
}

static inline void die(void)
{
	exit(EXIT_FAILURE);
}

static inline void panic(char *msg, ...)
{
	va_list vl;
	va_start(vl, msg);
	vfprintf(stderr, msg, vl);
	va_end(vl);

	die();
}

static inline void whine(char *msg, ...)
{
	va_list vl;
	va_start(vl, msg);
	vfprintf(stderr, msg, vl);
	va_end(vl);
}

static void *xzmalloc(size_t size)
{
	void *ptr;

	if (unlikely(size == 0))
		panic("xzmalloc: zero size\n");

	ptr = malloc(size);
	if (unlikely(ptr == NULL))
		panic("xzmalloc: out of memory (allocating %lu bytes)\n",
		      (u_long) size);
	memset(ptr, 0, size);

	return ptr;
}

static void xfree(void *ptr)
{
	if (unlikely(ptr == NULL))
		panic("xfree: NULL pointer given as argument\n");
	free(ptr);
}

void check_for_root_maybe_die(void)
{
	if (geteuid() != 0 || geteuid() != getuid())
		panic("Uhhuh, not root?! \n");
}

static void usage(void)
{
	printf("\n%s %s\n", PROGNAME, VERSNAME);
	printf("Usage: %s <cmd> [<args> ...]\n", PROGNAME);
	printf("Commands:\n");
	printf("  preload <module>             - preload module\n");
	printf("  add <name> <type>            - add fblock instance\n");
	printf("  set <name> <key=val>         - set option for fblock\n");
	printf("  rm <name>                    - remove fblock from stack if unbound\n");
	printf("  bind <name1> <name2>         - bind two fblocks\n");
	printf("  unbind <name1> <name2>       - unbind two fblocks\n");
	printf("  replace <name1> <name2>      - exchange fb1 with fb2 (*)\n");
	printf("  replace_drop <name1> <name2> - exchange fb1 with fb2 (*)\n");
	printf("  subscribe <name1> <name2>    - subscribe fb2 to fb1 (+)\n");
	printf("  unsubscribe <name1> <name2>  - unsubscribe fb2 from fb1 (+)\n");
	printf("\n");
	printf("Note (*):\n");
	printf("  (*) 'replace' drops functional block <name1> and replaces\n");
	printf("      it with functional block <name2> where <name1> and\n");
	printf("      <name2> now point to the same functional block. If\n");
	printf("      both are of the same type, private data will be\n");
	printf("      transferred to <name2>. If this is unwanted, use\n");
	printf("      'replace_drop' instead.\n");
	printf("  (+) 'subscribe' is used to receive events from other\n");
	printf("      functional blocks.\n");
	printf("\n");
	printf("Please report bugs to <dborkma@tik.ee.ethz.ch>\n");
	printf("Copyright (C) 2011 Daniel Borkmann\n");
	printf("License: GNU GPL version 2\n");
	printf("This is free software: you are free to change and redistribute it.\n");
	printf("There is NO WARRANTY, to the extent permitted by law.\n\n");

	die();
}

static void version(void)
{
	printf("\n%s %s\n", PROGNAME, VERSNAME);
	printf("Please report bugs to <dborkma@tik.ee.ethz.ch>\n");
	printf("Copyright (C) 2011 Daniel Borkmann\n");
	printf("License: GNU GPL version 2\n");
	printf("This is free software: you are free to change and redistribute it.\n");
	printf("There is NO WARRANTY, to the extent permitted by law.\n\n");

	die();
}

static void do_preload(int argc, char **argv)
{
	int ret, fd;
	char path[256], file[320], cmd[512], *env;
	struct stat sb;

	if (argc != 1)
		panic("Invalid args!\n");

	memset(cmd, 0, sizeof(cmd));
	env = getenv("FBCFG_PRELOAD_DIR");
	if (!env) {
		snprintf(cmd, sizeof(cmd), "modprobe %s", argv[0]);
		cmd[sizeof(cmd) - 1] = 0;
		ret = system(cmd);
		ret = WEXITSTATUS(ret);
		if (ret != 0)
			panic("Preload failed!\n");
		return;
	}

	memset(path, 0, sizeof(path));
	memcpy(path, env, sizeof(path));
	path[sizeof(path) - 1] = 0;
	memset(file, 0, sizeof(file));
	snprintf(file, sizeof(file), "%s%s.ko", path, argv[0]);
	file[sizeof(file) - 1] = 0;

	fd = open(file, O_RDONLY);
	if (fd < 0)
		panic("Module does not exist!\n");
	ret = fstat(fd, &sb);
	if (ret < 0)
		panic("Cannot fstat file!\n");
	if (!S_ISREG (sb.st_mode))
		panic("Module is not a regular file!\n");
	if (sb.st_uid != geteuid())
		panic("Module is not owned by root! Someone could "
		      "compromise your system!\n");
	close(fd);

	snprintf(cmd, sizeof(cmd), "insmod %s", file);
	cmd[sizeof(cmd) - 1] = 0;
	ret = system(cmd);
	ret = WEXITSTATUS(ret);
	if (ret != 0)
		panic("Preload failed!\n");
}

static void send_netlink(struct lananlmsg *lmsg)
{
	int sock, ret;
	struct sockaddr_nl src_addr, dest_addr;
	struct nlmsghdr *nlh;
	struct iovec iov;
	struct msghdr msg;

	if (unlikely(!lmsg))
		return;

	sock = socket(PF_NETLINK, SOCK_RAW, NETLINK_USERCTL);
	if (unlikely(sock < 0))
		panic("Cannot get NETLINK_USERCTL socket from kernel! "
		      "Modules not loaded?!\n");

	memset(&src_addr, 0, sizeof(src_addr));
	src_addr.nl_family = AF_NETLINK;
	src_addr.nl_pad = 0;
	src_addr.nl_pid = getpid();
	src_addr.nl_groups = 0;

	ret = bind(sock, (struct sockaddr *) &src_addr, sizeof(src_addr));
	if (unlikely(ret))
		panic("Cannot bind socket!\n");

	memset(&dest_addr, 0, sizeof(dest_addr));
	dest_addr.nl_family = AF_NETLINK;
	dest_addr.nl_pad = 0;
	dest_addr.nl_pid = 0;
	dest_addr.nl_groups = 0;

	nlh = xzmalloc(NLMSG_SPACE(sizeof(*lmsg)));
	nlh->nlmsg_len = NLMSG_SPACE(sizeof(*lmsg));
	nlh->nlmsg_pid = getpid();
	nlh->nlmsg_type = USERCTLGRP_CONF;
	nlh->nlmsg_flags = NLM_F_REQUEST;

	memcpy(NLMSG_DATA(nlh), lmsg, sizeof(*lmsg));

	iov.iov_base = nlh;
	iov.iov_len = nlh->nlmsg_len;

	memset(&msg, 0, sizeof(msg));
	msg.msg_name = &dest_addr;
	msg.msg_namelen = sizeof(dest_addr);
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;

	ret = sendmsg(sock, &msg, 0);
	if (unlikely(ret < 0))
		panic("Cannot send NETLINK message to the kernel!\n");

	close(sock);
	xfree(nlh);
}

static void do_add(int argc, char **argv)
{
	struct lananlmsg lmsg;
	struct lananlmsg_add *msg;

	if (argc != 2)
		usage();

	memset(&lmsg, 0, sizeof(lmsg));
	lmsg.cmd = NETLINK_USERCTL_CMD_ADD;
	msg = (struct lananlmsg_add *) lmsg.buff;
	strlcpy(msg->name, argv[0], sizeof(msg->name));
	strlcpy(msg->type, argv[1], sizeof(msg->type));
	send_netlink(&lmsg);
}

static void do_set(int argc, char **argv)
{
	struct lananlmsg lmsg;
	struct lananlmsg_set *msg;

	if (argc != 2)
		usage();

	memset(&lmsg, 0, sizeof(lmsg));
	lmsg.cmd = NETLINK_USERCTL_CMD_SET;
	msg = (struct lananlmsg_set *) lmsg.buff;
	strlcpy(msg->name, argv[0], sizeof(msg->name));
	strlcpy(msg->option, argv[1], sizeof(msg->option));
	send_netlink(&lmsg);
}

static void do_rm(int argc, char **argv)
{
	struct lananlmsg lmsg;
	struct lananlmsg_rm *msg;

	if (argc != 1)
		usage();

	memset(&lmsg, 0, sizeof(lmsg));
	lmsg.cmd = NETLINK_USERCTL_CMD_RM;
	msg = (struct lananlmsg_rm *) lmsg.buff;
	strlcpy(msg->name, argv[0], sizeof(msg->name));
	send_netlink(&lmsg);
}

static void do_bind(int argc, char **argv)
{
	struct lananlmsg lmsg;
	struct lananlmsg_bind *msg;

	if (argc != 2)
		usage();

	memset(&lmsg, 0, sizeof(lmsg));
	lmsg.cmd = NETLINK_USERCTL_CMD_BIND;
	msg = (struct lananlmsg_bind *) lmsg.buff;
	strlcpy(msg->name1, argv[0], sizeof(msg->name1));
	strlcpy(msg->name2, argv[1], sizeof(msg->name2));
	send_netlink(&lmsg);
}

static void do_unbind(int argc, char **argv)
{
	struct lananlmsg lmsg;
	struct lananlmsg_unbind *msg;

	if (argc != 2)
		usage();

	memset(&lmsg, 0, sizeof(lmsg));
	lmsg.cmd = NETLINK_USERCTL_CMD_UNBIND;
	msg = (struct lananlmsg_unbind *) lmsg.buff;
	strlcpy(msg->name1, argv[0], sizeof(msg->name1));
	strlcpy(msg->name2, argv[1], sizeof(msg->name2));
	send_netlink(&lmsg);
}

static void do_replace(int argc, char **argv, int drop)
{
	if (argc != 2)
		usage();
}

static void do_subscribe(int argc, char **argv)
{
	if (argc != 2)
		usage();
}

static void do_unsubscribe(int argc, char **argv)
{
	if (argc != 2)
		usage();
}

int main(int argc, char **argv)
{
	check_for_root_maybe_die();

	if (argc <= 1)
		usage();
	argc--;	argv++;
	if (!strncmp("help", argv[0], strlen("help")))
		usage();
	else if (!strncmp("version", argv[0], strlen("version")))
		version();
	else if (!strncmp("preload", argv[0], strlen("preload")))
		do_preload(--argc, ++argv);
	else if (!strncmp("add", argv[0], strlen("add")))
		do_add(--argc, ++argv);
	else if (!strncmp("set", argv[0], strlen("set")))
		do_set(--argc, ++argv);
	else if (!strncmp("rm", argv[0], strlen("rm")))
		do_rm(--argc, ++argv);
	else if (!strncmp("bind", argv[0], strlen("bind")))
		do_bind(--argc, ++argv);
	else if (!strncmp("unbind", argv[0], strlen("unbind")))
		do_unbind(--argc, ++argv);
	else if (!strncmp("replace", argv[0], strlen("replace")))
		do_replace(--argc, ++argv, 0);
	else if (!strncmp("replace-drop", argv[0], strlen("replace-drop")))
		do_replace(--argc, ++argv, 1);
	else if (!strncmp("subscribe", argv[0], strlen("subscribe")))
		do_subscribe(--argc, ++argv);
	else if (!strncmp("unsubscribe", argv[0], strlen("unsubscribe")))
		do_unsubscribe(--argc, ++argv);
	else
		usage();

	return 0;
}
