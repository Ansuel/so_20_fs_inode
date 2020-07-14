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
  root.inode_block[MaxInodeInFFB - 1] = -1;
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
  char *root_name = "/";

  if (memcmp(firstDir->fcb.name, root_name, strlen(root_name)))
    goto err;

  // Cache top level dir in fs
  fs->fdb_current_dir = firstDir;
  fs->fdb_top_level_dir = firstDir;

  DirectoryHandle *dir = (DirectoryHandle *)malloc(sizeof(DirectoryHandle));
  dir->sfs = fs;
  dir->fdb = firstDir;
  dir->directory = NULL;
  dir->pos_in_block = 0;
  dir->pos_in_dir = 0;

  FdbChain *first_chain = calloc(1, sizeof(FdbChain));
  first_chain->current = firstDir;

  fs->fdb_chain = first_chain;

  return dir;

err:
  free(firstDir);
  return NULL;
}

int SimpleFS_unload(SimpleFS *fs, DirectoryHandle *root) {

  FdbChain *chain = fs->fdb_chain, *prevChain;

  while (chain) {
    free(chain->current);
    prevChain = chain->prev;
    free(chain);
    chain = prevChain;
  }

  free(root);

  return 0;
}

int checkExistingFileDir(DirectoryHandle *d, const char * filename) {

  FirstFileBlock file;

  char *name = calloc(MaxFilenameLen, sizeof(char));
  memcpy(name, filename, strlen(filename));

  FirstDirectoryBlock *dir = d->fdb;
  DiskDriver *disk = d->sfs->disk;
  int entries = dir->num_entries;
  int searched = 0;
  int i, block, ret;

  int found = 0;

  // Search in the first file_blocks
  for (i = 0; i < MaxFileInDir && searched < entries; i++, searched++) {
    block = dir->file_blocks[i];

    // We load the block alyeas as a file
    // The fdb has the fcb as first struct elemen so this is not a problem
    // This is casted to a fdb if is_dir is true in fcb
    ret = DiskDriver_readBlock(disk, &file, block);
    if (ret < 0)
      goto exit;
    // Actually search the file
    if (!memcmp(name, file.fcb.name, sizeof(char) * MaxFilenameLen)) {
        found = 1;
        goto exit;
    }
  }

  // Cerco il file nei directory block negli inode definiti nel fdb
  int inodeIndex;
  for (inodeIndex = 0; inodeIndex < MaxInodeInFFB - 1; inodeIndex++) {

    int dir_block = dir->inode_block[inodeIndex];
    if (dir_block) {
      DirectoryBlock tmp_block;
      ret =
          DiskDriver_readBlock(disk, &tmp_block, dir->inode_block[inodeIndex]);
      if (ret < 0)
        goto exit;

      for (i = 0; i < MaxElemInBlock && searched < entries; i++, searched++) {
        block = tmp_block.file_blocks[i];

        // We load the block alyeas as a file
        // The fdb has the fcb as first struct elemen so this is not a problem
        // This is casted to a fdb if is_dir is true in fcb
        ret = DiskDriver_readBlock(disk, &file, block);
        if (ret < 0)
          goto exit;

        // Actually search the file
        if (!memcmp(name, file.fcb.name, sizeof(char) * MaxFilenameLen)) {
            found = 1;
            goto exit;
        }
      }
    }
  }

  // Check if we have more than 30 inode block
  int nextInodeBlock = dir->inode_block[MaxInodeInFFB - 1];
  // Loop to clear all blocks declared in inod blocks
  while (nextInodeBlock != -1) {
    InodeBlock indexBlock;
    ret = DiskDriver_readBlock(disk, &indexBlock, nextInodeBlock);
    if (ret < 0)
      goto exit;
    for (inodeIndex = 0; inodeIndex < MaxElemInBlock - 1; inodeIndex++) {
      if (indexBlock.inodeList[inodeIndex] == 0) {
        goto exit;
      }
      DirectoryBlock tmp_dir;
      ret = DiskDriver_readBlock(disk, &tmp_dir,
                                 indexBlock.inodeList[inodeIndex]);
      if (ret < 0)
        goto exit;

      for (i = 0; i < MaxElemInBlock && searched < entries; i++, searched++) {
        block = tmp_dir.file_blocks[i];

        ret = DiskDriver_readBlock(disk, &file, block);
        if (ret < 0)
          goto exit;

        // Actually search the file
        if (!memcmp(name, file.fcb.name, sizeof(char) * MaxFilenameLen)) {
            found = 1;
            goto exit;
        }
      }
    }

    nextInodeBlock = indexBlock.inodeList[MaxElemInBlock - 1];
  }

exit:

  free(name);
  return found;
}

