#include "ecfs_exec.h"

#define TRAMP_ADDR 0xc000000
#define TRAMP_SIZE 4096

/* 
 * This creates a code page with instructions to load 
 * the registers with the register set from the snapshotted
 * process
 */
static unsigned long create_reg_loader(struct user_regs_struct *regs, unsigned long entry)
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
 	 * Set rsp
	 */
	*tptr++ = 0x48;
	*tptr++ = 0xbc;
	*(long *)tptr = regs->rsp;
	tptr += 8;

	/*
	 * XXX: temporary until we figure
	 * out a way that doesn't clobber
	 * movabs $entry, %r11
	 * jmpq *%r11
	 */
	*tptr++ = 0x49;
	*tptr++ = 0xbb;
	*(long *)tptr = entry;
	tptr += 8;
	
	*tptr++ = 0x41;
	*tptr++ = 0xff;
	*tptr++ = 0xe3;
	
	return (unsigned long)trampoline;
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
                /* don't remap vsyscall */
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
	
	do_unmappings(old_prog);
		
	ret = load_ecfs_binary(ecfs_desc->mem);
        if (ret == -1) {
                fprintf(stderr, "load_ecfs_binary() failed\n");
                return -1;
        }
	unsigned long tramp_addr = create_reg_loader(&regs[0], (unsigned long)entrypoint);
	tramp_code = (void *)tramp_addr;
	tramp_code();

	exit(0);
}


