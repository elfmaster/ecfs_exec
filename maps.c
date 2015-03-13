#include "ecfs_exec.h"

int do_unmappings(const char *progname)
{
	char *p, *q;
	unsigned long start, end;
	char buf[512], orig[512];
	FILE *fd = fopen("/proc/self/maps", "r");
	if (fd == NULL) {
		perror("fopen");
		return -1;
	}
	while (fgets(buf, sizeof(buf), fd)) {
		if (strstr(buf, progname)) {
			memcpy(orig, buf, 512);
			char *lo = buf;
			p = strchr(lo, '-');
			*p = '\0';
			start = strtoul(lo, NULL, 16);
			p++;
			q = strchr(p, 0x20);
			*q = '\0';
			end = strtoul(p, NULL, 16);
			munmap((void *)start, end - start);
		}
	}	
	fclose(fd);
	return 0;
}
