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
  // for(i = 0; i < 5000; i++){
  //   ret = SimpleFS_createFile(root, "File");
  //   if (!ret) return 0;
  // }

  // FileHandle *ultimo = SimpleFS_createFile(root, "Ultimo");
  // FileHandle* open_file = SimpleFS_openFile(root, "Ultimo");
  // if(open_file){printf("Trovato\n");}

  // printf("Elementi nella dir: %d\n", root->fdb->num_entries);

  ret = SimpleFS_mkDir(root, "NUOVA CARTELLA");
  if(!ret){ printf("cartella creata\n");}

  printf("Elementi nella dir: %d\n", root->fdb->num_entries);

  printf("La cahin è in %s cartella superiore\n", fs.fdb_chain->current->fcb.name);

  ret = SimpleFS_changeDir(root, "NUOVA CARTELLA");
  if(!ret){
    printf("Sono in: %s\n", root->fdb->fcb.name);
    printf("La mia top_level è: %s\n", root->directory->fcb.name);
  }

  printf("La cahin è in %s cartella superiore %s\n", fs.fdb_chain->current->fcb.name, fs.fdb_chain->prev ? fs.fdb_chain->prev->current->fcb.name : "NULL");


  ret = SimpleFS_mkDir(root, "NUOVA CARTELLA2");
  if(!ret){ printf("cartella creata\n");}
  printf("Elementi nella dir: %d\n", root->fdb->num_entries);
  

  FileHandle *file16 = SimpleFS_createFile(root, "DecimoFile");

  printf("Elementi nella dir: %d dir %s\n", root->fdb->num_entries, root->fdb->fcb.name);

  ret = SimpleFS_changeDir(root, "NUOVA CARTELLA2");
  if(!ret){
    printf("Sono in: %s\n", root->fdb->fcb.name);
    printf("La mia top_level è: %s\n", root->directory->fcb.name);
    }

  printf("La cahin è in %s cartella superiore %s\n", fs.fdb_chain->current->fcb.name, fs.fdb_chain->prev ? fs.fdb_chain->prev->current->fcb.name : "NULL");


  // for(i = 0; i < 4000; i++){
  //   char str[128] = {0};
  //   // sprintf(str, "File%d",i);
  //   ret = SimpleFS_createFile(root, str);
  //   if (!ret) return 0;
  // }

  // printf("Elementi nella dir: %d\n", root->fdb->num_entries);

  // for(i = 2000; i < 2020; i++){
  //   char str[128] = {0};
  //   sprintf(str, "File%d",i);
  //   ret = SimpleFS_remove(&fs, str);
  //   if (ret) return 0;
  // }

  // printf("Elementi nella dir: %d dir %s\n", root->directory->num_entries, root->directory->fcb.name);
  // printf("Elementi nella dir: %d dir %s\n", root->fdb->num_entries, root->fdb->fcb.name);
  // printf("Elementi nella dir: %d dir %s\n", fs.fdb_top_level_dir->num_entries, fs.fdb_top_level_dir->fcb.name);

}
