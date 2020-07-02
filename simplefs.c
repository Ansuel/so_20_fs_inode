#include "simplefs.h"
#include "utils.h"
#include <stdlib.h>
#include <string.h>

// creates the inital structures, the top level directory
// has name "/" and its control block is in the first position
// it also clears the bitmap of occupied blocks on the disk
// the current_directory_block is cached in the SimpleFS struct
// and set to the top level directory
int SimpleFS_format(SimpleFS *fs) {

  FirstDirectoryBlock root = {0};
  char *tmp = "/";
  memcpy(root.fcb.name, tmp, strlen(tmp));
  root.fcb.is_dir = 1;
  root.inode_block[31] = -1;
  int ret;
  ret = DiskDriver_writeBlock(fs->disk, &root, 0);
  if (ret)
    return handle_error("Error in format: ", ret);
  return 0;
}

// initializes a file system on an already made disk
// returns a handle to the top level directory stored in the first block
DirectoryHandle *SimpleFS_init(SimpleFS *fs, DiskDriver *disk) {

  FirstDirectoryBlock *firstDir = calloc(1, sizeof(FirstDirectoryBlock));
  DiskDriver_readBlock(fs->disk, firstDir, 0);

  // Cache top level dir in fs
  fs->fdb_current_dir = firstDir;
  fs->fdb_top_level_dir = firstDir;

  DirectoryHandle *dir = (DirectoryHandle *)malloc(sizeof(DirectoryHandle));
  dir->sfs = fs;
  dir->fdb = firstDir;
  dir->directory = NULL;
  dir->pos_in_block = 0;
  dir->pos_in_dir = 0;

  return dir;
}