// creates an empty file in the directory d
// returns null on error (file existing, no free blocks)
// an empty file consists only of a block of type FirstBlock
FileHandle *SimpleFS_createFileDir(DirectoryHandle *d, const char *filename,
                                   int is_dir) {
  if (checkExistingFileDir(d, filename)) {
    return NULL;
  }

  int ret;

  FileHandle *file = calloc(1, sizeof(FileHandle));

  DiskDriver *disk = d->sfs->disk;
  int destBlock = DiskDriver_getFreeBlock(disk, 0);
  if (destBlock < 0)
    goto err;

  FirstFileBlock *ffb = calloc(1, sizeof(FirstFileBlock));
  ffb->fcb.directory_block = d->pos_in_block;
  ffb->fcb.block_in_disk = destBlock;
  memcpy(ffb->fcb.name, filename, strlen(filename));
  ffb->fcb.size_in_bytes = 0;
  ffb->fcb.size_in_blocks = 0;
  ffb->fcb.is_dir = is_dir;
  ffb->inode_block[MaxInodeInFFB - 1] = -1;

  ret = DiskDriver_writeBlock(disk, ffb, destBlock);
  if (ret < 0)
    goto errFFB;
  FirstDirectoryBlock *destDir = d->fdb;

  int free_block = 0;

  // CASO 1: provo a creare l'elemento nella porzione data dell'fdb
  int i, j;
  for (i = 0; i < MaxFileInDir; i++) {
    if (destDir->file_blocks[i] == 0) {
      destDir->file_blocks[i] = destBlock;
      goto exit;
    }
  }

  // CASO 2: provo a creare l'elemento usando gli inode salvati nell'fdb (se
  // presenti)
  DirectoryBlock tmp_block;
  for (i = 0; i < 31; i++) {
    // controllo se l'inode block esiste
    if (destDir->inode_block[i] != 0) {
      ret = DiskDriver_readBlock(disk, &tmp_block, destDir->inode_block[i]);
      if (ret < 0)
        goto errFFB;
      for (j = 0; j < MaxElemInBlock; j++) {
        // E' STATO TROVATO UNA POSIZIONE LIBERA NEL DIRECTORY BLOCK
        if (tmp_block.file_blocks[j] == 0) {
          tmp_block.file_blocks[j] = destBlock;
          ret =
              DiskDriver_writeBlock(disk, &tmp_block, destDir->inode_block[i]);
          if (ret < 0)
            goto errFFB;
          goto exit;
        }
      }
    } else {
      // l'inode è vuoto quindi viene allocato un directory block
      free_block = DiskDriver_getFreeBlock(disk, 0);
      if (free_block < 0)
        goto errFFB;
      memset(&tmp_block, 0, sizeof(DirectoryBlock));
      tmp_block.file_blocks[0] = destBlock;
      ret = DiskDriver_writeBlock(disk, &tmp_block, free_block);
      if (ret < 0)
        goto errFFB;
      destDir->inode_block[i] = free_block;
      goto exit;
    }
  }

  // SCANSIONE DEI SUCCESSIVI INODEBLOCK, PER TROVARE L'INDIRCE DI UN
  // DIRECTORY_BLOCK IN CUI VI E' UNA POSIZIONE LIBERA
  int next_inode_block = destDir->inode_block[MaxInodeInFFB - 1];
  int current_inode_block = -1;
  InodeBlock tmp_inode_block;

  // CASO 3: provo a creare il file usando un inode block collegato ad un inode
  // block Controllo che l'inode block esiste
  if (next_inode_block != -1) {
    // CASO 3.1: NON CI SONO POSIZIONI LIBERE NEI DIRECTORY_BLOCK ESISTENTI
    //         DI CONSEGUENZA BISOGNA ALLOCARE UN NUOVO DIRECTORY_BLOCK(MA E'
    //         PRESENTE ALMENO UNA POSIZIONE LIBERA IN UN INODE BLOCK CHE
    //         CONTERRA' IL SUO INDICE)
    for (i = 0; i < MaxElemInBlock - 1; i++) {
      // Cerco un inode block libero per allocare un nuovo directory block
      if (tmp_inode_block.inodeList[i] == 0) {
        DirectoryBlock new_dir_block;
        new_dir_block.file_blocks[0] = destBlock;
        int num_block = DiskDriver_getFreeBlock(disk, 0);
        if (num_block < 0)
          goto errFFB;
        ret = DiskDriver_writeBlock(disk, &new_dir_block, num_block);
        if (ret < 0)
          goto errFFB;
        tmp_inode_block.inodeList[i] = num_block;
        ret =
            DiskDriver_writeBlock(disk, &tmp_inode_block, current_inode_block);
        if (ret < 0)
          goto errFFB;

        goto exit;
      }
    }
  }

  // Scorro tutti gli inode block collegati
  while (next_inode_block != -1) {
    current_inode_block = next_inode_block;
    ret = DiskDriver_readBlock(disk, &tmp_inode_block, current_inode_block);
    if (ret < 0)
      goto errFFB;

    // Controllo tutti i directoryblock nell'inodeblock
    for (i = 0; i < MaxElemInBlock - 1; i++) {
      if (tmp_inode_block.inodeList[i] != 0) {
        ret = DiskDriver_readBlock(disk, &tmp_block,
                                   tmp_inode_block.inodeList[i]);
        if (ret < 0)
          goto errFFB;

        // Controllo tutti gli elementi nel directory block
        for (j = 0; j < MaxElemInBlock; j++) {
          // E' STATO TROVATO UNA POSIZIONE LIBERA NEL DIRECTORY BLOCK
          if (tmp_block.file_blocks[j] == 0) {
            tmp_block.file_blocks[j] = destBlock;
            ret = DiskDriver_writeBlock(disk, &tmp_block,
                                        tmp_inode_block.inodeList[i]);
            if (ret < 0)
              goto errFFB;
            goto exit;
          }
        }
      }
    }
    next_inode_block = tmp_inode_block.inodeList[i];
  }

  // 4: Tutti gli inodeblock sono occupati quindi devo allocare un nuovo
  // inodeblock e directory block

  InodeBlock new_inode_block = {0};
  DirectoryBlock new_dir_block = {0};
  new_dir_block.file_blocks[0] = destBlock;
  int num_block = DiskDriver_getFreeBlock(disk, 0);
  if (num_block < 0)
    goto errFFB;
  ret = DiskDriver_writeBlock(disk, &new_dir_block, num_block);
  if (ret < 0)
    goto errFFB;
  new_inode_block.inodeList[0] = num_block;
  new_inode_block.inodeList[MaxElemInBlock - 1] = -1;
  num_block = DiskDriver_getFreeBlock(disk, 0);
  if (num_block < 0)
    goto errFFB;
  ret = DiskDriver_writeBlock(disk, &new_inode_block, num_block);
  if (ret < 0)
    goto errFFB;
  // COLLEGO UN INODEBLOCK CON UN ALTRO INODEBLOCK
  if (current_inode_block != -1) {
    tmp_inode_block.inodeList[MaxElemInBlock - 1] = num_block;
    ret = DiskDriver_writeBlock(disk, &tmp_inode_block, current_inode_block);
    if (ret < 0)
      goto errFFB;
  } else {
    // ALLOCO IL PRIMO INODEBLOCK COLLEGATO AL FIRST DIRECTORY BLOCK
    destDir->inode_block[MaxInodeInFFB - 1] = num_block;
  }

exit:

  // Costruisco il file handler
  file->sfs = d->sfs;
  file->ffb = ffb;
  file->directory = destDir;
  file->pos_in_file = 0;

  // Aggiorno il fdb e lo scrivo
  destDir->num_entries++;
  ret = DiskDriver_writeBlock(disk, destDir, d->pos_in_block);
  if (ret < 0)
    goto errFFB;

  return file;

errFFB:
  free(ffb);
err:
  free(file);

  return NULL;
}

FileHandle *SimpleFS_createFile(DirectoryHandle *d, const char *filename) {

  return SimpleFS_createFileDir(d, filename, 0);
}

/*  Write to internal inode starting from StartIndex
 *
 * *f: FileHandle of the file to write data
 * *data: data to read from
 * size: size of the data to write
 * startIndex: index num to start writing in the InternalInode
 * *num_blocks: pointer to the num blocks left to write
 * *written_bytes: pointer to the bytes left to write
 * *written_blocks: pointer to the blocks written
 *
 * returns: 0 if successful, -1 on error
 */
int writeToInternalNode(FileHandle *f, void *data, int size, int startIndex,
                        int *num_blocks, int *written_bytes,
                        int *written_blocks) {
  DiskDriver *disk = f->sfs->disk;
  FirstFileBlock *ffb = f->ffb;
  int i;
  char *tmp_buf = calloc(MaxDataInBlock, sizeof(char));
  int current_block;
  int writeFFB = 0;
  int ret = 0;

  // Loop all the Inode block saved in the FFB
  for (i = startIndex; i < MaxInodeInFFB - 1 && *num_blocks > 0; i++) {
    current_block = ffb->inode_block[i];
    // Check if block is allocated. If allocated read else gen a new one and
    // save
    if (current_block == 0) {
      current_block = DiskDriver_getFreeBlock(disk, 0);
      if (current_block < 0)
        goto err;
      ffb->inode_block[i] = current_block;
      writeFFB = 1;
    } else {
      ret = DiskDriver_readBlock(disk, tmp_buf, current_block);
      if (ret < 0)
        goto err;
    }

    *num_blocks -= 1;

    // If it's the last block to write, we mus write less data than a block,
    // else write an entire block of data
    if (*num_blocks == 0) {
      memcpy(tmp_buf, data + *written_bytes, size - *written_bytes);
      *written_bytes += size - *written_bytes;
    } else {
      memcpy(tmp_buf, data + *written_bytes, MaxDataInBlock);
      *written_bytes += MaxDataInBlock;
    }

    // Actually write the block and decrement counter
    ret = DiskDriver_writeBlock(disk, tmp_buf, current_block);
    if (ret < 0)
      goto err;
    *written_blocks += 1;
  }


  // Write the FFB
  if (writeFFB) {
    ret = DiskDriver_writeBlock(disk, ffb, ffb->fcb.block_in_disk);
    if (ret < 0)
      goto err;
  }

  free(tmp_buf);

  return 0;

err:
  free(tmp_buf);

  return ret;
}

/*  Write in the ExternalInodeBlock
 *
 * *currInode: pointer to the current inodeBlock to read block from
 * *disk: disk to use to read and write data from
 * startFrom: index to start writing data in the externalInode
 * *data: data to read from
 * size: size of the data to write
 * *num_blocks: pointer to the num blocks left to write
 * *written_bytes: pointer to the bytes left to write
 * *written_blocks: pointer to the blocks written
 *
 * returns: 1 if new blocks are allocated else 0
 */
