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
}

// creates an empty file in the directory d
// returns null on error (file existing, no free blocks)
// an empty file consists only of a block of type FirstBlock
FileHandle *SimpleFS_createFileDir(DirectoryHandle *d, const char *filename,
                                   int is_dir) {

  // TODO: CONTROLLARE SE IL FILE GIA ESISTE NELLA CARTELLA

  FileHandle *file = calloc(1, sizeof(FileHandle));

  DiskDriver *disk = d->sfs->disk;
  int destBlock = DiskDriver_getFreeBlock(disk, 0);
  FirstFileBlock *ffb = calloc(1, sizeof(FirstFileBlock));
  ffb->fcb.directory_block = d->pos_in_block;
  ffb->fcb.block_in_disk = destBlock;
  memcpy(ffb->fcb.name, filename, strlen(filename));
  ffb->fcb.size_in_bytes = 0;
  ffb->fcb.size_in_blocks = 0;
  ffb->fcb.is_dir = is_dir;
  ffb->inode_block[MaxInodeInFFB - 1] = -1;

  DiskDriver_writeBlock(disk, ffb, destBlock);
  FirstDirectoryBlock *destDir = d->fdb;

  int free_block = 0;

  // CASO 1: provo a creare l'elemento nella porzione data dell'fdb
  int i, j;
  for (i = 0; i < MaxFileInDir; i++) {
    if (destDir->file_blocks[i] == 0) {
      destDir->file_blocks[i] = destBlock;
      // printf("ESCO DAL CASO 1 salvando il blocco %d con nome %s pos:%d\n",
      //  destBlock, filename, i);
      goto exit;
    }
  }

  // CASO 2: provo a creare l'elemento usando gli inode salvati nell'fdb (se
  // presenti)
  DirectoryBlock tmp_block;
  for (i = 0; i < 31; i++) {
    // controllo se l'inode block esiste
    if (destDir->inode_block[i] != 0) {
      DiskDriver_readBlock(disk, &tmp_block, destDir->inode_block[i]);
      for (j = 0; j < MaxElemInBlock; j++) {
        // E' STATO TROVATO UNA POSIZIONE LIBERA NEL DIRECTORY BLOCK
        if (tmp_block.file_blocks[j] == 0) {
          tmp_block.file_blocks[j] = destBlock;
          DiskDriver_writeBlock(disk, &tmp_block, destDir->inode_block[i]);
          // printf(
          // "ESCO DAL CASO 2 salvando il blocco %d con nome %s inode: %d %d
          // index: %d\n", destBlock, filename, i, destDir->inode_block[i], j);
          goto exit;
        }
      }
    } else {
      // l'inode è vuoto quindi viene allocato un directory block
      free_block = DiskDriver_getFreeBlock(disk, 0);
      memset(&tmp_block, 0, sizeof(DirectoryBlock));
      tmp_block.file_blocks[0] = destBlock;
      DiskDriver_writeBlock(disk, &tmp_block, free_block);
      // printf(
      // "ESCO DAL CASO 2 ALLOC salvando il blocco %d con nome %s inode: %d %d
      // blocco free: %d\n", destBlock, filename, i, destDir->inode_block[i],
      // free_block);
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
        DiskDriver_writeBlock(disk, &new_dir_block, num_block);
        tmp_inode_block.inodeList[i] = num_block;
        DiskDriver_writeBlock(disk, &tmp_inode_block, current_inode_block);
        // printf("ESCO DAL CASO 3.1\n");

        goto exit;
      }
    }
  }

  // Scorro tutti gli inode block collegati
  while (next_inode_block != -1) {
    current_inode_block = next_inode_block;
    DiskDriver_readBlock(disk, &tmp_inode_block, current_inode_block);

    // Controllo tutti i directoryblock nell'inodeblock
    for (i = 0; i < MaxElemInBlock - 1; i++) {
      if (tmp_inode_block.inodeList[i] != 0) {
        DiskDriver_readBlock(disk, &tmp_block, tmp_inode_block.inodeList[i]);

        // Controllo tutti gli elementi nel directory block
        for (j = 0; j < MaxElemInBlock; j++) {
          // E' STATO TROVATO UNA POSIZIONE LIBERA NEL DIRECTORY BLOCK
          if (tmp_block.file_blocks[j] == 0) {
            tmp_block.file_blocks[j] = destBlock;
            DiskDriver_writeBlock(disk, &tmp_block,
                                  tmp_inode_block.inodeList[i]);
            // printf("ESCO DAL CASO 2.1 salvando il blocco %d con nome %s
            // inode: %d index: %d \n", destBlock, filename, i, j);
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
  DiskDriver_writeBlock(disk, &new_dir_block, num_block);
  new_inode_block.inodeList[0] = num_block;
  new_inode_block.inodeList[MaxElemInBlock - 1] = -1;
  num_block = DiskDriver_getFreeBlock(disk, 0);
  DiskDriver_writeBlock(disk, &new_inode_block, num_block);
  // printf("ESCO DAL CASO 3.2\n");
  // COLLEGO UN INODEBLOCK CON UN ALTRO INODEBLOCK
  if (current_inode_block != -1) {
    tmp_inode_block.inodeList[MaxElemInBlock - 1] = num_block;
    DiskDriver_writeBlock(disk, &tmp_inode_block, current_inode_block);
  } else {
    // ALLOCO IL PRIMO INODEBLOCK COLLEGATO AL FIRST DIRECTORY BLOCK
    // printf("sto allocando un inodeblock: %d\n", num_block);
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
  DiskDriver_writeBlock(disk, destDir, d->pos_in_block);

  return file;
}

FileHandle *SimpleFS_createFile(DirectoryHandle *d, const char *filename) {

  return SimpleFS_createFileDir(d, filename, 0);
}

// writes in the file, at current position for size bytes stored in data
// overwriting and allocating new space if necessary
// returns the number of bytes written
int SimpleFS_write(FileHandle *f, void *data, int size) {
  int written_bytes = 0;
  int ret;
  FirstFileBlock *ffb = f->ffb;
  DiskDriver *disk = f->sfs->disk;
  // Se è maggiore del massimo numero di dati nel firstFileBlock devi usare gli
  // inode Altrimenti direttamente nel FFB
  ffb->inode_block[MaxInodeInFFB - 1] = -1;

  // Dati entrano nel FFB
  if (size <= MaxDataInFFB) {

    memcpy(ffb->data, data, size);
    ffb->fcb.size_in_bytes = size;
    ffb->fcb.size_in_blocks = 1;

    DiskDriver_writeBlock(disk, ffb, ffb->fcb.block_in_disk);

    return size;
  }

  int num_blocks = ((size - MaxDataInFFB) + BLOCK_SIZE - 1) / BLOCK_SIZE;

  memcpy(ffb->data, data, MaxDataInFFB);
  ffb->fcb.size_in_bytes = size;
  ffb->fcb.size_in_blocks = num_blocks + 1;
  written_bytes += MaxDataInFFB;

  int i;
  char *tmp_buf;
  printf("Num_blocks :%d\n Written_bytes: %d\n", num_blocks, written_bytes);
  // I blocchi da scrivere necessari entrano in un FFB
  for (i = 0; i < MaxInodeInFFB -1 && num_blocks > 0; i++) {
    int freeBlock = DiskDriver_getFreeBlock(disk, 0);
    num_blocks--;
    if (num_blocks == 0) {
      tmp_buf = calloc(MaxDataInBlock, sizeof(char));
      memcpy(tmp_buf, data + written_bytes, size - written_bytes);
      written_bytes += size - written_bytes;
    } else {
      tmp_buf = data + written_bytes;
      written_bytes += MaxDataInBlock;
    }
    DiskDriver_writeBlock(disk, tmp_buf, freeBlock);
    ffb->inode_block[i] = freeBlock;
    // printf("Num_block :%d \t Written_bytes: %d\n", num_blocks, written_bytes);
  }

  InodeBlock LastInodeBlock;
  int lastIndex;
  while (num_blocks > 0) {
    // Caso 3 i blocchi da scrivere hanno bisogno di più di 31 inode

    int new_free_block = DiskDriver_getFreeBlock(disk, 0);
    InodeBlock new_inode_block = {0};

    for (i = 0; i < MaxElemInBlock && num_blocks > 0; i++) {
      int freeBlock = DiskDriver_getFreeBlock(disk, 0);

      num_blocks--;

      if (num_blocks == 0) {
        tmp_buf = calloc(MaxDataInBlock, sizeof(char));
        memcpy(tmp_buf, data + written_bytes, size - written_bytes);
        written_bytes += size - written_bytes;
      } else {
        tmp_buf = data + written_bytes;
        written_bytes += MaxDataInBlock;
      }
      DiskDriver_writeBlock(disk, tmp_buf, freeBlock);
      new_inode_block.inodeList[i] = freeBlock;
    }
    // FirstInodeBlock need to link the inodeblock to ffb
    if (ffb->inode_block[31] == -1) {
      ffb->inode_block[31] = new_free_block;
    } else {
      DiskDriver_readBlock(disk, &LastInodeBlock, lastIndex);
      LastInodeBlock.inodeList[MaxElemInBlock - 1] = new_free_block;
      DiskDriver_writeBlock(disk, &LastInodeBlock, lastIndex);
    }
    new_inode_block.inodeList[MaxElemInBlock-1] = -1;
    DiskDriver_writeBlock(disk, &new_inode_block, new_free_block);
        printf("last index : %d new_free_block: %d\n",lastIndex,new_free_block);

    lastIndex = new_free_block;
  }
  
  DiskDriver_writeBlock(disk, ffb, ffb->fcb.block_in_disk);

  return written_bytes;
}

int DeleteStoredFile(DiskDriver *disk, FirstFileBlock *file) {
  int blocksToDel = file->fcb.size_in_blocks;
  int inodeIndex;

  // File is stored only in the ffb, skip other clean
  if (blocksToDel > 1)
    goto exit;

  // Clear the blocks stored in inode block array
  for (inodeIndex = 0; inodeIndex < MaxInodeInFFB - 1 && blocksToDel > 0;
       inodeIndex++, blocksToDel--) {
    DiskDriver_freeBlock(disk, file->inode_block[inodeIndex]);
  }

  // Check if we have more than 30 inode block
  int nextInodeBlock = file->inode_block[MaxInodeInFFB - 1];

  // Loop to clear all blocks declared in inod blocks
  while (nextInodeBlock != -1) {
    InodeBlock indexBlock;
    DiskDriver_readBlock(disk, &indexBlock, nextInodeBlock);
    for (inodeIndex = 0; inodeIndex < MaxElemInBlock - 1 && blocksToDel > 0;
         inodeIndex++, blocksToDel--) {

      DiskDriver_freeBlock(disk, indexBlock.inodeList[inodeIndex]);
    }

    // Clear the inode block
    DiskDriver_freeBlock(disk, nextInodeBlock);
    nextInodeBlock = indexBlock.inodeList[MaxElemInBlock - 1];
  }

exit:
  // Finally clear th ffb
  DiskDriver_freeBlock(disk, file->fcb.block_in_disk);

  return 0;
}

int DeleteStoredDir(DiskDriver *disk, FirstDirectoryBlock *dir) {
  FirstFileBlock *file_in_dir = malloc(sizeof(FirstFileBlock));

  int entryToDel = dir->num_entries;
  int inodeIndex;

  int i, j, block;

  for (i = 0; i < MaxFileInDir && entryToDel > 0; i++) {
    block = dir->file_blocks[i];
    if (block) {
      entryToDel--;
      // We load the block alyeas as a file
      // The fdb has the fcb as first struct elemen so this is not a problem
      // This is casted to a fdb if is_dir is true in fcb
      DiskDriver_readBlock(disk, file_in_dir, block);
      FileControlBlock fcb = file_in_dir->fcb;
      if (fcb.is_dir) {
        DeleteStoredDir(disk, (FirstDirectoryBlock *)file_in_dir);
      } else {
        DeleteStoredFile(disk, file_in_dir);
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
      DiskDriver_readBlock(disk, &dirBlock, block);
      // Check the directoryblock
      for (j = 0; j < MaxElemInBlock && entryToDel > 0; j++) {

        block = dirBlock.file_blocks[j];
        if (block) {
          entryToDel--;
          DiskDriver_readBlock(disk, file_in_dir, block);
          FileControlBlock fcb = file_in_dir->fcb;

          // Actually search the file
          if (fcb.is_dir) {
            DeleteStoredDir(disk, (FirstDirectoryBlock *)file_in_dir);
          } else {
            DeleteStoredFile(disk, file_in_dir);
          }
        }
      }
      // Clear the directory blocks
      DiskDriver_freeBlock(disk, block);
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
    DiskDriver_readBlock(disk, &inodeBlock, nextInodeBlock);
    for (inodeIndex = 0; inodeIndex < MaxElemInBlock - 1; inodeIndex++) {

      block = inodeBlock.inodeList[i];
      if (block) {
        DiskDriver_readBlock(disk, &dirBlock, block);

        for (j = 0; j < MaxElemInBlock && entryToDel > 0; j++) {
          block = dirBlock.file_blocks[j];

          if (block) {
            entryToDel--;
            DiskDriver_readBlock(disk, file_in_dir, block);
            FileControlBlock fcb = file_in_dir->fcb;

            // Actually search the file
            if (fcb.is_dir) {
              DeleteStoredDir(disk, (FirstDirectoryBlock *)file_in_dir);
            } else {
              DeleteStoredFile(disk, file_in_dir);
            }
          }
        }

        // Clear the directory blocks
        DiskDriver_freeBlock(disk, block);
      }

      // Clear the inode block
      DiskDriver_freeBlock(disk, nextInodeBlock);
      nextInodeBlock = inodeBlock.inodeList[MaxElemInBlock - 1];
    }
  }
exit:

  DiskDriver_freeBlock(disk, dir->fcb.block_in_disk);
  free(file_in_dir);

  return 0;
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

  int i, block, inodeBlockNum, dirBlockNum;

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
      if (ret)
        return handle_error("Errore ret", ret);
      FileControlBlock fcb = file->fcb;

      // Actually search the file
      if (!memcmp(name, fcb.name, sizeof(char) * MaxFilenameLen)) {
        if (fcb.is_dir) {
          ret = DeleteStoredDir(disk, (FirstDirectoryBlock *)file);
        } else {
          ret = DeleteStoredFile(disk, file);
        }

        // Setto l'indice vuoto nel file_blocks del fdb
        if (!ret) {
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
      DiskDriver_readBlock(disk, &dirBlock, inodeBlockNum);
      // Check the directoryblock
      for (dirBlockNum = 0; dirBlockNum < MaxElemInBlock && entries > 0;
           dirBlockNum++) {

        block = dirBlock.file_blocks[dirBlockNum];
        if (block) {
          entries--;
          DiskDriver_readBlock(disk, file, block);
          FileControlBlock fcb = file->fcb;

          // Actually search the file
          if (!memcmp(name, fcb.name, sizeof(char) * MaxFilenameLen)) {
            if (fcb.is_dir) {
              ret = DeleteStoredDir(disk, (FirstDirectoryBlock *)file);
            } else {
              ret = DeleteStoredFile(disk, file);
            }

            // Update the dirblock linked from the inodeBlockNum
            if (!ret) {
              dirBlock.file_blocks[dirBlockNum] = 0;
              // Check if the block is empty and dealloc if needed
              if (CheckEmptyDirBlock(dirBlock)) {
                DiskDriver_freeBlock(disk, inodeBlockNum);
                dir->inode_block[i] = 0;
              } else {
                DiskDriver_writeBlock(disk, &dirBlock, inodeBlockNum);
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

    DiskDriver_readBlock(disk, &inodeBlock, nextInodeBlock);
    for (inodeIndex = 0; inodeIndex < MaxElemInBlock - 1; inodeIndex++) {

      inodeBlockNum = inodeBlock.inodeList[i];
      if (inodeBlockNum) {
        DiskDriver_readBlock(disk, &dirBlock, inodeBlockNum);

        for (dirBlockNum = 0; dirBlockNum < MaxElemInBlock && entries > 0;
             dirBlockNum++) {
          block = dirBlock.file_blocks[dirBlockNum];

          if (block) {
            entries--;
            DiskDriver_readBlock(disk, file, block);
            FileControlBlock fcb = file->fcb;

            // Actually search the file
            if (!memcmp(name, fcb.name, sizeof(char) * MaxFilenameLen)) {
              if (fcb.is_dir) {
                ret = DeleteStoredDir(disk, (FirstDirectoryBlock *)file);
              } else {
                ret = DeleteStoredFile(disk, file);
              }

              if (!ret) {
                dirBlock.file_blocks[dirBlockNum] = 0;
                DiskDriver_writeBlock(disk, &dirBlock, inodeBlockNum);

                if (CheckEmptyDirBlock(dirBlock)) {
                  DiskDriver_freeBlock(disk, inodeBlockNum);
                  inodeBlock.inodeList[i] = 0;
                  DiskDriver_writeBlock(disk, &inodeBlock, nextInodeBlock);
                } else {
                  DiskDriver_writeBlock(disk, &dirBlock, inodeBlockNum);
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
  dir->num_entries--;
  DiskDriver_writeBlock(disk, dir, dir->fcb.block_in_disk);

  free(file);
  free(name);

  return ret;
}

// creates a new directory in the current one (stored in
// fs->current_directory_block) 0 on success -1 on error
int SimpleFS_mkDir(DirectoryHandle *d, char *dirname) {

  FileHandle *file = SimpleFS_createFileDir(d, dirname, 1);

  if (file) {
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
      free(d->sfs->fdb_chain);
      d->sfs->fdb_chain = prev;
      d->sfs->fdb_current_dir = d->fdb;
      return 0;
    }
    return -1;
  }

  // FirstDirectoryBlock *parent = d->directory;

  DiskDriver *disk = d->sfs->disk;
  FirstDirectoryBlock *dir = d->fdb;

  int entries = dir->num_entries;
  int i, block, j;

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
      DiskDriver_readBlock(disk, file, block);
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
    goto fail;

  DirectoryBlock dirBlock;
  // Search in the first inode block in ffb
  for (i = 0; i < MaxInodeInFFB - 1; i++) {
    block = dir->inode_block[i];

    // Check if inode block exist
    if (block) {
      // Read the directoryblock linked by the inode
      DiskDriver_readBlock(disk, &dirBlock, block);
      // Check the directoryblock
      for (j = 0; j < MaxElemInBlock && entries > 0; j++) {

        block = dirBlock.file_blocks[j];
        if (block) {
          entries--;

          DiskDriver_readBlock(disk, file, block);
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
    goto fail;

  // Check if we have more than 30 inode block
  int nextInodeBlock = dir->inode_block[MaxInodeInFFB - 1], inodeIndex;
  InodeBlock inodeBlock;

  // Loop to clear all blocks declared in inod blocks
  while (nextInodeBlock != -1) {

    DiskDriver_readBlock(disk, &inodeBlock, nextInodeBlock);
    for (inodeIndex = 0; inodeIndex < MaxElemInBlock - 1; inodeIndex++) {

      block = inodeBlock.inodeList[i];
      if (block) {
        DiskDriver_readBlock(disk, &dirBlock, block);

        for (j = 0; j < MaxElemInBlock && entries > 0; j++) {
          block = dirBlock.file_blocks[j];

          if (block) {
            entries--;
            DiskDriver_readBlock(disk, file, block);
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

fail:

  free(name);
  free(file);

  return -1;

exit:

  // if (parent && parent != d->sfs->fdb_top_level_dir) {
  //   free(parent);
  // }

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
  int i, block;
  // Search in the first file_blocks
  // printf("entro nel for\n");

  // Cerco il file nel file_blocks del fdb
  for (i = 0; i < MaxFileInDir && searched < entries; i++, searched++) {
    block = dir->file_blocks[i];

    // We load the block alyeas as a file
    // The fdb has the fcb as first struct elemen so this is not a problem
    // This is casted to a fdb if is_dir is true in fcb
    DiskDriver_readBlock(disk, file, block);
    FileControlBlock fcb = file->fcb;
    // printf("name: %s fcb.name: %s blocco %d\n", name, fcb.name, block);
    // Actually search the file
    if (!memcmp(name, fcb.name, sizeof(char) * MaxFilenameLen)) {
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

  // Cerco il file nei directory block negli inode definiti nel fdb
  int inodeIndex;
  for (inodeIndex = 0; inodeIndex < MaxInodeInFFB - 1; inodeIndex++) {

    int dir_block = dir->inode_block[inodeIndex];
    if (dir_block) {
      DirectoryBlock tmp_block;
      DiskDriver_readBlock(disk, &tmp_block, dir->inode_block[inodeIndex]);

      for (i = 0; i < MaxElemInBlock && searched < entries; i++, searched++) {
        block = tmp_block.file_blocks[i];

        // We load the block alyeas as a file
        // The fdb has the fcb as first struct elemen so this is not a problem
        // This is casted to a fdb if is_dir is true in fcb
        DiskDriver_readBlock(disk, file, block);
        FileControlBlock fcb = file->fcb;
        // printf("name: %s fcb.name: %s blocco %d\n", name, fcb.name, block);
        // Actually search the file
        if (!memcmp(name, fcb.name, sizeof(char) * MaxFilenameLen)) {
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
  }

  // printf("dopo il for e quindi > 30 inode blcok\n");
  // Check if we have more than 30 inode block
  int nextInodeBlock = dir->inode_block[MaxInodeInFFB - 1];
  // Loop to clear all blocks declared in inod blocks
  while (nextInodeBlock != -1) {
    InodeBlock indexBlock;
    DiskDriver_readBlock(disk, &indexBlock, nextInodeBlock);
    for (inodeIndex = 0; inodeIndex < MaxElemInBlock - 1; inodeIndex++) {
      if (indexBlock.inodeList[inodeIndex] == 0) {
        return NULL;
      }
      DirectoryBlock tmp_dir;
      DiskDriver_readBlock(disk, &tmp_dir, indexBlock.inodeList[inodeIndex]);

      for (i = 0; i < MaxElemInBlock && searched < entries; i++, searched++) {
        block = tmp_dir.file_blocks[i];

        DiskDriver_readBlock(disk, file, block);
        FileControlBlock fcb = file->fcb;

        // printf("file numero: %d %s in block %d\n", entries, fcb.name, block);
        // Actually search the file
        if (!memcmp(name, fcb.name, sizeof(char) * MaxFilenameLen)) {
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

    nextInodeBlock = indexBlock.inodeList[MaxElemInBlock - 1];
  }
  return NULL;
}

int SimpleFS_readDir(char **names, DirectoryHandle *d) {

  int entries = d->fdb->num_entries;
  int i, inodeBlockNum, j, found = 0;
  int block;
  FirstFileBlock file;
  DiskDriver *disk = d->sfs->disk;
  FirstDirectoryBlock *dir = d->fdb;

  for (i = 0; i < MaxFileInDir && found < entries; i++) {
    // if (names[i]) {
    block = d->fdb->file_blocks[i];
    if (block) {
      // printf("indice i: %d \t found: %d\n", i,found);
      DiskDriver_readBlock(disk, &file, block);
      names[found] = malloc(MaxFilenameLen);
      memcpy(names[found], file.fcb.name, MaxFilenameLen);
      found++;
      // printf("indice i: %d \t found: %d\n", i,found);
    }
  }
  // }

  DirectoryBlock dirBlock;

  // Search in the first inode block in ffb
  for (i = 0; i < MaxInodeInFFB - 1; i++) {
    inodeBlockNum = dir->inode_block[i];

    // Check if inode block exist
    if (inodeBlockNum) {
      // Read the directoryblock linked by the inode
      DiskDriver_readBlock(disk, &dirBlock, inodeBlockNum);
      // Check the directoryblock
      for (j = 0; j < MaxElemInBlock && found < entries; j++) {

        block = dirBlock.file_blocks[j];
        if (block) {
          DiskDriver_readBlock(disk, &file, block);
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

    DiskDriver_readBlock(disk, &inodeBlock, nextInodeBlock);
    for (inodeIndex = 0; inodeIndex < MaxElemInBlock - 1; inodeIndex++) {

      inodeBlockNum = inodeBlock.inodeList[i];
      if (inodeBlockNum) {
        DiskDriver_readBlock(disk, &dirBlock, inodeBlockNum);

        for (j = 0; j < MaxElemInBlock && found < entries; j++) {
          block = dirBlock.file_blocks[j];

          if (block) {
            DiskDriver_readBlock(disk, &file, block);
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

int SimpleFS_read(FileHandle *f, void *data, int size) {

  DiskDriver *disk = f->sfs->disk;
  int bytes_read = 0;
  int i, inodeBlockNum;
  FirstFileBlock *ffb = f->ffb;
  int block_to_read;
  if (size < MaxDataInFFB) {
    memcpy(data, ffb->data, size);
    return size;
  }

  memcpy(data, ffb->data, MaxDataInFFB);
  bytes_read += MaxDataInFFB;

  FileBlock fileBlock;
  block_to_read = (size - bytes_read + MaxDataInBlock - 1) / MaxDataInBlock;
  printf("block to read %d\n", block_to_read);

  // Search in the first inode block in ffb
  for (i = 0; i < MaxInodeInFFB - 1 && block_to_read > 0; i++) {
    inodeBlockNum = ffb->inode_block[i];

    // Check if inode block exist
    if (inodeBlockNum) {
      // Read the directoryblock linked by the inode
      DiskDriver_readBlock(disk, &fileBlock, inodeBlockNum);
      block_to_read--;
      if (block_to_read > 0) {
        memcpy(data + bytes_read, &fileBlock, MaxDataInBlock);
        bytes_read += MaxDataInBlock;
      } else {
        memcpy(data + bytes_read, &fileBlock, size - bytes_read);
        bytes_read += size - bytes_read;
      }
    }
  }

  // Check if we have more than 30 inode block
  int nextInodeBlock = ffb->inode_block[MaxInodeInFFB - 1], inodeIndex;
  InodeBlock inodeBlock;

  // Loop to clear all blocks declared in inod blocks
  while (nextInodeBlock != -1) {

    DiskDriver_readBlock(disk, &inodeBlock, nextInodeBlock);
    for (inodeIndex = 0; inodeIndex < MaxElemInBlock - 1 && block_to_read > 0; inodeIndex++) {
      inodeBlockNum = inodeBlock.inodeList[inodeIndex];
      if (inodeBlockNum) {
        // Read the directoryblock linked by the inode
        DiskDriver_readBlock(disk, &fileBlock, inodeBlock.inodeList[inodeIndex]);
        block_to_read--;
        if (block_to_read > 0) {
          memcpy(data + bytes_read, &fileBlock, MaxDataInBlock);
          bytes_read += MaxDataInBlock;
        } else {
          memcpy(data + bytes_read, &fileBlock, size - bytes_read);
          bytes_read += size - bytes_read;
        }
          // printf("Num_blocks :%d Written_bytes: %d inodeindex: %d InodeBlockNum: %d\n", block_to_read,bytes_read,inodeIndex, inodeBlockNum );
      }
    }
    printf("last index : %d\n",nextInodeBlock);
    nextInodeBlock = inodeBlock.inodeList[MaxElemInBlock - 1];
    
  }
  return bytes_read;
}
