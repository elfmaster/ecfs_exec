#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>

int main(int argc, char **argv)
{
	int pid;
	void *handle;
	int (*ecfs_exec)(char **, char *);
	int status;

	if (argc < 2) {
		printf("Usage: %s <ecfs_file>\n", argv[0]);
		exit(-1);
	}
	/*
	 * The LD_BIND_NOW is necessary because the userland
	 * exec will unmap() the main executable mappings that
	 * it overwrites with the new program, and meanwhile
	 * any dynamic linker resolution will fail because the
	 * old mappings won't be there for it to fix up.
	 * so we must resolve everything immediately.
	 */
	 
	putenv("LD_BIND_NOW=1");
	handle = dlopen("./libecfsexec.so", RTLD_LAZY);
	if (handle == NULL) {
		fprintf(stderr, "dlopen failed: %s", dlerror());
		exit(-1);
	}
	ecfs_exec = dlsym(handle, "ecfs_exec");
	if (ecfs_exec == NULL) {
		fprintf(stderr, "dlsym failed: %s", dlerror());
		exit(-1);
	}
	ecfs_exec(argv, argv[1]);
	/*
	pid = fork();
	if (pid == 0) {
		ecfs_exec(argv, argv[1]);
		exit(0);
	}
	wait(&status);
	if (!WIFEXITED(status)) {
		printf("Executing %s failed\n", argv[1]);
	}	
	*/
	exit(0);

}
