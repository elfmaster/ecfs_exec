#include "ecfs_exec.h"

#define ROUNDUP(x, y)   ((((x)+((y)-1))/(y))*(y))
#define ALIGN(k, v) (((k)+((v)-1))&(~((v)-1)))
#define ALIGNDOWN(k, v) ((unsigned long)(k)&(~((unsigned long)(v)-1)))



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


