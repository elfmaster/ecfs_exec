#include "ecfs_exec.h"

#define ROUNDUP(x, y)   ((((x)+((y)-1))/(y))*(y))
#define ALIGN(k, v) (((k)+((v)-1))&(~((v)-1)))
#define ALIGNDOWN(k, v) ((unsigned long)(k)&(~((unsigned long)(v)-1)))


void * load_elf_binary(char *mapped, int fixed, Elf64_Ehdr **elf_ehdr, Elf64_Ehdr **ldso_ehdr)
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
        int i;

        if (fixed)
                mapflags |= MAP_FIXED;
        ehdr = (Elf64_Ehdr *)mapped;
        phdr = (Elf64_Phdr *)(mapped + ehdr->e_phoff);
        entry_point = (void *)ehdr->e_entry;
        
        for (i = 0; i < ehdr->e_phnum; i++) {
                if (phdr[i].p_type == PT_INTERP) {
                        interp = (Elf64_Phdr *)&phdr[i];
                        continue;
                } 
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
                        PROT_READ|PROT_WRITE|PROT_EXEC, mapflags, -1, 0
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
                	*elf_ehdr = segment;
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
        if (interp) {
                Elf64_Ehdr *junk_ehdr = NULL;
                char *name = (char *)&mapped[interp->p_offset];
                int fd = open(name, O_RDONLY);
                uint8_t *map = (uint8_t *)mmap(0, LINKER_SIZE, PROT_READ, MAP_PRIVATE, fd, 0);
                if (map == (void *)MAP_FAILED) {
			perror("mmap");
                        exit(-1);
                }
                entry_point = (void *)load_elf_binary(map, 0, ldso_ehdr, &junk_ehdr);
        }

        if (fixed)
                brk(ROUNDUP(brk_addr, 0x1000));

        return (void *)entry_point;

}

unsigned long * create_stack(void)
{
        uint8_t *mem;
        mem = mmap(NULL, STACK_SIZE, PROT_READ|PROT_WRITE|PROT_EXEC, MAP_PRIVATE|MAP_GROWSDOWN|MAP_ANONYMOUS, -1, 0);
        if(mem == MAP_FAILED) {
		perror("mmap");
		exit(-1);
        }
        return (unsigned long *)(mem + STACK_SIZE);
}

ElfW(Addr) build_auxv_stack(struct argdata *args)
{
        uint64_t *esp, *envp, *argv, esp_start;
        int i, count, totalsize, stroffset, len, argc;
        char *strdata, *s;
        void *stack;
        Elf64_auxv_t *auxv;

        count += sizeof(int); // argc
        count += args->argcount * sizeof(char *);
        count += sizeof(void *); // NULL
        count += args->envpcount * sizeof(char *);
        count += sizeof(void *); // NULL
        count += AUXV_COUNT * sizeof(Elf64_auxv_t);
        
        count = (count + 16) & ~(16 - 1);
        totalsize = count + args->envplen + args->arglen;
        totalsize = (totalsize + 16) & ~(16 - 1);
        
        stack = (void *)create_stack();
        
        esp = (uint64_t *)stack;
        uint64_t *sp = esp = esp - (totalsize / sizeof(void *));
        esp_start = (uint64_t)esp;
        strdata = (char *)(esp_start + count);  
        
        s = args->argstr;
        argv = esp;
        envp = argv + args->argcount + 1;
        
        *esp++ = args->argcount;
        for (argc = args->argcount; argc > 0; argc--) {
                strcpy(strdata, s);
                len = _strlen(s) + 1;
                s += len;
                *esp++ = (uintptr_t)strdata;
                strdata += len;
        }
        
        *esp++ = (uintptr_t)0;
	for (s = args->envstr, i = 0; i < args->envpcount; i++) {
                strcpy(strdata, s);
                len = _strlen(s) + 1;
                s += len;
                *esp++ = (uintptr_t)strdata;
                strdata += len;
        }
        
        *esp++ = (uintptr_t)0;
                
        /*
         * Fill out auxillary vector portion of stack
         * so we now have:
         * [argc][argv][envp][auxillary vector][argv/envp strings]
         */
        memcpy((void *)esp, (void *)args->saved_auxv->vector, args->saved_auxv->size);
        auxv = (Elf64_auxv_t *)esp;
        for (i = 0; auxv->a_type != AT_NULL; auxv++)
        {
                switch(auxv->a_type) {
                        case AT_PAGESZ:
                                auxv->a_un.a_val = PAGE_SIZE;
                                break;
                        case AT_PHDR:
                                auxv->a_un.a_val = elf->textVaddr + elf->ehdr->e_phoff;
                                break;
                        case AT_PHNUM:
                                auxv->a_un.a_val = elf->ehdr->e_phnum;
                                break;
                        case AT_BASE:
                                auxv->a_un.a_val = (unsigned long)linker->text;
                                break;
                        case AT_ENTRY:
                                auxv->a_un.a_val = elf->ehdr->e_entry;
                                break;
                        
                }
        }

        return esp_start;

}