// creates an empty file in the directory d
// returns null on error (file existing, no free blocks)
// an empty file consists only of a block of type FirstBlock
FileHandle *SimpleFS_createFile(DirectoryHandle *d, const char *filename) {

  FileHandle *file = calloc(1, sizeof(FileHandle));


  DiskDriver *disk = d->sfs->disk;
  int destBlock = DiskDriver_getFreeBlock(disk, 0);
  FirstFileBlock *ffb = calloc(1, sizeof(FirstFileBlock));
  ffb->fcb.directory_block = d->pos_in_block;
  ffb->fcb.block_in_disk = destBlock;
  memcpy(ffb->fcb.name, filename, strlen(filename));
  ffb->fcb.size_in_bytes = 0;
  ffb->fcb.size_in_blocks = 0;
  ffb->fcb.is_dir = 0;

  DiskDriver_writeBlock(disk, ffb, destBlock);
  FirstDirectoryBlock *destDir = d->fdb;
  // CASO 1: GLI ELEMENTI NELLA DIR SONO MENO DI MaxFileInDir
  // if(d->fdb->num_entries <= MaxFileInDir){
    int i;
    for(i = 0; i < MaxFileInDir; i++){
      if(destDir->file_blocks[i] == 0){
        destDir->file_blocks[i] = destBlock;
        printf("ESCO DAL CASO 1\n");
        goto exit;
      }
    }

  // CASO 2: GLI ELEMENTI NELLA DIR SONO MAGGIORI DI MaxFileInDir 
  //         E QUINDI BISOGNA CONTROLLARE SE C'è UN DIRECTORY BLOCK LIBERO
  //         PER INSERIRE IL NUOVO FILE IN QUEL BLOCCO
  // CHECK DI TUTTI I BLOCCHI PER TROVARE UNA POSIZIONE LIBERA NEL DIRECTORY_BLOCK
    DirectoryBlock tmp_block;
    for(i = 0; i < 31; i++){
      if(d->fdb->inode_block[i] != 0) {
      DiskDriver_readBlock(disk, &tmp_block, d->fdb->inode_block[i]);
      int j;
      for(j = 0; j < BLOCK_SIZE/sizeof(int); j++){
        // E' STATO TROVATO UNA POSIZIONE LIBERA NEL DIRECTORY BLOCK
        if(tmp_block.file_blocks[j] == 0) {
          tmp_block.file_blocks[j] = destBlock;
          DiskDriver_writeBlock(disk, &tmp_block, d->fdb->inode_block[i]);
          printf("ESCO DAL CASO 2\n");
          goto exit;
        }
      }
      }
    }

    // SCANSIONE DEI SUCCESSIVI INODEBLOCK, PER TROVARE L'INDIRCE DI UN DIRECTORY_BLOCK
    // IN CUI VI E' UNA POSIZIONE LIBERA
    int next_inode_block = d->fdb->inode_block[31];
    int current_inode_block = -1;
    InodeBlock tmp_inode_block;
            printf("next_inode_block: %d\n", next_inode_block);

    // CASO 2.1: SE IL PRIMO INODEBLOCK DOPO IL FIRSTDIRECTORYBLOCK NON C'è ALLOCATO,
    //           SALTA DIRETTAMENTE ALLA ALLOCAZIONE
    if(next_inode_block != -1){      
      while(next_inode_block != -1) {
        current_inode_block = next_inode_block;
        DiskDriver_readBlock(disk, &tmp_inode_block, next_inode_block);
        for(i= 0; i < BLOCK_SIZE/sizeof(int)-1; i++){
          DiskDriver_readBlock(disk, &tmp_block, d->fdb->inode_block[i]);
          if(d->fdb->inode_block[i] != 0) {
          int j;
          for(j = 0; j < BLOCK_SIZE/sizeof(int); j++){
            // E' STATO TROVATO UNA POSIZIONE LIBERA NEL DIRECTORY BLOCK
            if(tmp_block.file_blocks[j] == 0) {
              tmp_block.file_blocks[j] = destBlock;
              DiskDriver_writeBlock(disk, &tmp_block, d->fdb->inode_block[i]);
              printf("ESCO DAL CASO 2.1\n");
              goto exit;
            }
          }
          }
        }
        next_inode_block = tmp_inode_block.inodeList[i];
      }
      // CASO 3: NON CI SONO POSIZIONI LIBERE NEI DIRECTORY_BLOCK ESISTENTI
      //         DI CONSEGUENZA BISOGNA ALLOCARE UN NUOVO DIRECTORY_BLOCK(MA E' PRESENTE
      //         ALMENO UNA POSIZIONE LIBERA IN UN INODE BLOCK CHE CONTERRA' IL SUO INDICE)
      for(i= 0; i < BLOCK_SIZE/sizeof(int)-1; i++){
        // 3.1: ALLOCATO UN NUOVO DIRECTORY BLOCK, E IL SUO INDICE E' STATO MESSO IN UN INODE BLOCK
        //      GIA' ESISTENTE
          if(tmp_inode_block.inodeList[i]==0) {
            DirectoryBlock new_dir_block;
            new_dir_block.file_blocks[0] = destBlock;
            int num_block = DiskDriver_getFreeBlock(disk,0);
            DiskDriver_writeBlock(disk,&new_dir_block,num_block);
            tmp_inode_block.inodeList[i] = num_block;
            DiskDriver_writeBlock(disk,&tmp_inode_block,current_inode_block);
            printf("ESCO DAL CASO 3.1\n");

            goto exit;
            }
      }
    } 
    // 3.2: NON CI SONO POSIZIONI LIBERE DENTRO UN INODE BLOCK, QUINDI, DEVO ALLOCARE
    //      UN NUOVO INODE BLOCK PER INSERIRE L'INDICE DEL NUOVO DIRECTORY BLOCK

      InodeBlock new_inode_block;
      DirectoryBlock new_dir_block;
      new_dir_block.file_blocks[0] = destBlock;
      int num_block = DiskDriver_getFreeBlock(disk,0);
      DiskDriver_writeBlock(disk,&new_dir_block,num_block);
      new_inode_block.inodeList[0] = num_block;
      new_inode_block.inodeList[MaxInodeInBlock-1] = -1;
      num_block = DiskDriver_getFreeBlock(disk,0);
      DiskDriver_writeBlock(disk,&new_inode_block,num_block);
      printf("ESCO DAL CASO 3.2\n");
      // COLLEGO UN INODEBLOCK CON UN ALTRO INODEBLOCK
      if(current_inode_block != -1) {
        tmp_inode_block.inodeList[31] = num_block;
        DiskDriver_writeBlock(disk,&tmp_inode_block,current_inode_block);
      } else {
      // ALLOCO IL PRIMO INODEBLOCK COLLEGATO AL FIRST DIRECTORY BLOCK
        printf("sto allocando un inodeblock: %d\n", num_block);
        destDir->inode_block[31] = num_block;
      }
  
exit:


  file->sfs = d->sfs;
  file->ffb = ffb;
  file->directory = destDir;
  file->pos_in_file = 0;
  destDir->num_entries++; 
  DiskDriver_writeBlock(disk, destDir, d->pos_in_block);

  return file;
}

