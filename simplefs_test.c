#include "simplefs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DISK_NAME "test.fs"

int main(int agc, char** argv) {
  printf("FileControlBlock size %ld\n", sizeof(FileControlBlock));
  printf("FirstFileBlock size %ld\n", sizeof(FirstFileBlock));
  printf("InodeBlock size %ld\n", sizeof(InodeBlock));
  printf("FileBlock size %ld\n", sizeof(FileBlock));
  printf("FirstDirectoryBlock size %ld\n", sizeof(FirstDirectoryBlock));
  printf("DirectoryBlock size %ld\n", sizeof(DirectoryBlock));
  int ret;
  char* read = calloc(BLOCK_SIZE, sizeof(char));
  memset(read, 90, BLOCK_SIZE);

  
  DiskDriver *disk = malloc(sizeof(DiskDriver));

  DiskDriver_init(disk, DISK_NAME, 10000);

  ret = DiskDriver_writeBlock(disk, "testTest", 90);
  printf("Scritti blocchi : %d", ret);

  ret = DiskDriver_readBlock(disk, read , 90);
  printf("Letto : %s", read);
  

}
