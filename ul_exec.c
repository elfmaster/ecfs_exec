#include "ecfs_exec.h"

#define ROUNDUP(x, y)   ((((x)+((y)-1))/(y))*(y))
#define ALIGN(k, v) (((k)+((v)-1))&(~((v)-1)))
#define ALIGNDOWN(k, v) ((unsigned long)(k)&(~((unsigned long)(v)-1)))



/* 
 * Its very easy to load an ecfs file. The dynamic linker, the stack,
 * the heap, etc. is already in the file so loading it doesn't require
 * any of the extra work needed in a regular ul_exec (e.g. grugqs ul_exec)
 */
void * load_ecfs_binary(char *mapped, int fixed)
{
        Elf64_Ehdr *ehdr;
        Elf64_Phdr *phdr, *interp = NULL;
        void *text_segment = NULL;
        void *entry_point = NULL;
        unsigned long initial_vaddr = 0;
        unsigned long brk_addr = 0;
        char buf[128];
        unsigned int mapflags = MAP_PRIVATE|MAP_ANONYMOUS;
        unsigned int protflags = 0;
        unsigned long map_addr = 0, rounded_len, k;
        unsigned long unaligned_map_addr = 0;
        void *segment;
	struct stat st;
        int i;

        if (fixed)
                mapflags |= MAP_FIXED;
        ehdr = (Elf64_Ehdr *)mapped;
        phdr = (Elf64_Phdr *)(mapped + ehdr->e_phoff);
        entry_point = (void *)ehdr->e_entry;
        
        for (i = 0; i < ehdr->e_phnum; i++) {
                if (phdr[i].p_type != PT_LOAD)
                        continue;
                if (text_segment && !fixed) {
                        unaligned_map_addr
                                = (unsigned long)text_segment
                                + ((unsigned long)phdr[i].p_vaddr - (unsigned long)initial_vaddr);
                        map_addr = ALIGNDOWN((unsigned long)unaligned_map_addr, 0x1000);
                        mapflags |= MAP_FIXED;
                } else if (fixed) {
                        map_addr = ALIGNDOWN(phdr[i].p_vaddr, 0x1000);
                } else {
                        map_addr = 0UL;
                }

                if (fixed && initial_vaddr == 0)
                        initial_vaddr = phdr[i].p_vaddr;

                rounded_len = (unsigned long)phdr[i].p_memsz + ((unsigned long)phdr[i].p_vaddr % 0x1000);
                rounded_len = ROUNDUP(rounded_len, 0x1000);
                segment = mmap(
                        (void *)map_addr,
                        rounded_len,
                        PROT_NONE, mapflags, -1, 0
                );
		
		if (segment == MAP_FAILED ) {
			perror("mmap");
			exit(-1);
		}
                memcpy(
                        fixed ? (void *)phdr[i].p_vaddr:
                        (void *)((unsigned long)segment + ((unsigned long)phdr[i].p_vaddr % 0x1000)),
                        mapped + phdr[i].p_offset,
                        phdr[i].p_filesz
                );      

                if (!text_segment) {
                    	text_segment = segment;
                     	initial_vaddr = phdr[i].p_vaddr;
                        if (!fixed) 
                        	entry_point = (void *)((unsigned long)entry_point
                                - (unsigned long)phdr[i].p_vaddr
                               	+ (unsigned long)text_segment);
                }


                if (phdr[i].p_flags & PF_R)
                        protflags |= PROT_READ;
                if (phdr[i].p_flags & PF_W)
                        protflags |= PROT_WRITE;
                if (phdr[i].p_flags & PF_X)
                        protflags |= PROT_EXEC;
                
                if (mprotect(segment, rounded_len, protflags) < 0) {
                	perror("mprotect");
                	exit(-1);
                }

                k = phdr[i].p_vaddr + phdr[i].p_memsz;
                if (k > brk_addr) 
                        brk_addr = k;
        }

        return (void *)entry_point;

}