// writes in the file, at current position for size bytes stored in data
// overwriting and allocating new space if necessary
// returns the number of bytes written
int SimpleFS_write(FileHandle *f, void *data, int size) {

  int ret;
  FirstFileBlock *ffb = f->ffb;
  DiskDriver *disk = f->sfs->disk;
  // Se è maggiore del massimo numero di dati nel firstFileBlock devi usare gli
  // inode Altrimenti direttamente nel FFB
  ffb->inode_block[31] = -1;

  // Dati entrano nel FFB
  if (size <= MaxDataInFFB) {

    memcpy(ffb->data, data, size);
    ffb->fcb.size_in_bytes = size;
    ffb->fcb.size_in_blocks = 1;

    DiskDriver_writeBlock(disk, ffb, ffb->fcb.block_in_disk);

    return size;
  }

  int remainingData = size - MaxDataInFFB;

  int num_blocks = (remainingData + BLOCK_SIZE - 1) / BLOCK_SIZE;

  memcpy(ffb->data, data, MaxDataInFFB);
  ffb->fcb.size_in_bytes = size;
  ffb->fcb.size_in_blocks = num_blocks + 1;

  int i;

  // I blocchi da scrivere necessari entrano in un FFB
  if (num_blocks <= 31) {
    for (i = 0; i < num_blocks; i++) {
      int freeBlock = DiskDriver_getFreeBlock(disk, 0);

      DiskDriver_writeBlock(disk, data + MaxFileInDir + (i * BLOCK_SIZE),
                            freeBlock);
      ffb->inode_block[i] = freeBlock;
    }

    DiskDriver_writeBlock(disk, ffb, ffb->fcb.block_in_disk);

    return size;
  }

  // Caso 3 i blocchi da scrivere hanno bisogno di più di 31 inode
  int InodeBlockToAlloc =
      (((num_blocks - 31) + (BLOCK_SIZE / sizeof(int)) - 1) /
       (BLOCK_SIZE / sizeof(int)));

  for (i = 0; i < 30; i++) {
    int freeBlock = DiskDriver_getFreeBlock(disk, 0);

    DiskDriver_writeBlock(disk, data + MaxFileInDir + (i * BLOCK_SIZE),
                          freeBlock);
    ffb->inode_block[i] = freeBlock;
  }

  int inodeBlock;
  InodeBlock blockIndex = {0}, lastblockIndex = {0};

  for (inodeBlock = 0; inodeBlock < InodeBlockToAlloc; inodeBlock++) {
    int freeInodeBlock = DiskDriver_getFreeBlock(disk, 0);

    // Set the last inode block to the conseguent inodeBlock
    if (ffb->inode_block[31] == -1)
      ffb->inode_block[31] = freeInodeBlock;

    // Connect the last inode block to the new one
    if (lastblockIndex.inodeList[MaxInodeInBlock - 1] == -1)
      lastblockIndex.inodeList[MaxInodeInBlock - 1] = freeInodeBlock;

    for (i = 0; i < MaxInodeInBlock - 1 && num_blocks >= 0; i++, num_blocks--) {
      int freeBlock = DiskDriver_getFreeBlock(disk, 0);

      ret = DiskDriver_writeBlock(disk, data + MaxFileInDir + (i * BLOCK_SIZE),
                                  freeBlock);
      if (ret)
        return ret;
      blockIndex.inodeList[i] = freeBlock;
    }

    blockIndex.inodeList[MaxInodeInBlock - 1] = -1;

    DiskDriver_writeBlock(disk, &blockIndex, freeInodeBlock);

    lastblockIndex = blockIndex;
  }

  DiskDriver_writeBlock(disk, ffb, ffb->fcb.block_in_disk);

  return size;
}

