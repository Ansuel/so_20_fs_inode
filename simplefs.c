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

  DiskDriver *disk = d->sfs->disk;
  int destBlock = DiskDriver_getFreeBlock(disk, 0);
  printf("destBlock: %d\n", destBlock);
  FirstFileBlock *ffb = calloc(1, sizeof(FirstFileBlock));
  ffb->fcb.directory_block = d->pos_in_block;
  ffb->fcb.block_in_disk = destBlock;
  memcpy(ffb->fcb.name, filename, strlen(filename));
  ffb->fcb.size_in_bytes = 0;
  ffb->fcb.size_in_blocks = 0;
  ffb->fcb.is_dir = 0;

  DiskDriver_writeBlock(disk, ffb, destBlock);
  // TODO: quanti elementi abbiamo nel FirstDirectoryBlock?
  FirstDirectoryBlock *destDir = d->fdb;
  destDir->file_blocks[destDir->num_entries] = destBlock;
  destDir->num_entries++;
  DiskDriver_writeBlock(disk, destDir, d->pos_in_block);

  FileHandle *file = calloc(1, sizeof(FileHandle));

  file->sfs = d->sfs;
  file->ffb = ffb;
  file->directory = destDir;
  file->pos_in_file = 0;

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

      DiskDriver_writeBlock(disk, data + MaxDataInFDB + (i * BLOCK_SIZE),
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

    DiskDriver_writeBlock(disk, data + MaxDataInFDB + (i * BLOCK_SIZE),
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

      ret = DiskDriver_writeBlock(disk, data + MaxDataInFDB + (i * BLOCK_SIZE),
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
  for (i = 0; i < 31 && i < entries; i++, entries--) {
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

  free(file);
  free(name);

  return ret;
}