int writeInExternalInodeBlock(InodeBlock *currInode, DiskDriver *disk,
                              int startFrom, void *data, int size,
                              int *num_blocks, int *written_bytes,
                              int *written_blocks) {

  char *tmp_buf = calloc(MaxDataInBlock, sizeof(char));
  int current_block, i;
  int writeCurrInode = 0;

  int ret = 0;

  for (i = startFrom; i < MaxElemInBlock - 1 && *num_blocks > 0; i++) {
    current_block = currInode->inodeList[i];

    // Check if block is allocated. If allocated read else gen a new one and
    // save
    if (current_block == 0) {
      current_block = DiskDriver_getFreeBlock(disk, 0);
      if (current_block < 0)
        goto err;
      currInode->inodeList[i] = current_block;
      writeCurrInode = 1;
    } else {
      ret = DiskDriver_readBlock(disk, tmp_buf, current_block);
      if (ret < 0)
        goto err;
    }

    *num_blocks -= 1;

    // If it's the last block to write, we mus write less data than a block,
    // else write an entire block of data
    if (*num_blocks == 0) {
      memcpy(tmp_buf, data + *written_bytes, size - *written_bytes);
      *written_bytes += size - *written_bytes;
    } else {
      memcpy(tmp_buf, data + *written_bytes, MaxDataInBlock);
      *written_bytes += MaxDataInBlock;
    }

    // Actually write the block and decrement counter
    ret = DiskDriver_writeBlock(disk, tmp_buf, current_block);
    if (ret < 0)
      goto err;
    *written_blocks += 1;
  }

  free(tmp_buf);

  return writeCurrInode;

err:
  free(tmp_buf);

  return ret;
}

/*  Write to external inode starting from succInodeBlockNum
 *
 * *f: FileHandle of the file to write data
 * *data: data to read from
 * size: size of the data to write
 * succInodeBlockNum: num of External InodeBlock to start write data
 * prevInodeBlockNum: num of prev ExternalInodeBlock
 * *num_blocks: pointer to the num blocks left to write
 * *written_bytes: pointer to the bytes left to write
 * *written_blocks: pointer to the blocks written
 *
 * returns: 0 if successful, -1 on error
 */
int writeToExternalINode(FileHandle *f, void *data, int size,
                         int succInodeBlockNum, int prevInodeBlockNum,
                         int *num_blocks, int *written_bytes,
                         int *written_blocks) {
  DiskDriver *disk = f->sfs->disk;
  FirstFileBlock *ffb = f->ffb;

  int currInodeBlock;
  InodeBlock prevInode;

  InodeBlock currInode;
  int succInodeBlock = succInodeBlockNum;
  int prevInodeBlock = prevInodeBlockNum;
  int writeCurrInode = 0;
  int writePrevInode = 0;
  int writeFFB = 0;

  int ret;

  while (*num_blocks > 0) {
    writePrevInode = 0;

    if (succInodeBlock == -1) {
      memset(&currInode, 0, sizeof(InodeBlock));
      currInodeBlock = DiskDriver_getFreeBlock(disk, 0);
      currInode.inodeList[MaxElemInBlock - 1] = -1;
      // Actually allocate the block to mark it as usedx in the bitmpa
      ret = DiskDriver_writeBlock(disk, &currInode, currInodeBlock);
      if (ret < 0)
        return ret;
      writePrevInode = 1;
    } else {
      currInodeBlock = succInodeBlock;
      ret = DiskDriver_readBlock(disk, &currInode, currInodeBlock);
      if (ret < 0)
        return ret;
    }

    writeCurrInode =
        writeInExternalInodeBlock(&currInode, disk, 0, data, size, num_blocks,
                                  written_bytes, written_blocks);
    if (writeCurrInode < 0)
      return writeCurrInode;

    if (writeCurrInode) {
      DiskDriver_writeBlock(disk, &currInode, currInodeBlock);
    }

    // FirstInodeBlock need to link the inodeblock to ffb
    if (writePrevInode) {
      if (ffb->inode_block[31] == -1) {
        ffb->inode_block[31] = currInodeBlock;
        writeFFB = 1;
      } else {
        ret = DiskDriver_readBlock(disk, &prevInode, prevInodeBlock);
        if (ret < 0)
          return ret;
        prevInode.inodeList[MaxElemInBlock - 1] = currInodeBlock;
        ret = DiskDriver_writeBlock(disk, &prevInode, prevInodeBlock);
        if (ret < 0)
          return ret;
      }
    }

    succInodeBlock = currInode.inodeList[MaxElemInBlock - 1];
    prevInodeBlock = currInodeBlock;
  }

  // Write the FFB
  if (writeFFB) {
    ret = DiskDriver_writeBlock(disk, ffb, ffb->fcb.block_in_disk);
    if (ret < 0)
      return ret;
  }

  return 0;
}

