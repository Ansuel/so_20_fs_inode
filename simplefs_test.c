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

  printf("Bitmap Blocks: %d\n", disk->header->bitmap_blocks);
  printf("Bitmap Entires: %d\n", disk->header->bitmap_entries);
  printf("Reserved blocks: %d\n", disk->reserved_blocks);

  // TEST SIMPLEFS.C

  SimpleFS fs;
  fs.disk = disk;
  ret = SimpleFS_format(&fs);
  if (ret) return -1;

  DirectoryHandle* dirHandle = SimpleFS_init(&fs, fs.disk);

  FileHandle *file1 = SimpleFS_createFile(dirHandle, "PrimoFile");
  FileHandle *file2 = SimpleFS_createFile(dirHandle, "SecondoFile");
  FileHandle *file3 = SimpleFS_createFile(dirHandle, "TerzoFile");
  FileHandle *file4 = SimpleFS_createFile(dirHandle, "QuartoFile");
  FileHandle *file5 = SimpleFS_createFile(dirHandle, "QuintoFile");
  FileHandle *file6 = SimpleFS_createFile(dirHandle, "SestoFile");
  FileHandle *file7 = SimpleFS_createFile(dirHandle, "SettimoFile");
  FileHandle *file8 = SimpleFS_createFile(dirHandle, "OttavoFile");
  FileHandle *file9 = SimpleFS_createFile(dirHandle, "NonoFile");
  FileHandle *file10 = SimpleFS_createFile(dirHandle, "DecimoFile");
  int i;
  for(i = 0; i < 50000; i++){
    ret = SimpleFS_createFile(dirHandle, "File");
    if (!ret) return 0;
  }

  // FileHandle *ultimo1 = SimpleFS_createFile(dirHandle, "penUltimo");
  FileHandle *ultimo = SimpleFS_createFile(dirHandle, "Ultimo");

  // FileHandle* open_file1 = SimpleFS_openFile(dirHandle, "penUltimo");
  FileHandle* open_file = SimpleFS_openFile(dirHandle, "Ultimo");
  if(open_file){printf("Trovato\n");}
  // SimpleFS_write(open_file, "STO FUNZIONANDO", strlen("STO FUNZIONANDO"));

  // FileHandle* open_file2 = SimpleFS_openFile(dirHandle, "PrimoFile");

  printf("Elementi nella dir: %d\n", dirHandle->fdb->num_entries);


  // char * testText1 = "Test prima scrittura";
  // int written = SimpleFS_write(open_file, testText1, strlen(testText1));
  // printf("Scritti n byte: %d\n", written);
  
  // SimpleFS_createFile(dirHandle,"NUOVO FILE");
  // FileHandle* file = SimpleFS_openFile(dirHandle, "NUOVO FILE");
  // printf("Dopo mkDir - Elementi nella dir: %d\n", dirHandle->fdb->num_entries);
  // printf("BHO\n");
  // SimpleFS_write(file, "STO FUNZIONANDO", strlen("STO FUNZIONANDO"));
  // printf("Elementi nella dir: %d\n", dirHandle->fdb->num_entries);



}