int DeleteStoredFile(DiskDriver *disk, FirstFileBlock *file) {
  int blocksToDel = file->fcb.size_in_blocks;
  int inodeIndex;

  // File is storend in more blocks
  if (blocksToDel > 1) {
    // Clear the blocks stored in inode block array
    for (inodeIndex = 0; inodeIndex < 31 && blocksToDel > 0;
         inodeIndex++, blocksToDel--) {
      DiskDriver_freeBlock(disk, file->inode_block[inodeIndex]);
    }

    // Check if we have more than 30 inode block
    int nextInodeBlock = file->inode_block[31];

    // Loop to clear all blocks declared in inod blocks
    while (nextInodeBlock != -1) {
      InodeBlock indexBlock;
      DiskDriver_readBlock(disk, &indexBlock, nextInodeBlock);
      for (inodeIndex = 0; inodeIndex < MaxInodeInBlock - 1 && blocksToDel > 0;
           inodeIndex++, blocksToDel--) {

        DiskDriver_freeBlock(disk, indexBlock.inodeList[inodeIndex]);
      }

      // Clear the inode block
      DiskDriver_freeBlock(disk, nextInodeBlock);
      nextInodeBlock = indexBlock.inodeList[MaxInodeInBlock - 1];
    }
  }

  // Finally clear th ffb
  DiskDriver_freeBlock(disk, file->fcb.block_in_disk);

  return 0;
}

int DeleteStoredDir(DiskDriver *disk, FirstDirectoryBlock *dir) {
  FirstFileBlock* file_in_dir = malloc(sizeof(FirstFileBlock));

  int entryToDel = dir->num_entries;
  int inodeIndex;

  for (inodeIndex = 0; inodeIndex < 31 && entryToDel > 0;
       inodeIndex++, entryToDel--) {
    DiskDriver_readBlock(disk, file_in_dir, dir->inode_block[inodeIndex]);
    if (file_in_dir->fcb.is_dir) {
      DeleteStoredDir(disk, (FirstDirectoryBlock *)file_in_dir);
    } else {
      DeleteStoredFile(disk, file_in_dir);
    }
  }

  // Check if we have more than 30 inode block
  int nextInodeBlock = dir->inode_block[31];

  // Loop to clear all blocks declared in inod blocks
  while (nextInodeBlock != -1) {
    InodeBlock indexBlock;
    DiskDriver_readBlock(disk, &indexBlock, nextInodeBlock);
    for (inodeIndex = 0; inodeIndex < MaxInodeInBlock - 1 && entryToDel > 0;
         inodeIndex++, entryToDel--) {

      DiskDriver_readBlock(disk, &file_in_dir,
                           indexBlock.inodeList[inodeIndex]);
      if (file_in_dir->fcb.is_dir) {
        DeleteStoredDir(disk, (FirstDirectoryBlock *)file_in_dir);
      } else {
        DeleteStoredFile(disk, file_in_dir);
      }
    }

    // Clear the inode block
    DiskDriver_freeBlock(disk, nextInodeBlock);
    nextInodeBlock = indexBlock.inodeList[MaxInodeInBlock - 1];
  }

  DiskDriver_freeBlock(disk, dir->fcb.block_in_disk);
  free(file_in_dir);

  return 0;
}