int SimpleFS_write(FileHandle *f, void *data, int size) {

  int written_bytes = 0;
  int written_blocks = 0;

  FirstFileBlock *ffb = f->ffb;
  DiskDriver *disk = f->sfs->disk;
  int pos_start = f->pos_in_file;
  int block_start = f->pos_in_block;
  int file_size = f->ffb->fcb.size_in_bytes;

  int num_blocks; // Blocks to write

  int ret;

  /*  3 casi:
   *  - Scrivo nell'FFB
   *  - Scrivo negli InodeBlock nell'FFB
   *  - Scrvio negli InodeBlock esterni all'FFB
   */

  // Caso 1: parti a scrivere nell FFB
  if (f->pos_in_block_type == FFB) {

    if (size + pos_start <= MaxDataInFFB) {

      memcpy(ffb->data + pos_start, data, size);
      written_blocks++;

      if (pos_start + size > file_size) {
        ffb->fcb.size_in_bytes = pos_start + size;
        if (ffb->fcb.size_in_blocks == 0) {
          ffb->fcb.size_in_blocks = written_blocks;
        }
      }
      ret = DiskDriver_writeBlock(disk, ffb, ffb->fcb.block_in_disk);
      if (ret < 0)
        return ret;
      ret = SimpleFS_seek(f, pos_start + size);
      if (ret < 0)
        return ret;
      return size;
    }

    memcpy(ffb->data + pos_start, data, MaxDataInFFB - pos_start);

    written_bytes += MaxDataInFFB - pos_start;

    num_blocks =
        (pos_start + size - MaxDataInFFB + BLOCK_SIZE - 1) / BLOCK_SIZE;

    ret = writeToInternalNode(f, data, size, 0, &num_blocks, &written_bytes,
                              &written_blocks);
    if (ret < 0)
      return ret;

    ret =
        writeToExternalINode(f, data, size, ffb->inode_block[MaxInodeInFFB - 1],
                             -1, &num_blocks, &written_bytes, &written_blocks);
    if (ret < 0)
      return ret;
    // FINE CASO 1

  } else if (f->pos_in_block_type == InodeBlockFFB) {
    // INIZIO CASO 2
    int index_in_inode_ffb = block_start;

    int offset_block =
        pos_start - MaxDataInFFB - (index_in_inode_ffb * MaxDataInBlock);

    // Writing in block allocated
    if (ffb->inode_block[index_in_inode_ffb] != 0) {
      FileBlock tmp_file = {0};
      int dataToWrite = (offset_block + size) > MaxDataInBlock
                            ? MaxDataInBlock - offset_block
                            : size;

      ret = DiskDriver_readBlock(disk, &tmp_file,
                                 ffb->inode_block[index_in_inode_ffb]);
      if (ret < 0)
        return ret;

      memcpy(tmp_file.data + offset_block, data, dataToWrite);
      ret = DiskDriver_writeBlock(disk, &tmp_file,
                                  ffb->inode_block[index_in_inode_ffb]);
      if (ret < 0)
        return ret;

      written_bytes += dataToWrite;
    }

    index_in_inode_ffb++;

    num_blocks = ((size - written_bytes) + BLOCK_SIZE - 1) / BLOCK_SIZE;

    ret = writeToInternalNode(f, data, size, index_in_inode_ffb, &num_blocks,
                              &written_bytes, &written_blocks);
    if (ret < 0)
      return ret;

    ret =
        writeToExternalINode(f, data, size, ffb->inode_block[MaxInodeInFFB - 1],
                             -1, &num_blocks, &written_bytes, &written_blocks);
    if (ret < 0)
      return ret;

    // FINE CASO 2
  } else if (f->pos_in_block_type == ExternalInodeBlock) {
    // INIZIO CASO 3

    // Calc data in external inode
    int data_in_External_inode =
        pos_start - MaxDataInFFB - ((MaxInodeInFFB - 1) * MaxDataInBlock);

    // Calc blocks in external inode
    int data_in_External_inode_blocks = data_in_External_inode / MaxDataInBlock;

    // Inode to skip
    int inodeToSkip = data_in_External_inode_blocks / (MaxElemInBlock - 1);

    // Absolute offset in ExternalInode
    int absOffsetBlock = pos_start - MaxDataInFFB -
                         ((MaxInodeInFFB - 1) * MaxDataInBlock) -
                         (inodeToSkip * MaxDataInBlock * (MaxElemInBlock - 1));

    // Index to write in the dest external Inode block
    int offsetBlock = absOffsetBlock / MaxDataInBlock;
    // Offset to write in the dest block in the external InodeBlock
    int offsetInBlock = absOffsetBlock % MaxDataInBlock;

    // Calc num blocks to write
    num_blocks = (size - +BLOCK_SIZE - 1) / BLOCK_SIZE;

    // Start the skipping process
    // Take the linked node from FFB
    InodeBlock currInode = {0};

    // Initialize the blockIndexs with the FFB
    int succInodeBlock = f->ffb->inode_block[31];
    int prevInodeBlock = -1;
    int currentInodeIndex;

    int targetBlock;

    int writeCurrInode = 0;

    // Read the inodeBlock linked in the FFB
    ret = DiskDriver_readBlock(disk, &currInode, succInodeBlock);
    if (ret < 0)
      return ret;

    currentInodeIndex = succInodeBlock;
    succInodeBlock = currInode.inodeList[MaxElemInBlock - 1];

    if (currentInodeIndex != -1) {

      //  Skip inode_start linked inodeBlock
      while (inodeToSkip > 0) {
        currentInodeIndex = succInodeBlock;
        ret = DiskDriver_readBlock(disk, &currInode, succInodeBlock);
        if (ret < 0)
          return ret;

        succInodeBlock = currInode.inodeList[MaxElemInBlock - 1];
        inodeToSkip--;
      }

      // Set the block to start writing from
      targetBlock = currInode.inodeList[offsetBlock];

      if (offsetBlock < MaxElemInBlock - 1) {

        char *tmp_file = calloc(MaxDataInBlock, sizeof(char));
        // Calc data to write in the targetBlock
        int dataToWrite = (offsetInBlock + size) > MaxDataInBlock
                              ? MaxDataInBlock - offsetInBlock
                              : size;

        // Write the data to the targetBlock with the offset calc
        ret = DiskDriver_readBlock(disk, tmp_file, targetBlock);
        if (ret < 0) {
          free(tmp_file);
          return ret;
        }

        memcpy(tmp_file + offsetInBlock, data, dataToWrite);
        ret = DiskDriver_writeBlock(disk, tmp_file, targetBlock);
        if (ret < 0) {
          free(tmp_file);
          return ret;
        }

        written_bytes += dataToWrite;

        free(tmp_file);

        // Set the blocks to write left
        num_blocks = ((size - written_bytes) + BLOCK_SIZE - 1) / BLOCK_SIZE;

        offsetBlock++;

        if (offsetBlock < MaxElemInBlock - 1) {

          writeCurrInode = writeInExternalInodeBlock(
              &currInode, disk, offsetBlock, data, size, &num_blocks,
              &written_bytes, &written_blocks);

          if (writeCurrInode < 0)
            return writeCurrInode;

          if (writeCurrInode)
            DiskDriver_writeBlock(disk, &currInode, currentInodeIndex);
        }
      }

      prevInodeBlock = currentInodeIndex;
      currentInodeIndex = currInode.inodeList[MaxElemInBlock - 1];
    }

    ret = writeToExternalINode(f, data, size, currentInodeIndex, prevInodeBlock,
                               &num_blocks, &written_bytes, &written_blocks);
    if (ret < 0)
      return ret;
  }

  // File size - start pos + writtenbytes
  if (pos_start + written_bytes > file_size) {
    ffb->fcb.size_in_bytes = pos_start + written_bytes;
    ffb->fcb.size_in_blocks += written_blocks;
  }

  ret = DiskDriver_writeBlock(disk, ffb, ffb->fcb.block_in_disk);
  if (ret < 0)
    return ret;
  ret = SimpleFS_seek(f, pos_start + written_bytes);
  if (ret < 0)
    return ret;

  return written_bytes;
}

int DeleteStoredFile(DiskDriver *disk, FirstFileBlock *file) {
  int blocksToDel = file->fcb.size_in_blocks;
  int inodeIndex;
  int ret;

  // Error by freeBlock doesn't terminate the programm

  // File is stored only in the ffb, skip other clean
  if (blocksToDel == 1)
    goto exit;

  // Clear the blocks stored in inode block array
  for (inodeIndex = 0; inodeIndex < MaxInodeInFFB - 1 && blocksToDel > 0;
       inodeIndex++, blocksToDel--) {
    ret = DiskDriver_freeBlock(disk, file->inode_block[inodeIndex]);
    if (ret < 0)
      handle_error("Error in freeing block", ret);
  }

  // Check if we have more than 30 inode block
  int nextInodeBlock = file->inode_block[MaxInodeInFFB - 1];

  // Loop to clear all blocks declared in inod blocks
  while (nextInodeBlock != -1) {
    InodeBlock indexBlock;
    ret = DiskDriver_readBlock(disk, &indexBlock, nextInodeBlock);
    if (ret < 0)
      return ret;
    for (inodeIndex = 0; inodeIndex < MaxElemInBlock - 1 && blocksToDel > 0;
         inodeIndex++, blocksToDel--) {

      ret = DiskDriver_freeBlock(disk, indexBlock.inodeList[inodeIndex]);
      if (ret < 0)
        handle_error("Error in freeing block", ret);
    }

    // Clear the inode block
    ret = DiskDriver_freeBlock(disk, nextInodeBlock);
    if (ret < 0)
      handle_error("Error in freeing block", ret);

    nextInodeBlock = indexBlock.inodeList[MaxElemInBlock - 1];
  }

exit:

  // Finally clear th ffb
  ret = DiskDriver_freeBlock(disk, file->fcb.block_in_disk);
  if (ret < 0)
    handle_error("Error in freeing block", ret);

  return ret;
}

