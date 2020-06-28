#include "disk_driver.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>

#define BLOCK_NUM 50
#define DISK_SIZE BLOCK_SIZE*BLOCK_NUM

void DiskDriver_init(DiskDriver *disk, const char *filename, int num_blocks)
{
    struct stat statbuf;
    DiskHeader header;
    int ret;

    int fd = open(filename, O_CREAT | O_RDWR , 0777);

    ret = fstat(fd, &statbuf);

    if (statbuf.st_size == 0) {
        ret = ftruncate(fd, DISK_SIZE);
        
        // Compile header in the fd
        header.num_blocks = BLOCK_NUM;
        // TODO: capire bitmap_blocks bitmap_entires
        header.free_blocks = BLOCK_NUM;
        header.first_free_block = 0;

        write(fd, &header, sizeof(DiskHeader));
    }

    disk->header = mmap(NULL, sizeof(DiskHeader), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    disk->fd = fd;

    return;
};