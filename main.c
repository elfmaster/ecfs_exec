#include "ecfs_exec.h"

int main(int argc, char **argv)
{
	ecfs_elf_t *ecfs_desc;
	ecfs_exec_t *exec_desc;

	if (argc < 2) {
		printf("Usage: %s <ecfs_file>\n", argv[0]);
		exit(0);
	}
	
	if ((desc = load_ecfs_file(argv[1])) == NULL) {
		printf("error loading file: %s\n", argv[1]);
		exit(-1);
	}
	
	/*
	 * Move some things from the ecfs descriptor into
	 * our own private descriptor specifically for the
	 * ecfs exec project (because it has some other stuff
	 * that we need, such as info for the linker.
 	 */
	exec_desc->textVaddr = ecfs_desc->textVaddr;
	exec_desc->dataVaddr = ecfs_desc->dataVaddr;
	exec_desc->textSize = ecfs_desc->textSize;
	exec_desc->dataSize = ecfs_desc->dataSize;
	exec_desc->textOff = ecfs_desc->textOff;
	exec_desc->dataOff = ecfs_desc->dataOff;





}