int DeleteStoredDir(DiskDriver *disk, FirstDirectoryBlock *dir) {
  FirstFileBlock *file_in_dir = malloc(sizeof(FirstFileBlock));

  int entryToDel = dir->num_entries;
  int inodeIndex;

  int ret;

  int i, j, block;

  for (i = 0; i < MaxFileInDir && entryToDel > 0; i++) {
    block = dir->file_blocks[i];
    if (block) {
      entryToDel--;
      // We load the block alyeas as a file
      // The fdb has the fcb as first struct elemen so this is not a problem
      // This is casted to a fdb if is_dir is true in fcb
      ret = DiskDriver_readBlock(disk, file_in_dir, block);
      if (ret < 0)
        goto err;

      FileControlBlock fcb = file_in_dir->fcb;
      if (fcb.is_dir) {
        ret = DeleteStoredDir(disk, (FirstDirectoryBlock *)file_in_dir);
        if (ret < 0)
          handle_error("There were some error in removing this directory", ret);
      } else {
        ret = DeleteStoredFile(disk, file_in_dir);
        if (ret < 0)
          handle_error("There were some error in removing this file", ret);
      }
    }
  }

  // Entries zero skip search in other blocks
  if (entryToDel == 0)
    goto exit;

  DirectoryBlock dirBlock;
  // Search in the first inode block in ffb
  for (i = 0; i < MaxInodeInFFB - 1; i++) {
    block = dir->inode_block[i];

    // Check if inode block exist
    if (block) {
      // Read the directoryblock linked by the inode
      ret = DiskDriver_readBlock(disk, &dirBlock, block);
      if (ret < 0)
        goto err;

      // Check the directoryblock
      for (j = 0; j < MaxElemInBlock && entryToDel > 0; j++) {

        block = dirBlock.file_blocks[j];
        if (block) {
          entryToDel--;
          ret = DiskDriver_readBlock(disk, file_in_dir, block);
          if (ret < 0)
            goto err;

          FileControlBlock fcb = file_in_dir->fcb;

          // Actually search the file
          if (fcb.is_dir) {
            ret = DeleteStoredDir(disk, (FirstDirectoryBlock *)file_in_dir);
            if (ret < 0)
              handle_error("There were some error in removing this directory",
                           ret);

          } else {
            ret = DeleteStoredFile(disk, file_in_dir);
            if (ret < 0)
              handle_error("There were some error in removing this file", ret);
          }
        }
      }
      // Clear the directory blocks
      ret = DiskDriver_freeBlock(disk, block);
      if (ret < 0)
        goto err;
    }
  }

  // Entries zero skip search in other blocks
  if (entryToDel == 0)
    goto exit;

  // Check if we have more than 30 inode block
  int nextInodeBlock = dir->inode_block[MaxInodeInFFB - 1];
  InodeBlock inodeBlock;

  // Loop to clear all blocks declared in inod blocks
  while (nextInodeBlock != -1) {
    ret = DiskDriver_readBlock(disk, &inodeBlock, nextInodeBlock);
    if (ret < 0)
      goto err;

    for (inodeIndex = 0; inodeIndex < MaxElemInBlock - 1; inodeIndex++) {

      block = inodeBlock.inodeList[i];
      if (block) {
        ret = DiskDriver_readBlock(disk, &dirBlock, block);
        if (ret < 0)
          goto err;

        for (j = 0; j < MaxElemInBlock && entryToDel > 0; j++) {
          block = dirBlock.file_blocks[j];

          if (block) {
            entryToDel--;
            ret = DiskDriver_readBlock(disk, file_in_dir, block);
            if (ret < 0)
              goto err;

            FileControlBlock fcb = file_in_dir->fcb;

            // Actually search the file
            if (fcb.is_dir) {
              ret = DeleteStoredDir(disk, (FirstDirectoryBlock *)file_in_dir);
              if (ret < 0)
                handle_error("There were some error in removing this directory",
                             ret);

            } else {
              ret = DeleteStoredFile(disk, file_in_dir);
              if (ret < 0)
                handle_error("There were some error in removing this file",
                             ret);
            }
          }
        }

        // Clear the directory blocks
        ret = DiskDriver_freeBlock(disk, block);
        if (ret < 0)
          goto err;
      }

      // Clear the inode block
      ret = DiskDriver_freeBlock(disk, nextInodeBlock);
      if (ret < 0)
        goto err;
      nextInodeBlock = inodeBlock.inodeList[MaxElemInBlock - 1];
    }
  }
exit:

  ret = DiskDriver_freeBlock(disk, dir->fcb.block_in_disk);
  if (ret < 0) {
    free(file_in_dir);
    return ret;
  }

  free(file_in_dir);

  return 0;
err:
  free(file_in_dir);

  return ret;
}

// Check if a directory block is empty
int CheckEmptyDirBlock(DirectoryBlock dir) {
  int i;
  for (i = 0; i < MaxElemInBlock; i++) {
    if (dir.file_blocks[i])
      return 0;
  }
  return 1;
}

int SimpleFS_remove(SimpleFS *fs, char *filename) {

  // By default set the file as not found
  int found = -1;

  if (!fs->fdb_current_dir)
    return -1;

  DiskDriver *disk = fs->disk;
  FirstDirectoryBlock *dir = fs->fdb_current_dir;

  int entries = dir->num_entries;

  int i, block, inodeBlockNum, dirBlockNum, ret;

  FirstFileBlock *file = calloc(1, sizeof(FirstFileBlock));

  // I hate strcmp
  char *name = calloc(MaxFilenameLen, sizeof(char));
  memcpy(name, filename, strlen(filename));

  // Search in the first file_blocks
  for (i = 0; i < MaxFileInDir && entries > 0; i++) {
    block = dir->file_blocks[i];
    if (block) {
      entries--;
      // We load the block alyeas as a file
      // The fdb has the fcb as first struct elemen so this is not a problem
      // This is casted to a fdb if is_dir is true in fcb
      ret = DiskDriver_readBlock(disk, file, block);
      if (ret < 0)
        goto err;
      FileControlBlock fcb = file->fcb;

      // Actually search the file
      if (!memcmp(name, fcb.name, sizeof(char) * MaxFilenameLen)) {
        if (fcb.is_dir) {
          found = DeleteStoredDir(disk, (FirstDirectoryBlock *)file);
          if (found < 0) {
            ret = found;
            goto err;
          }
        } else {
          found = DeleteStoredFile(disk, file);
          if (found < 0) {
            ret = found;
            goto err;
          }
        }

        // Setto l'indice vuoto nel file_blocks del fdb
        if (!found) {
          dir->file_blocks[i] = 0;
        }

        // File found skip searching in remaining entires
        goto exit;
      }
    }
  }

  // Entries zero skip search in other blocks
  if (entries == 0)
    goto exit;

  DirectoryBlock dirBlock;
  // Search in the first inode block in ffb
  for (i = 0; i < MaxInodeInFFB - 1; i++) {
    inodeBlockNum = dir->inode_block[i];

    // Check if inode block exist
    if (inodeBlockNum) {
      // Read the directoryblock linked by the inode
      ret = DiskDriver_readBlock(disk, &dirBlock, inodeBlockNum);
      if (ret < 0)
        goto err;
      // Check the directoryblock
      for (dirBlockNum = 0; dirBlockNum < MaxElemInBlock && entries > 0;
           dirBlockNum++) {

        block = dirBlock.file_blocks[dirBlockNum];
        if (block) {
          entries--;
          ret = DiskDriver_readBlock(disk, file, block);
          if (ret < 0)
            goto err;
          FileControlBlock fcb = file->fcb;

          // Actually search the file
          if (!memcmp(name, fcb.name, sizeof(char) * MaxFilenameLen)) {
            if (fcb.is_dir) {
              found = DeleteStoredDir(disk, (FirstDirectoryBlock *)file);
              if (found < 0) {
                ret = found;
                goto err;
              }
            } else {
              found = DeleteStoredFile(disk, file);
              if (found < 0) {
                ret = found;
                goto err;
              }
            }

            // Update the dirblock linked from the inodeBlockNum
            if (!found) {
              dirBlock.file_blocks[dirBlockNum] = 0;
              // Check if the block is empty and dealloc if needed
              if (CheckEmptyDirBlock(dirBlock)) {
                ret = DiskDriver_freeBlock(disk, inodeBlockNum);
                if (ret < 0)
                  goto err;
                dir->inode_block[i] = 0;
              } else {
                ret = DiskDriver_writeBlock(disk, &dirBlock, inodeBlockNum);
                if (ret < 0)
                  goto err;
              }
            }

            // File found skip searching in remaining entires
            goto exit;
          }
        }
      }
    }
  }

  // Entries zero skip search in other blocks
  if (entries == 0)
    goto exit;

  // Check if we have more than 30 inode block
  int nextInodeBlock = dir->inode_block[MaxInodeInFFB - 1], inodeIndex;
  InodeBlock inodeBlock;

  // Loop to clear all blocks declared in inod blocks
  while (nextInodeBlock != -1) {

    ret = DiskDriver_readBlock(disk, &inodeBlock, nextInodeBlock);
    if (ret < 0)
      goto err;
    for (inodeIndex = 0; inodeIndex < MaxElemInBlock - 1; inodeIndex++) {

      inodeBlockNum = inodeBlock.inodeList[i];
      if (inodeBlockNum) {
        ret = DiskDriver_readBlock(disk, &dirBlock, inodeBlockNum);
        if (ret < 0)
          goto err;

        for (dirBlockNum = 0; dirBlockNum < MaxElemInBlock && entries > 0;
             dirBlockNum++) {
          block = dirBlock.file_blocks[dirBlockNum];

          if (block) {
            entries--;
            ret = DiskDriver_readBlock(disk, file, block);
            if (ret < 0)
              goto err;
            FileControlBlock fcb = file->fcb;

            // Actually search the file
            if (!memcmp(name, fcb.name, sizeof(char) * MaxFilenameLen)) {
              if (fcb.is_dir) {
                found = DeleteStoredDir(disk, (FirstDirectoryBlock *)file);
                if (found < 0) {
                  ret = found;
                  goto err;
                }
              } else {
                found = DeleteStoredFile(disk, file);
                if (found < 0) {
                  ret = found;
                  goto err;
                }
              }

              if (!found) {
                dirBlock.file_blocks[dirBlockNum] = 0;
                ret = DiskDriver_writeBlock(disk, &dirBlock, inodeBlockNum);
                if (ret < 0)
                  goto err;

                if (CheckEmptyDirBlock(dirBlock)) {
                  ret = DiskDriver_freeBlock(disk, inodeBlockNum);
                  if (ret < 0)
                    goto err;
                  inodeBlock.inodeList[i] = 0;
                  ret =
                      DiskDriver_writeBlock(disk, &inodeBlock, nextInodeBlock);
                  if (ret < 0)
                    goto err;
                } else {
                  ret = DiskDriver_writeBlock(disk, &dirBlock, inodeBlockNum);
                  if (ret < 0)
                    goto err;
                }
              }

              // File found skip searching in remaining entires
              goto exit;
            }
          }
        }
      }
    }

    nextInodeBlock = inodeBlock.inodeList[MaxElemInBlock - 1];
  }

exit:

  if (!found)
    dir->num_entries--;
  DiskDriver_writeBlock(disk, dir, dir->fcb.block_in_disk);

  free(file);
  free(name);

  return found;

err:

  free(file);
  free(name);
  return ret;
}

