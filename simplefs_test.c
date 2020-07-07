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
  // printf("FileControlBlock size %ld\n", sizeof(FileControlBlock));
  // printf("FirstFileBlock size %ld\n", sizeof(FirstFileBlock));
  // printf("InodeBlock size %ld\n", sizeof(InodeBlock));
  // printf("FileBlock size %ld\n", sizeof(FileBlock));
  // printf("FirstDirectoryBlock size %ld\n", sizeof(FirstDirectoryBlock));
  // printf("DirectoryBlock size %ld\n", sizeof(DirectoryBlock));
  // TEST DISK_DRIVER.C

  printf("MaxFileInDir: %ld\n", MaxFileInDir);

  int ret;
  char *readed = calloc(BLOCK_SIZE, sizeof(char));
  memset(readed, 0, BLOCK_SIZE);

  DiskDriver *disk = malloc(sizeof(DiskDriver));

  DiskDriver_init(disk, DISK_NAME, 100000);

  printf("Bitmap Blocks: %d\n", disk->header->bitmap_blocks);
  printf("Bitmap Entires: %d\n", disk->header->bitmap_entries);
  printf("Reserved blocks: %d\n", disk->reserved_blocks);

  printf("\n\n");

  // TEST SIMPLEFS.C

  SimpleFS fs;
  fs.disk = disk;
  ret = SimpleFS_format(&fs);
  if (ret) return -1;

  DirectoryHandle* root = SimpleFS_init(&fs, fs.disk);

  /* Casi limite 
   * 3817 3818 3819 3820 3821 3822  Caso occupa tutto il data FFB e alloca un blocco
   * 130793 130794 130795 130796 130797 130798  Caso occupa tutti gli inode e alloca un externalInode
   * 4321001 4321002 4321003 4321004 4321005 4321006 Caso occupa un intero external inode e ne alloca un altro
   * 4328917 4328918 4328919 4328920 4328921 4328922 Caso a caso :D
   * limite di tutto???? limite del disco
   */

  FileHandle *f = SimpleFS_createFile(root, "File");

  int len = 10000000;
  // int i;
  char * text = calloc(len, sizeof(char));

  int seekOffset = 4328919;

  int textLen = 200;

  memset(text,'A',textLen);
  text[len-1] = '\0';

  char * tmp_buf = calloc(len,sizeof(char));
  SimpleFS_write(f, tmp_buf, len);

  printf("\n");

  SimpleFS_seek(f, seekOffset);

  char * buf = calloc(strlen(text)+1, sizeof(char));

  ret = SimpleFS_write(f, text, strlen(text));
  printf("Scritti %d bytes len: %ld\n", ret, strlen(text));

  SimpleFS_seek(f, seekOffset);

  ret = SimpleFS_read(f, buf, strlen(text));
  printf("Letti %d bytes\n", ret);

  printf("letto: %s\n", buf);

  printf("\n");

  SimpleFS_seek(f, seekOffset+18);

  char * tmp_text = "Cassa";

  ret = SimpleFS_write(f, tmp_text, strlen(tmp_text));
  printf("Scritti %d bytes\n", ret);

  SimpleFS_seek(f, seekOffset);

  memset(buf,0,textLen);
  ret = SimpleFS_read(f, buf, 18);
  printf("Letti %d bytes\n", ret);

  printf("letto: %s\n", buf);
  printf("\n");


  // SimpleFS_seek(f, seekOffset+18);

  memset(buf,0,strlen(text));

  ret = SimpleFS_read(f, buf, strlen(text)-18);
  printf("Letti %d bytes\n", ret);

  printf("letto: %s\n", buf);
  printf("\n");

  SimpleFS_seek(f, seekOffset+textLen);
   ret = SimpleFS_write(f, tmp_text, strlen(tmp_text));

   memset(buf,0,strlen(text));

   SimpleFS_seek(f, seekOffset);

  ret = SimpleFS_read(f, buf, textLen+strlen(tmp_text));
  printf("Letti %d bytes\n", ret);
  printf("letto: %s\n", buf);
  printf("\n");



}
