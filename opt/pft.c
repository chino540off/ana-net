#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

/* assumed more pagefaults */
void test1(char *addr, size_t len)
{
	size_t i;
	for (i = 0; i < len; ++i)
		addr[i]++;
}

/* assumed less pagefaults */
void test2(char *addr, size_t len)
{
	size_t i, pgsiz = sysconf(_SC_PAGE_SIZE);
	for (i = 0; i < len; ++i)
		addr[i & (pgsiz-1)]++;
}

int main(int argc, char **argv)
{
	int fd, ret;
	char *addr;
	struct stat sb;
	if (argc != 3) {
		printf("Usage: ./a.out <file> <test-nr:0|1>\n");
		return -EIO;
	}
	fd = open(argv[1], O_RDONLY);
	if (fd < 0)
		return -EIO;
	ret = fstat(fd, &sb);
	if (ret < 0)
		return -EIO;
	addr = mmap(NULL, sb.st_size, PROT_READ | PROT_WRITE,
		    MAP_PRIVATE, fd, 0);
	if (addr == MAP_FAILED)
		return -errno;
	if (atoi(argv[2]) == 1)
		test2(addr, sb.st_size);
	else
		test1(addr, sb.st_size);
	munmap(addr, sb.st_size);
	close(fd);
	return 0;
}

