#include "ecfs_exec.h"

#define TRAMP_SIZE 4096
#define MAX_THREADS 16

__thread long dummy_array[1024];
/* 
 * This creates a code page with instructions to load 
 * the registers with the register set from the snapshotted
 * process, and then transfers control to the entry point
 * which is the address of where the process-snapshot left
 * off.
 */
static unsigned long create_reg_loader(struct user_regs_struct *regs, unsigned long fpreg_va, unsigned long entry)
{
	uint8_t *trampoline;
	uint8_t *tptr;
	
	trampoline = mmap(NULL, TRAMP_SIZE, PROT_READ|PROT_WRITE|PROT_EXEC, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
	if (trampoline == MAP_FAILED ) {
		perror("mmap");
		exit(-1);
	}
	tptr = trampoline;
	
	/* 
	 * restore floating point registers
	 * movq $fpreg_va, %rdi
	 * fldenv (%rdi)
	 */
	if (fpreg_va < (uint32_t)~0x0) {
		*tptr++ = 0x48;
		*tptr++ = 0xc7;
		*tptr++ = 0xc7;
		*(uint32_t *)tptr = (uint32_t)fpreg_va;
		tptr += 4;
	} else {
		*tptr++ = 0x48;
		*tptr++ = 0xbf;
		*(long *)tptr = fpreg_va;
		tptr += 8;
	}
        /*
         * movabsq $regs->rsp, %rsp
         */
        *tptr++ = 0x48;
        *tptr++ = 0xbc;
        *(long *)tptr = regs->rsp;
        tptr += 8;

	/*
	 * Store entry point address at top of stack
	 * sub 0x8, %rsp
	 */
	*tptr++ = 0x48;
	*tptr++ = 0x83;
	*tptr++ = 0xec;
	*tptr++ = 0x10;
	
	/*
	 * movq $value, %rax
	 */
	*tptr++ = 0x48;
	*tptr++ = 0xb8;
	*(long *)tptr = entry;
	tptr += 8;
	
	/*
	 * movq %rax, (%rsp)
	 */
	*tptr++ = 0x48;
	*tptr++ = 0x89;
	*tptr++ = 0x04;
	*tptr++ = 0x24;
	/*
	 * The following instructions are to set all of the registers
	 * i.e: movabs $value, %rax (For each general purpose reg)
	 */
	
	*tptr++ = 0x48;
	*tptr++ = 0xb8;
	*(long *)tptr = regs->rax;
	tptr += 8;
	
        *tptr++ = 0x48;
        *tptr++ = 0xbb;
        *(long *)tptr = regs->rbx;
        tptr += 8;
	
        *tptr++ = 0x48;
        *tptr++ = 0xb9;
        *(long *)tptr = regs->rcx;
        tptr += 8;

        *tptr++ = 0x48;
        *tptr++ = 0xba;
        *(long *)tptr = regs->rdx;
        tptr += 8;
	
        *tptr++ = 0x48;
        *tptr++ = 0xbf;
        *(long *)tptr = regs->rdi;
        tptr += 8;

        *tptr++ = 0x48;
        *tptr++ = 0xbe;
        *(long *)tptr = regs->rsi;
        tptr += 8;

	*tptr++ = 0x49;
        *tptr++ = 0xbf;
        *(long *)tptr = regs->r15;
        tptr += 8;

        *tptr++ = 0x49;
        *tptr++ = 0xbe;
        *(long *)tptr = regs->r14;
        tptr += 8;

        *tptr++ = 0x49;
        *tptr++ = 0xbd;
        *(long *)tptr = regs->r13;
        tptr += 8;
	
        *tptr++ = 0x49;
        *tptr++ = 0xbc;
        *(long *)tptr = regs->r12;
        tptr += 8;

        *tptr++ = 0x49; 
        *tptr++ = 0xbb;
        *(long *)tptr = regs->r11;
        tptr += 8;

        *tptr++ = 0x49; 
        *tptr++ = 0xba;
        *(long *)tptr = regs->r10;
        tptr += 8;

        *tptr++ = 0x49; 
        *tptr++ = 0xb9;
        *(long *)tptr = regs->r9;
        tptr += 8;

        *tptr++ = 0x49; 
        *tptr++ = 0xb8;
        *(long *)tptr = regs->r8;
        tptr += 8;
	
	/*
	 * retq $0x8
	 */
	*tptr++ = 0xc2;
	*tptr++ = 0x08;
	*tptr++ = 0x00;

	return (unsigned long)trampoline;
}

/*
 * Set up file descriptor state for
 * snapshot execution.
 */
void reload_file_desc(fd_info_t *fdinfo)
{
	int ret;
	int fd;

	if (fdinfo->fd <= 3) // 0 - 3 are reserved and will be opened by this process
		return;
	if (fdinfo->net) {
		fd = open("/dev/null", O_RDWR);	// we don't handle sockets yet
		return;
	}
	if (fdinfo->path[0] == '\0') {
		fd = open("/dev/null", O_RDWR);
		return;
	}
	if (strchr(fdinfo->path, ']')) {// pipe:[8199] 
		fd = open("/dev/null", O_RDWR);
		return; 
	}
	if (strstr(fdinfo->path, "anon_inode")) {
		fd = open("/dev/null", O_RDWR);
		return;
	}
#if DEBUG
	fprintf(stderr, "[ecfs_exec] opening %s\n", fdinfo->path);
#endif
	fd = open(fdinfo->path, fdinfo->perms);
	if (fd < 0) {
		fprintf(stderr, "[ecfs_exec]: failed to open(%s, %04x): %s\n", fdinfo->path, fdinfo->perms, strerror(errno));
		exit(-1);
	}
	
	if (fdinfo->pos != 0) {
		ret = lseek(fd, fdinfo->pos, SEEK_SET);
		if (ret < 0) {
			fprintf(stderr, "[ecfs_exec]: failed to lseek(%d, %lx, SEEK_SET): %s\n", fd, fdinfo->pos, strerror(errno));
			exit(-1);
		}	
	}
	 
	
}

/* 
 * Its very easy to load an ecfs file. The dynamic linker, the stack,
 * the heap, etc. is already in the file so loading it doesn't require
 * any of the extra work needed in a regular ul_exec (e.g. grugqs ul_exec)
 */
int load_ecfs_binary(uint8_t *mapped)
{
        Elf64_Ehdr *ehdr;
        Elf64_Phdr *phdr;
        void *segment;
        uint8_t *mem = mapped;
        int i;

        ehdr = (Elf64_Ehdr *)mapped;
        phdr = (Elf64_Phdr *)&mem[ehdr->e_phoff];

        for (i = 0; i < ehdr->e_phnum; i++) {
                if (phdr[i].p_type != PT_LOAD)
                        continue;
                /* Until we fix ecfs bug writing some empty segments */
                if (phdr[i].p_filesz == 0)
                        continue;
                /* don't remap vsyscall 
		 * the kernel will do 
		 * that for us.
		 */
                if (phdr[i].p_vaddr == 0xffffffffff600000)
                        continue;
                segment = mmap((void *)phdr[i].p_vaddr, phdr[i].p_memsz, PROT_READ|PROT_WRITE|PROT_EXEC, MAP_PRIVATE|MAP_FIXED|MAP_ANONYMOUS, -1, 0);
                if (segment == MAP_FAILED) {
                        perror("mmap");
                        exit(-1);
                }
                /*
                 * note: in ecfs files memsz and filesz are the same
                 */
                memcpy(segment, &mem[phdr[i].p_offset], phdr[i].p_memsz);
        }
        
}


int ecfs_exec(char **argv, const char *filename)
{
	int i, ret;
	ecfs_elf_t *ecfs_desc;
	siginfo_t siginfo;
	struct elf_prstatus *prstatus;
	int prcount;
	struct user_regs_struct *regs;
	const char *old_prog = argv[0];
	void *fault_address;
	void *entrypoint, *stack;
	void (*tramp_code)(void);
	unsigned long tramp_addr;
	unsigned long fpreg_addr;
	pid_t tid[MAX_THREADS];
	fd_info_t *fdinfo;

	if ((ecfs_desc = load_ecfs_file(filename)) == NULL) {
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
	
	get_siginfo(ecfs_desc, &siginfo);
		
	entrypoint = (void *)regs[0].rip;
	stack = (void *)regs[0].rsp;
	
#if DEBUG
	printf("[+] Using entry point: %lx\n", entrypoint);
	printf("[+] Using stack vaddr: %lx\n", stack);
#endif

	do_unmappings(old_prog);
		
	/*
	 * load ecfs binary into memory to prepare
	 * for execution.
	 */
	ret = load_ecfs_binary(ecfs_desc->mem);
        if (ret == -1) {
                fprintf(stderr, "load_ecfs_binary() failed\n");
                return -1;
        }
	
	/*
	 * restore file descriptors
	 */
	int fdcount = get_fd_info(ecfs_desc, &fdinfo);
	for (i = 0; i < fdcount; i++) {
		reload_file_desc(&fdinfo[i]);
	}
	
	/*
 	 * get source location of fpregset_t 
	 */
	fpreg_addr = get_section_va(ecfs_desc, ".fpregset");
		
		
	/*
	 * inject code for restoring register state
	 * and setting up trampoline for execution on
	 * entry point.
	 */
	tramp_addr = create_reg_loader(&regs[0], fpreg_addr, (unsigned long)entrypoint);
	tramp_code = (void *)tramp_addr;
	tramp_code();
	
		
	
	exit(0);
}


