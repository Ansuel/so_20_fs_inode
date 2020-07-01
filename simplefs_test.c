#include "simplefs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
       #include <sys/stat.h>
       #include <fcntl.h>
        #include <unistd.h>

#define DISK_NAME "test.fs"

int main(int agc, char **argv) {
  printf("FileControlBlock size %ld\n", sizeof(FileControlBlock));
  printf("FirstFileBlock size %ld\n", sizeof(FirstFileBlock));
  printf("InodeBlock size %ld\n", sizeof(InodeBlock));
  printf("FileBlock size %ld\n", sizeof(FileBlock));
  printf("FirstDirectoryBlock size %ld\n", sizeof(FirstDirectoryBlock));
  printf("DirectoryBlock size %ld\n", sizeof(DirectoryBlock));

  // TEST DISK_DRIVER.C

  int ret;
  char *readed = calloc(BLOCK_SIZE, sizeof(char));
  memset(readed, 0, BLOCK_SIZE);

  DiskDriver *disk = malloc(sizeof(DiskDriver));

  DiskDriver_init(disk, DISK_NAME, 100000);

  // printf("Bitmap Blocks: %d\n", disk->header->bitmap_blocks);
  // printf("Bitmap Entires: %d\n", disk->header->bitmap_entries);
  // printf("Reserved blocks: %d\n", disk->reserved_blocks);

  // int free_block = DiskDriver_getFreeBlock(disk, 0);

  // ret = DiskDriver_writeBlock(disk, "testTest", free_block);
  // printf("Caratteri scritti : %d\n", ret);

  // ret = DiskDriver_readBlock(disk, read, free_block);
  // if (ret)
  //   printf("Error in read: %d", ret);
  // else
  //   printf("Letto : %s\n", read);

  // ret = DiskDriver_getFreeBlock(disk, 0);
  // printf("Primo blocco libero : %d\n", ret);

  // ret = DiskDriver_freeBlock(disk, free_block);

  // memset(read, 0, BLOCK_SIZE);
  // ret = DiskDriver_readBlock(disk, read, free_block);
  // if (ret)
  //   printf("Error in read: %d\n", ret);
  // else
  //   printf("Letto : %s\n", read);

  // ret = DiskDriver_getFreeBlock(disk, 0);
  // printf("Primo blocco libero : %d\n", ret);

  // TEST SIMPLEFS.C

  SimpleFS fs;
  fs.disk = disk;
  ret = SimpleFS_format(&fs);

  DirectoryHandle* dirHandle = SimpleFS_init(&fs, fs.disk);

  FileHandle *file1 = SimpleFS_createFile(dirHandle, "PrimoFile");
  FileHandle *file2 = SimpleFS_createFile(dirHandle, "SecondoFile");
  SimpleFS_createFile(dirHandle, "TerzoFile");
  SimpleFS_createFile(dirHandle, "QuartoFile");
  SimpleFS_createFile(dirHandle, "QuintoFile");
  printf("Elementi nella dir: %d\n", dirHandle->fdb->num_entries);


  char * testText1 = "Test prima scrittura";
  int written = SimpleFS_write(file1, testText1, strlen(testText1));
  printf("Scritti n byte: %d\n", written);

  

  int fd1 = open("testfi.txt", O_CREAT | O_RDWR, 0777);

  struct stat statbuf;
  fstat(fd1, &statbuf);

  int size = 150000;

  printf("File grande %d\n", statbuf.st_size);

  char * fileDAta = malloc(size);
  // char fileDAta[1000015];

  ret = read(fd1, fileDAta, size);

  SimpleFS_write(file2,fileDAta, size);


}