// creates a new directory in the current one (stored in
// fs->current_directory_block) 0 on success -1 on error
int SimpleFS_mkDir(DirectoryHandle *d, char *dirname) {

  FileHandle *file = SimpleFS_createFileDir(d, dirname, 1);

  if (file) {
    free(file->ffb);
    free(file);
    return 0;
  }

  return -1;
}

int SimpleFS_changeDir(DirectoryHandle *d, char *dirname) {

  if (!strcmp(dirname, "..")) {
    FdbChain *prev = d->sfs->fdb_chain->prev;
    if (prev) {
      d->directory = prev->prev ? prev->prev->current : NULL;
      d->fdb = prev->current;
      d->pos_in_dir = 0;
      d->pos_in_block = prev->current->fcb.block_in_disk;
      free(d->sfs->fdb_chain->current);
      free(d->sfs->fdb_chain);
      d->sfs->fdb_chain = prev;
      d->sfs->fdb_current_dir = d->fdb;
      return 0;
    }
    return -1;
  }

  DiskDriver *disk = d->sfs->disk;
  FirstDirectoryBlock *dir = d->fdb;

  int entries = dir->num_entries;
  int i, block, j, ret;

  FirstFileBlock *file = malloc(sizeof(FirstFileBlock));

  // I hate strcmp
  char *name = calloc(MaxFilenameLen, sizeof(char));
  memcpy(name, dirname, strlen(dirname));

  // Search in the first file_blocks
  for (i = 0; i < 31 && entries > 0; i++) {
    block = dir->file_blocks[i];
    if (block) {
      entries--;
      // We load the block alyeas as a file
      // The fdb has the fcb as first struct elemen so this is not a problem
      // This is casted to a fdb if is_dir is true in fcb
      ret = DiskDriver_readBlock(disk, file, block);
      if (ret < 0)
        goto err;
      FileControlBlock fcb = file->fcb;

      // Actually search the file
      if (!memcmp(name, fcb.name, sizeof(char) * MaxFilenameLen)) {
        if (fcb.is_dir) {
          goto exit;
        }
      }
    }
  }

  // Entries zero skip search in other blocks
  if (entries == 0)
    goto err;

  DirectoryBlock dirBlock;
  // Search in the first inode block in ffb
  for (i = 0; i < MaxInodeInFFB - 1; i++) {
    block = dir->inode_block[i];

    // Check if inode block exist
    if (block) {
      // Read the directoryblock linked by the inode
      ret = DiskDriver_readBlock(disk, &dirBlock, block);
      if (ret < 0)
        goto err;
      // Check the directoryblock
      for (j = 0; j < MaxElemInBlock && entries > 0; j++) {

        block = dirBlock.file_blocks[j];
        if (block) {
          entries--;

          ret = DiskDriver_readBlock(disk, file, block);
          if (ret < 0)
            goto err;
          FileControlBlock fcb = file->fcb;

          // Actually search the file
          if (!memcmp(name, fcb.name, sizeof(char) * MaxFilenameLen)) {
            if (fcb.is_dir) {
              goto exit;
            }
          }
        }
      }
    }
  }

  // Entries zero skip search in other blocks
  if (entries == 0)
    goto err;

  // Check if we have more than 30 inode block
  int nextInodeBlock = dir->inode_block[MaxInodeInFFB - 1], inodeIndex;
  InodeBlock inodeBlock;

  // Loop to clear all blocks declared in inod blocks
  while (nextInodeBlock != -1) {

    ret = DiskDriver_readBlock(disk, &inodeBlock, nextInodeBlock);
    if (ret < 0)
      goto err;
    for (inodeIndex = 0; inodeIndex < MaxElemInBlock - 1; inodeIndex++) {

      block = inodeBlock.inodeList[i];
      if (block) {
        ret = DiskDriver_readBlock(disk, &dirBlock, block);
        if (ret < 0)
          goto err;

        for (j = 0; j < MaxElemInBlock && entries > 0; j++) {
          block = dirBlock.file_blocks[j];

          if (block) {
            entries--;
            ret = DiskDriver_readBlock(disk, file, block);
            if (ret < 0)
              goto err;
            FileControlBlock fcb = file->fcb;

            // Actually search the file
            if (!memcmp(name, fcb.name, sizeof(char) * MaxFilenameLen)) {
              if (fcb.is_dir) {
                goto exit;
              }
            }
          }
        }
      }
    }

    nextInodeBlock = inodeBlock.inodeList[MaxElemInBlock - 1];
  }

exit:

  d->directory = d->fdb;
  d->fdb = (FirstDirectoryBlock *)file;
  d->pos_in_dir = 0;
  d->pos_in_block = file->fcb.block_in_disk;

  d->sfs->fdb_current_dir = d->fdb;

  free(name);

  // Allocazione della catena di directory
  FdbChain *new_chain = calloc(1, sizeof(FdbChain));
  new_chain->current = d->fdb;
  new_chain->prev = d->sfs->fdb_chain;
  d->sfs->fdb_chain = new_chain;

  return 0;

err:

  free(name);
  free(file);

  return -1;
}

