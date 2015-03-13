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

int load_ecfs_binary(uint8_t *);