int SimpleFS_remove(SimpleFS *fs, char *filename) {

  // By default set the file as not found
  int ret = -1;

  if (!fs->fdb_current_dir)
    return -1;

  // int len = strlen(fs->fdb_current_dir.name);

  // // Can't remove root directly? Can this happen?
  // if (fs->fdb_current_dir == fs->fdb_top_level_dir)
  //   return -1;

  DiskDriver *disk = fs->disk;
  FirstDirectoryBlock *dir = fs->fdb_current_dir;

  int entries = dir->num_entries;
  int i, block;

  FirstFileBlock *file = malloc(sizeof(FirstFileBlock));

  // I hate strcmp
  char *name = calloc(MaxFileLen, sizeof(char));
  memcpy(name, filename, strlen(filename));

  // Search in the first file_blocks
  for (i = 0; i < 31 && entries > 0; i++, entries--) {
    block = dir->file_blocks[i];

    // We load the block alyeas as a file
    // The fdb has the fcb as first struct elemen so this is not a problem
    // This is casted to a fdb if is_dir is true in fcb
    DiskDriver_readBlock(disk, file, block);
    FileControlBlock fcb = file->fcb;

    // Actually search the file
    if (!memcmp(name, fcb.name, sizeof(char) * MaxFileLen)) {
      if (fcb.is_dir) {
        ret = DeleteStoredDir(disk, (FirstDirectoryBlock *)file);
      } else {
        ret = DeleteStoredFile(disk, file);
      }
      // File found skip searching in remaining entires
      goto exit;
    }
  }

  // Check if we have more than 30 inode block
  int nextInodeBlock = dir->inode_block[31], inodeIndex;

  // Loop to clear all blocks declared in inod blocks
  while (nextInodeBlock != -1) {
    InodeBlock indexBlock;
    DiskDriver_readBlock(disk, &indexBlock, nextInodeBlock);
    for (inodeIndex = 0; inodeIndex < MaxInodeInBlock - 1 && entries > 0;
         inodeIndex++, entries--) {

      block = dir->file_blocks[i];

      DiskDriver_readBlock(disk, file, block);
      FileControlBlock fcb = file->fcb;

      // Actually search the file
      if (!memcmp(name, fcb.name, sizeof(char) * MaxFileLen)) {
        if (fcb.is_dir) {
          ret = DeleteStoredDir(disk, (FirstDirectoryBlock *)file);
        } else {
          ret = DeleteStoredFile(disk, file);
        }
        // File found skip searching in remaining entires
        goto exit;
      }
    }

    nextInodeBlock = indexBlock.inodeList[MaxInodeInBlock - 1];
  }

exit:
  dir->num_entries--;
  free(file);
  free(name);

  return ret;
}

// creates a new directory in the current one (stored in fs->current_directory_block)
// 0 on success
// -1 on error
int SimpleFS_mkDir(DirectoryHandle* d, char* dirname){

int new_free_block = DiskDriver_getFreeBlock(d->sfs->disk,0);
FirstDirectoryBlock* current_dir_block = d->sfs->fdb_current_dir;

DirectoryHandle* new_dir = calloc(1,sizeof(DirectoryHandle));
new_dir->sfs = d->sfs;
new_dir->directory = d->fdb;
new_dir->pos_in_dir = 0;
new_dir->pos_in_block = new_free_block;

FirstDirectoryBlock* new_fdb = calloc(1, sizeof(FirstDirectoryBlock));
new_fdb->fcb.directory_block = d->fdb->fcb.block_in_disk;
new_fdb->fcb.block_in_disk = new_dir->pos_in_block;
char* tmp = calloc(128,sizeof(char));
memcpy(tmp,dirname,strlen(dirname));
memcpy(new_fdb->fcb.name,tmp,strlen(tmp));
free(tmp);
new_fdb->fcb.size_in_bytes = 0;
new_fdb->fcb.size_in_blocks = 1;
new_fdb->fcb.is_dir = 1;
new_fdb->num_entries = 0;

DiskDriver_writeBlock(d->sfs->disk,new_fdb,new_free_block);
current_dir_block->file_blocks[current_dir_block->num_entries] = new_free_block;
current_dir_block->num_entries++;
// TODO: fare caso in cui num_entries > 31
return 0;


}

