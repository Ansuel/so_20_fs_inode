#include "simplefs.h"
#include <stdio.h>
#include <stdlib.h>

#define DISK_NAME "test.fs"

int main(int agc, char** argv) {
  printf("FirstBlock size %ld\n", sizeof(FirstFileBlock));
  printf("DataBlock size %ld\n", sizeof(FileBlock));
  printf("FirstDirectoryBlock size %ld\n", sizeof(FirstDirectoryBlock));
  printf("DirectoryBlock size %ld\n", sizeof(DirectoryBlock));
  
  DiskDriver *disk = malloc(sizeof(DiskDriver));

  DiskDriver_init(disk, DISK_NAME, 0);

  printf("%d", disk->header->num_blocks);

  disk->header->num_blocks = 43;


}
