#include "ecfs_exec.h"

int ecfs_exec(const char *filename)
{
	ecfs_elf_t *ecfs_desc;
	struct elf_prstatus *prstatus;
	int prcount;
	struct user_regs_struct *regs;

	if ((ecfs_desc = load_ecfs_file(argv[1])) == NULL) {
		printf("error loading file: %s\n", argv[1]);
		exit(-1);
	}
	
	/*
 	 * Convert register set into user_regs_struct for each thread
	 * that was in the process represented by the ecfs file.
 	 */
	prcount = get_prstatus_structs(ecfs_desc, &prstatus);
	regs = malloc(prcount * sizeof(struct user_regs_struct));
	for (i = 0; i < prcount; i++) 
		memcpy(&regs[i], &prstatus[i].pr_reg, sizeof(struct user_regs_struct));
	
	
        void *entry = load_ecfs_binary(ecfs_desc->mem, 1);
        if (entry == NULL) {
                fprintf(stderr, "load_ecfs_binary() failed\n");
                return -1;
        }

	

}