// opens a file in the  directory d. The file should be exisiting
FileHandle* SimpleFS_openFile(DirectoryHandle* d, const char* filename){

  FileHandle* openFile = calloc(1,sizeof(FileHandle));

  FirstFileBlock *file = malloc(sizeof(FirstFileBlock));

  char *name = calloc(MaxFileLen, sizeof(char));
  memcpy(name, filename, strlen(filename));

  FirstDirectoryBlock* dir = d->fdb;
  DiskDriver* disk = d->sfs->disk;
  int entries = dir->num_entries;
  int i, block, ret;
  // Search in the first file_blocks
    printf("entro nel for\n");

  for (i = 0; i < 31 && entries > 0; i++, entries--) {
    block = dir->file_blocks[i];

    // We load the block alyeas as a file
    // The fdb has the fcb as first struct elemen so this is not a problem
    // This is casted to a fdb if is_dir is true in fcb
    DiskDriver_readBlock(disk, file, block);
    FileControlBlock fcb = file->fcb;
    printf("name: %s\n fcb.name: %s\n", name, fcb.name );
    // Actually search the file
    if (!memcmp(name, fcb.name, sizeof(char) * MaxFileLen)) {
      if (fcb.is_dir) {
        return NULL;
      } else {
        openFile->sfs = d->sfs;
        openFile->ffb = file;
        openFile->directory = dir;
        openFile->pos_in_file = 0;
        return openFile;
      }
    }
  }
  printf("dopo il for e quindi > 30 inode blcok\n");
  // Check if we have more than 30 inode block
  int nextInodeBlock = dir->inode_block[31], inodeIndex;
  printf("next_inode_block: %d\n", nextInodeBlock);
  // Loop to clear all blocks declared in inod blocks
  while (nextInodeBlock != -1) {
    InodeBlock indexBlock;
    DiskDriver_readBlock(disk, &indexBlock, nextInodeBlock);
    for (inodeIndex = 0; inodeIndex < MaxInodeInBlock - 1; inodeIndex++) {
      if(indexBlock.inodeList[inodeIndex] == 0) { return NULL;}
      printf("indexBlock.inodeList[inodeIndex] %d\n", indexBlock.inodeList[inodeIndex]);
      DirectoryBlock tmp_dir;
      DiskDriver_readBlock(disk,&tmp_dir,indexBlock.inodeList[inodeIndex]);
      int i;
      for(i = 0; i < MaxFileInDir && entries > 0; i++, entries--){
        block = tmp_dir.file_blocks[i];

        DiskDriver_readBlock(disk, file, block);
        FileControlBlock fcb = file->fcb;

        printf("file numero: %d %s\n",entries,fcb.name);
        // Actually search the file
        if (!memcmp(name, fcb.name, sizeof(char) * MaxFileLen)) {
          if (fcb.is_dir) {
            return NULL;
          } else {
            openFile->sfs = d->sfs;
            openFile->ffb = file;
            openFile->directory = dir;
            openFile->pos_in_file = 0;
            return openFile;
          }
        }
      }
    }

    nextInodeBlock = indexBlock.inodeList[MaxInodeInBlock - 1];
    printf("nextInodeBlock: %d\n", nextInodeBlock );

  } 
  return NULL;
}
