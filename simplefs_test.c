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

  printf("MaxFileInDir: %d\n", MaxFileInDir);

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

  DirectoryHandle* root = SimpleFS_init(&fs, fs.disk);

  // CREO 10 FILE NELLA ROOT
  FileHandle *file1 = SimpleFS_createFile(root, "PrimoFile");
  FileHandle *file2 = SimpleFS_createFile(root, "SecondoFile");
  FileHandle *file3 = SimpleFS_createFile(root, "TerzoFile");
  FileHandle *file4 = SimpleFS_createFile(root, "QuartoFile");
  FileHandle *file5 = SimpleFS_createFile(root, "QuintoFile");
  FileHandle *file6 = SimpleFS_createFile(root, "SestoFile");
  FileHandle *file7 = SimpleFS_createFile(root, "SettimoFile");
  FileHandle *file8 = SimpleFS_createFile(root, "OttavoFile");
  FileHandle *file9 = SimpleFS_createFile(root, "NonoFile");
  FileHandle *file10 = SimpleFS_createFile(root, "DecimoFile");
 int i;

  printf("Elementi nella dir: %d\n", root->fdb->num_entries);

  // LEGGO QUANTI FILE E I NOMI DEI FILE DELLA ROOT
  char** names = calloc(root->fdb->num_entries,sizeof(char*));
  SimpleFS_readDir(names, root);
  printf("Elementi nella dir: %d\n", root->fdb->num_entries);

  for(i = 0; i < root->fdb->num_entries; i++){
    if(names[i]) printf("file numero: %d\t nome: %s\n", i, names[i]);
  }

  // CREO UNA NUOVA CARTELLA E CAMBIO IL DIRHANDLE
  SimpleFS_mkDir(root,"NUOVA CARTELLA");
  SimpleFS_changeDir(root, "NUOVA CARTELLA");

  // CREO 4000 FILE NELLA NUOVA CARTELLA
  for(i = 0; i < 10; i++){
    char str[128] = {0};
    sprintf(str, "File%d",i);
    ret = SimpleFS_createFile(root, str);
    if (!ret) return 0;
  }

  // LEGGO QUANTI FILE E I NOMI DEI FILE NELLA CARTELLA
  char** namess = calloc(root->fdb->num_entries,sizeof(char*));
  SimpleFS_readDir(namess, root);
  printf("Elementi nella dir: %d\n", root->fdb->num_entries);

  for(i = 0; i < root->fdb->num_entries; i++){
    if(namess[i]) printf("file numero: %d\t nome: %s\n", i, namess[i]);
  }

  // RITORNO NELLA ROOT E CREO 50 FILE
  SimpleFS_changeDir(root, "..");
  for(i = 0; i < 10; i++){
    char str[128] = {0};
    sprintf(str, "File%d",i);
    ret = SimpleFS_createFile(root, str);
    if (!ret) return 0;
  }
  printf("Elementi nella dir: %d\n", root->fdb->num_entries);

  // LEGGO QUANTI E QULI FILE CI SONO NELLA ROOT
  char** namesss = calloc(root->fdb->num_entries,sizeof(char*));
  SimpleFS_readDir(namesss, root);
  printf("Elementi nella dir: %d\n", root->fdb->num_entries);

  for(i = 0; i < root->fdb->num_entries; i++){
    if(namesss[i]) printf("file numero: %d\t nome: %s\n", i, namesss[i]);
  }

  SimpleFS_remove(&fs, "PrimoFile");
  char** namessss = calloc(root->fdb->num_entries,sizeof(char*));
  SimpleFS_readDir(namesss, root);
  for(i = 0; i < root->fdb->num_entries; i++){
    if(namessss[i]) printf("file numero: %d\t nome: %s\n", i, namessss[i]);
  }
}