// opens a file in the  directory d. The file should be exisiting
FileHandle *SimpleFS_openFile(DirectoryHandle *d, const char *filename) {

  FileHandle *openFile = calloc(1, sizeof(FileHandle));

  FirstFileBlock *file = malloc(sizeof(FirstFileBlock));

  char *name = calloc(MaxFilenameLen, sizeof(char));
  memcpy(name, filename, strlen(filename));

  FirstDirectoryBlock *dir = d->fdb;
  DiskDriver *disk = d->sfs->disk;
  int entries = dir->num_entries;
  int searched = 0;
  int i, block, ret;

  // Search in the first file_blocks
  for (i = 0; i < MaxFileInDir && searched < entries; i++, searched++) {
    block = dir->file_blocks[i];

    // We load the block alyeas as a file
    // The fdb has the fcb as first struct elemen so this is not a problem
    // This is casted to a fdb if is_dir is true in fcb
    ret = DiskDriver_readBlock(disk, file, block);
    if (ret < 0)
      goto err;
    FileControlBlock fcb = file->fcb;
    // Actually search the file
    if (!memcmp(name, fcb.name, sizeof(char) * MaxFilenameLen)) {
      if (fcb.is_dir) {
        goto err;
      } else {
        openFile->sfs = d->sfs;
        openFile->ffb = file;
        openFile->directory = dir;
        openFile->pos_in_file = 0;
        goto succ;
      }
    }
  }

  // Cerco il file nei directory block negli inode definiti nel fdb
  int inodeIndex;
  for (inodeIndex = 0; inodeIndex < MaxInodeInFFB - 1; inodeIndex++) {

    int dir_block = dir->inode_block[inodeIndex];
    if (dir_block) {
      DirectoryBlock tmp_block;
      ret =
          DiskDriver_readBlock(disk, &tmp_block, dir->inode_block[inodeIndex]);
      if (ret < 0)
        goto err;

      for (i = 0; i < MaxElemInBlock && searched < entries; i++, searched++) {
        block = tmp_block.file_blocks[i];

        // We load the block alyeas as a file
        // The fdb has the fcb as first struct elemen so this is not a problem
        // This is casted to a fdb if is_dir is true in fcb
        ret = DiskDriver_readBlock(disk, file, block);
        if (ret < 0)
          goto err;

        FileControlBlock fcb = file->fcb;
        // Actually search the file
        if (!memcmp(name, fcb.name, sizeof(char) * MaxFilenameLen)) {
          if (fcb.is_dir) {
            goto err;
          } else {
            openFile->sfs = d->sfs;
            openFile->ffb = file;
            openFile->directory = dir;
            openFile->pos_in_file = 0;
            goto succ;
          }
        }
      }
    }
  }

  // Check if we have more than 30 inode block
  int nextInodeBlock = dir->inode_block[MaxInodeInFFB - 1];
  // Loop to clear all blocks declared in inod blocks
  while (nextInodeBlock != -1) {
    InodeBlock indexBlock;
    ret = DiskDriver_readBlock(disk, &indexBlock, nextInodeBlock);
    if (ret < 0)
      goto err;
    for (inodeIndex = 0; inodeIndex < MaxElemInBlock - 1; inodeIndex++) {
      if (indexBlock.inodeList[inodeIndex] == 0) {
        goto err;
      }
      DirectoryBlock tmp_dir;
      ret = DiskDriver_readBlock(disk, &tmp_dir,
                                 indexBlock.inodeList[inodeIndex]);
      if (ret < 0)
        goto err;

      for (i = 0; i < MaxElemInBlock && searched < entries; i++, searched++) {
        block = tmp_dir.file_blocks[i];

        ret = DiskDriver_readBlock(disk, file, block);
        if (ret < 0)
          goto err;
        FileControlBlock fcb = file->fcb;

        // Actually search the file
        if (!memcmp(name, fcb.name, sizeof(char) * MaxFilenameLen)) {
          if (fcb.is_dir) {
            goto err;
          } else {
            openFile->sfs = d->sfs;
            openFile->ffb = file;
            openFile->directory = dir;
            openFile->pos_in_file = 0;
            goto succ;
          }
        }
      }
    }

    nextInodeBlock = indexBlock.inodeList[MaxElemInBlock - 1];
  }

err:

  free(openFile);
  free(file);
  free(name);
  return NULL;

succ:

  free(name);
  return openFile;
}

int SimpleFS_readDir(char **names, DirectoryHandle *d) {

  int entries = d->fdb->num_entries;
  int i, inodeBlockNum, j, found = 0;
  int block, ret;
  FirstFileBlock file;
  DiskDriver *disk = d->sfs->disk;
  FirstDirectoryBlock *dir = d->fdb;

  for (i = 0; i < MaxFileInDir && found < entries; i++) {
    block = d->fdb->file_blocks[i];
    if (block) {
      ret = DiskDriver_readBlock(disk, &file, block);
      if (ret < 0)
        return ret;
      names[found] = malloc(MaxFilenameLen);
      memcpy(names[found], file.fcb.name, MaxFilenameLen);
      found++;
    }
  }

  DirectoryBlock dirBlock;

  // Search in the first inode block in ffb
  for (i = 0; i < MaxInodeInFFB - 1; i++) {
    inodeBlockNum = dir->inode_block[i];

    // Check if inode block exist
    if (inodeBlockNum) {
      // Read the directoryblock linked by the inode
      ret = DiskDriver_readBlock(disk, &dirBlock, inodeBlockNum);
      if (ret < 0)
        return ret;
      // Check the directoryblock
      for (j = 0; j < MaxElemInBlock && found < entries; j++) {

        block = dirBlock.file_blocks[j];
        if (block) {
          ret = DiskDriver_readBlock(disk, &file, block);
          if (ret < 0)
            return ret;
          names[found] = malloc(MaxFilenameLen);
          memcpy(names[found], file.fcb.name, MaxFilenameLen);
          found++;
        }
      }
    }
  }

  // Check if we have more than 30 inode block
  int nextInodeBlock = dir->inode_block[MaxInodeInFFB - 1], inodeIndex;
  InodeBlock inodeBlock;

  // Loop to clear all blocks declared in inod blocks
  while (nextInodeBlock != -1) {

    ret = DiskDriver_readBlock(disk, &inodeBlock, nextInodeBlock);
    if (ret < 0)
      return ret;
    for (inodeIndex = 0; inodeIndex < MaxElemInBlock - 1; inodeIndex++) {

      inodeBlockNum = inodeBlock.inodeList[i];
      if (inodeBlockNum) {
        ret = DiskDriver_readBlock(disk, &dirBlock, inodeBlockNum);
        if (ret < 0)
          return ret;

        for (j = 0; j < MaxElemInBlock && found < entries; j++) {
          block = dirBlock.file_blocks[j];

          if (block) {
            ret = DiskDriver_readBlock(disk, &file, block);
            if (ret < 0)
              return ret;
            names[found] = malloc(MaxFilenameLen);
            memcpy(names[found], file.fcb.name, MaxFilenameLen);
            found++;
          }
        }
      }
    }

    nextInodeBlock = inodeBlock.inodeList[MaxElemInBlock - 1];
  }
  return 0;
}

/*  Read in external inode starting from startFrom
 *
 * *disk: disk to read data from
 * *inodeBlock: Inodeblock to read fileBlock from
 * *blockToRead: pointer to the number block to read
 *  size: size of the data to read
 * *data: pointer to write data in
 *  startFrom: num of External InodeBlock to start read data
 * *bytes_read: pointer to the bytes readed
 *
 * returns: 0 if successful, -1 on error
 */
int readDataFromExternalInode(DiskDriver *disk, InodeBlock *inodeBlock,
                              int *blockToRead, int size, void *data,
                              int startFrom, int *bytes_read) {

  int i, inodeBlockNum, ret;
  FileBlock fileBlock;

  for (i = startFrom; i < MaxElemInBlock - 1 && *blockToRead > 0; i++) {
    inodeBlockNum = inodeBlock->inodeList[i];
    if (inodeBlockNum) {
      // Read the directoryblock linked by the inode
      ret = DiskDriver_readBlock(disk, &fileBlock, inodeBlockNum);
      if (ret < 0)
        return ret;
      *blockToRead -= 1;
      if (*blockToRead > 0) {
        memcpy(data + *bytes_read, &fileBlock, MaxDataInBlock);
        *bytes_read += MaxDataInBlock;
      } else {
        memcpy(data + *bytes_read, &fileBlock, size - *bytes_read);
        *bytes_read += size - *bytes_read;
      }
    }
  }
  return 0;
}

/*  Read from external inode starting from nextInodeBlock
 *
 * *disk: disk to read data from
 * *blockToRead: pointer to the number block to read
 *  size: size of the data to read
 * *data: pointer to write data in
 *  nextInodeBlock: num of External InodeBlock to start read data
 * *bytes_read: pointer to the bytes readed
 *
 * returns: 0 if successful, -1 on error
 */
int readFromExternalInode(DiskDriver *disk, int *blockToRead, int size,
                          void *data, int nextInodeBlock, int *bytes_read) {

  InodeBlock inodeBlock;
  int ret;

  while (nextInodeBlock != -1 && *blockToRead > 0) {
    ret = DiskDriver_readBlock(disk, &inodeBlock, nextInodeBlock);
    if (ret < 0)
      return ret;
    ret = readDataFromExternalInode(disk, &inodeBlock, blockToRead, size, data,
                                    0, bytes_read);
    if (ret < 0)
      return ret;
    nextInodeBlock = inodeBlock.inodeList[MaxElemInBlock - 1];
  }
  return 0;
}

