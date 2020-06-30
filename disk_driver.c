#include "disk_driver.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_INDEX_FOR_BLOCK_BITMAP (BLOCK_SIZE/sizeof(int))

void DiskDriver_init(DiskDriver *disk, const char *filename, int num_blocks){
    struct stat statbuf;
    DiskHeader header;
    int ret;

    int fd = open(filename, O_CREAT | O_RDWR , 0777);

    ret = fstat(fd, &statbuf);

    if (statbuf.st_size == 0) {
        ret = ftruncate(fd, num_blocks*BLOCK_SIZE);
        
        // Compile header in the fd
        header.num_blocks = num_blocks;
        if(MAX_INDEX_FOR_BLOCK_BITMAP <= num_blocks ) {
            header.bitmap_blocks = 1;
        } else {
            header.bitmap_blocks = (num_blocks+MAX_INDEX_FOR_BLOCK_BITMAP-1)/MAX_INDEX_FOR_BLOCK_BITMAP;
        }
        header.bitmap_entries = header.bitmap_blocks*sizeof(int);
        header.free_blocks = num_blocks - 1 - header.bitmap_blocks;
        header.first_free_block = 1 + header.bitmap_blocks;

        write(fd, &header, sizeof(DiskHeader));
    }

    disk->header = mmap(NULL, sizeof(DiskHeader), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    disk->bitmap_data = mmap(NULL, sizeof(int)*header.bitmap_blocks, PROT_READ | PROT_WRITE, MAP_SHARED, fd, sizeof(DiskHeader));
    disk->fd = fd;

    return;
};

int DiskDriver_writeBlock(DiskDriver* disk, void* src, int block_num){
    assert(disk);
    if(block_num > disk->header->num_blocks || block_num < disk->header->first_free_block) { return -1; }
    int ret;
    int offset = block_num*BLOCK_SIZE; 
    char* tmp = calloc(BLOCK_SIZE, sizeof(char));
    memcpy(tmp, src, strlen(src));
    ret = lseek(disk->fd, offset, SEEK_SET );
    if(ret == -1) perror("Errore nella lseek");
    ret = write(disk->fd, tmp, BLOCK_SIZE);
    return ret;
}

int DiskDriver_readBlock(DiskDriver* disk, void* dest, int block_num){
    assert(disk);
    if(block_num > disk->header->num_blocks || block_num == 0) { return -1; } //TODO: aggiungere condizione lettura blocco libero return -1
    int ret;
    int offset = block_num*BLOCK_SIZE;
    ret = lseek(disk->fd, offset, SEEK_SET );
    if(ret == -1) perror("Errore nella lseek");
    read(disk->fd, dest, BLOCK_SIZE);
    return 0;
}

