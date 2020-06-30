#include "simplefs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DISK_NAME "test.fs"

int main(int agc, char **argv) {
  printf("FileControlBlock size %ld\n", sizeof(FileControlBlock));
  printf("FirstFileBlock size %ld\n", sizeof(FirstFileBlock));
  printf("InodeBlock size %ld\n", sizeof(InodeBlock));
  printf("FileBlock size %ld\n", sizeof(FileBlock));
  printf("FirstDirectoryBlock size %ld\n", sizeof(FirstDirectoryBlock));
  printf("DirectoryBlock size %ld\n", sizeof(DirectoryBlock));
  int ret;
  char *read = calloc(BLOCK_SIZE, sizeof(char));
  memset(read, 0, BLOCK_SIZE);

  DiskDriver *disk = malloc(sizeof(DiskDriver));

  DiskDriver_init(disk, DISK_NAME, 10000);

  printf("Bitmap Blocks: %d\n", disk->header->bitmap_blocks);
  printf("Bitmap Entires: %d\n", disk->header->bitmap_entries);
  printf("Reserved blocks: %d\n", disk->reserved_blocks);

  int free_block = DiskDriver_getFreeBlock(disk, 0);

  ret = DiskDriver_writeBlock(disk, "testTest", free_block);
  printf("Scritti blocchi : %d\n", ret);

  ret = DiskDriver_readBlock(disk, read, free_block);
  if (ret)
    printf("Error in read: %d", ret);
  else
    printf("Letto : %s\n", read);

  ret = DiskDriver_getFreeBlock(disk, 0);
  printf("Primo blocco libero : %d\n", ret);

  ret = DiskDriver_freeBlock(disk, free_block);

  memset(read, 0, BLOCK_SIZE);
  ret = DiskDriver_readBlock(disk, read, free_block);
  if (ret)
    printf("Error in read: %d\n", ret);
  else
    printf("Letto : %s\n", read);

  ret = DiskDriver_getFreeBlock(disk, 0);
  printf("Primo blocco libero : %d\n", ret);
}
