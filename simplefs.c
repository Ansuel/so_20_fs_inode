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

  int num_blocks = (remainingData + BLOCK_SIZE -1) / BLOCK_SIZE ;

    memcpy(ffb->data, data, MaxDataInFFB);
    ffb->fcb.size_in_bytes = size;
    ffb->fcb.size_in_blocks = num_blocks + 1;

    int i;

// I blocchi da scrivere necessari entrano in un FFB
    if (num_blocks <= 31) {
        for (i=0;i<num_blocks;i++) {
            int freeBlock = DiskDriver_getFreeBlock(disk, 0);

            DiskDriver_writeBlock(disk, data + MaxDataInFDB + (i*BLOCK_SIZE), freeBlock);
            ffb->inode_block[i] = freeBlock;
        }

        DiskDriver_writeBlock(disk, ffb, ffb->fcb.block_in_disk);

        return size;
    }

// Caso 3 i blocchi da scrivere hanno bisogno di più di 31 inode
    int InodeBlockToAlloc = (((num_blocks - 31) + (BLOCK_SIZE/sizeof(int)) -1 ) / (BLOCK_SIZE/sizeof(int)));
    printf("Num bloks %d MaxInodeInBlock %ld\n",num_blocks, MaxInodeInBlock);
    printf("Blocs to alloc %d  %ld\n", InodeBlockToAlloc,  ( (BLOCK_SIZE/sizeof(int))  / MaxInodeInBlock));

    for (i=0;i<30;i++) {
        int freeBlock = DiskDriver_getFreeBlock(disk, 0);

        DiskDriver_writeBlock(disk, data + MaxDataInFDB + (i*BLOCK_SIZE), freeBlock);
        ffb->inode_block[i] = freeBlock;
    }

    int inodeBlock;
    InodeBlock blockIndex = {0}, lastblockIndex = {0};

    printf("Ciao :D");
    
    for (inodeBlock=0;inodeBlock<InodeBlockToAlloc;inodeBlock++) {
        int freeInodeBlock = DiskDriver_getFreeBlock(disk, 0);

        printf("Alloco %d inode block\n",freeInodeBlock);

        // Set the last inode block to the conseguent inodeBlock
        if (ffb->inode_block[31] == -1)
            ffb->inode_block[31] = freeInodeBlock;

        if (lastblockIndex.inodeList[MaxInodeInBlock-1] == -1)
            lastblockIndex.inodeList[MaxInodeInBlock-1] = freeInodeBlock;

        for (i=0;i<MaxInodeInBlock && num_blocks >= 0;i++, num_blocks--) {
            int freeBlock = DiskDriver_getFreeBlock(disk, 0);
            printf("Alloco %d file block\n",freeBlock);

            ret = DiskDriver_writeBlock(disk, data + MaxDataInFDB + (i*BLOCK_SIZE), freeBlock);
            if (ret) return handle_error("Errore scrittura", ret);
            blockIndex.inodeList[i] = freeBlock;
        }

        blockIndex.inodeList[MaxInodeInBlock-1] = -1;
        
        DiskDriver_writeBlock(disk, &blockIndex, freeInodeBlock);

        lastblockIndex = blockIndex;

    }

    DiskDriver_writeBlock(disk, ffb, ffb->fcb.block_in_disk);

    return size;

}
