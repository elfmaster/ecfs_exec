#include "libecfs.h"


struct fileinfo {
	int fd;
	char *path;
};


typedef struct {
        int size;
        int count;
        uint8_t *vector;
} auxv_t;

struct argdata {
        int argcount;
        int arglen;
        char *argstr;
        
        int envpcount;
        int envplen;
        char *envstr;
        auxv_t *saved_auxv;
}; 

typedef struct elfinfo {
	Elf64_Ehdr *ehdr;
	Elf64_Phdr *phdr;
	Elf64_Shdr *shdr;
	Elf64_auxv_t *auxv;
	Elf64_Addr textVaddr;
	Elf64_Addr dataVaddr;
	Elf64_Off textOff;
	Elf64_Off dataOff;
	size_t textSize;
	size_t dataSize;
} elfinfo_t;
	
struct linker {
        Elf64_Addr entry;
        Elf64_Addr textVaddr;
        unsigned int textSize;
        unsigned int textOff;
        Elf64_Addr dataVaddr;
        unsigned int dataSize;
        unsigned int dataOff;
        unsigned char *text;
        unsigned char *data;
        Elf64_Ehdr *ehdr;
        Elf64_Phdr *phdr;
};


typedef struct ecfs_exec {
	struct linker *linker;
	struct fileinfo *fileinfo;
	elfinfo_t *elfinfo;
	int filecount;
	struct user_regs_struct *regs;
} ecfs_exec_t;