/*  Read from internal inode starting from startFrom
 *
 * *f: FileHandle of the file to read data
 * *blockToRead: pointer to the number block to read
 *  size: size of the data to read
 * *data: pointer to write data in
 *  startFrom: num of internal InodeBlock to start read data
 * *bytes_read: pointer to the bytes readed
 *
 * returns: 0 if successful, -1 on error
 */
int readFromInternalInode(FileHandle *f, int *blockToRead, int size, void *data,
                          int startFrom, int *bytes_read) {

  int inodeBlockNum, i, ret;
  FileBlock fileBlock;

  for (i = startFrom; i < MaxInodeInFFB - 1 && *blockToRead > 0; i++) {
    inodeBlockNum = f->ffb->inode_block[i];
    // Check if inode block exist
    if (inodeBlockNum) {
      // Read the directoryblock linked by the inode
      ret = DiskDriver_readBlock(f->sfs->disk, &fileBlock, inodeBlockNum);
      if (ret < 0)
        return ret;
      *blockToRead -= 1;
      if (*blockToRead > 0) {
        memcpy(data + *bytes_read, &fileBlock, MaxDataInBlock);
        *bytes_read += MaxDataInBlock;
      } else {
        memcpy(data + *bytes_read, &fileBlock, size - *bytes_read);
        *bytes_read += size - *bytes_read;
      }
    }
  }
  return 0;
}

int SimpleFS_read(FileHandle *f, void *data, int size) {

  DiskDriver *disk = f->sfs->disk;

  int pos_start = f->pos_in_file;

  int bytes_read = 0;
  int i, ret;
  FirstFileBlock *ffb = f->ffb;

  FileBlock fileBlock;

  if (f->pos_in_block_type == FFB) {
    // CASO 1:
    int block_to_read;
    if (pos_start + size < MaxDataInFFB) {
      memcpy(data, ffb->data + pos_start, size);
      ret = SimpleFS_seek(f, pos_start + size);
      if (ret < 0)
        return ret;
      return size;
    }

    memcpy(data, ffb->data + pos_start, MaxDataInFFB - pos_start);
    bytes_read += MaxDataInFFB - pos_start;

    block_to_read = (size - bytes_read + MaxDataInBlock - 1) / MaxDataInBlock;

    // Search in the first inode block in ffb
    ret = readFromInternalInode(f, &block_to_read, size, data, 0, &bytes_read);
    if (ret < 0)
      return ret;
    int nextInodeBlock = ffb->inode_block[MaxInodeInFFB - 1];
    // Read from the remaning external Inode if presents
    ret = readFromExternalInode(disk, &block_to_read, size, data,
                                nextInodeBlock, &bytes_read);
    if (ret < 0)
      return ret;

  } else if (f->pos_in_block_type == InodeBlockFFB) {
    // CASO 2:

    int block_to_skip = (pos_start - MaxDataInFFB) / MaxDataInBlock;

    int offsetInBlock =
        pos_start - MaxDataInFFB - (block_to_skip * MaxDataInBlock);

    int bytes_to_read_in_block = offsetInBlock + size > MaxDataInBlock
                                     ? MaxDataInBlock - offsetInBlock
                                     : size;

    int block_to_read =
        (size - bytes_to_read_in_block + MaxDataInBlock - 1) / MaxDataInBlock;
    // Read the data from blockToSkip with offset
    ret =
        DiskDriver_readBlock(disk, &fileBlock, ffb->inode_block[block_to_skip]);
    if (ret < 0)
      return ret;
    memcpy(data, fileBlock.data + offsetInBlock, bytes_to_read_in_block);
    bytes_read += bytes_to_read_in_block;

    block_to_skip++;

    // Read in the internal inode block
    ret = readFromInternalInode(f, &block_to_read, size, data, block_to_skip,
                                &bytes_read);
    if (ret < 0)
      return ret;
    int nextInodeBlock = ffb->inode_block[MaxInodeInFFB - 1];
    // Read the remaining externel block if presents
    ret = readFromExternalInode(disk, &block_to_read, size, data,
                                nextInodeBlock, &bytes_read);
    if (ret < 0)
      return ret;

  } else if (f->pos_in_block_type == ExternalInodeBlock) {
    // CASO 3:

    int data_in_External_inode =
        pos_start - MaxDataInFFB - ((MaxInodeInFFB - 1) * MaxDataInBlock);

    int data_in_External_inode_blocks = data_in_External_inode / MaxDataInBlock;
    int allocated_inode = data_in_External_inode_blocks / (MaxElemInBlock - 1);
    // Absolute offset in external Inode
    int absOffsetBlock =
        pos_start - MaxDataInFFB - ((MaxInodeInFFB - 1) * MaxDataInBlock) -
        (allocated_inode * MaxDataInBlock * (MaxElemInBlock - 1));

    int offsetBlock = absOffsetBlock / MaxDataInBlock;
    int offsetInBlock = absOffsetBlock % MaxDataInBlock;

    int nextInodeBlock = ffb->inode_block[MaxInodeInFFB - 1];
    InodeBlock inodeBlock = {0};

    // Skip inode
    for (i = 0; i < allocated_inode; i++) {
      ret = DiskDriver_readBlock(disk, &inodeBlock, nextInodeBlock);
      if (ret < 0)
        return ret;
      nextInodeBlock = inodeBlock.inodeList[MaxElemInBlock - 1];
    }

    ret = DiskDriver_readBlock(disk, &inodeBlock, nextInodeBlock);
    if (ret < 0)
      return ret;

    int bytes_to_read_in_block = offsetInBlock + size > MaxDataInBlock
                                     ? MaxDataInBlock - offsetInBlock
                                     : size;

    int block_to_read =
        (size - bytes_to_read_in_block + MaxDataInBlock - 1) / MaxDataInBlock;

    // Read the data from blockToSkip with offset
    ret = DiskDriver_readBlock(disk, &fileBlock,
                               inodeBlock.inodeList[offsetBlock]);
    if (ret < 0)
      return ret;
    memcpy(data, fileBlock.data + offsetInBlock, bytes_to_read_in_block);

    bytes_read += bytes_to_read_in_block;

    offsetBlock++;

    // Read remaning fileBlock in the current external Inode
    ret = readDataFromExternalInode(disk, &inodeBlock, &block_to_read, size,
                                    data, offsetBlock, &bytes_read);
    if (ret < 0)
      return ret;
    nextInodeBlock = inodeBlock.inodeList[MaxElemInBlock - 1];
    // Read remaining External Inode if presents
    ret = readFromExternalInode(disk, &block_to_read, size, data,
                                nextInodeBlock, &bytes_read);
    if (ret < 0)
      return ret;
  }
  ret = SimpleFS_seek(f, pos_start + bytes_read);
  if (ret < 0)
    return ret;
  return bytes_read;
}

int SimpleFS_seek(FileHandle *f, int pos) {

  // offset outside the file
  if (pos > f->ffb->fcb.size_in_bytes) {
    return -1;
  }

  // Requested pos in the FFB
  if (pos < MaxDataInFFB) {
    f->pos_in_block = 1;
    f->pos_in_block_type = FFB;
    goto exit;
  }

  int askedBlock = (pos - MaxDataInFFB) / MaxDataInBlock;

  // Requested block is defined in the inodeblock in the FFB
  if (pos < MaxDataInFFB + ((MaxInodeInFFB - 1) * MaxDataInBlock)) {
    f->pos_in_block = askedBlock;
    f->pos_in_block_type = InodeBlockFFB;
    goto exit;
  }

  // Requested block is defined in the linked inodeblock
  f->pos_in_block = askedBlock;
  f->pos_in_block_type = ExternalInodeBlock;

exit:
  f->pos_in_file = pos;

  return 0;
}

// closes a file handle (destroyes it)
int SimpleFS_close(FileHandle *f) {

  free(f->ffb);
  free(f);
  return 0;
}
