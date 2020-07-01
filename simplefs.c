#include "simplefs.h"
#include "utils.h"
#include <string.h>
#include <stdlib.h>

// creates the inital structures, the top level directory
// has name "/" and its control block is in the first position
// it also clears the bitmap of occupied blocks on the disk
// the current_directory_block is cached in the SimpleFS struct
// and set to the top level directory
int SimpleFS_format(SimpleFS* fs){

    FirstDirectoryBlock root = {0};
    char* tmp = "/";
    memcpy(root.fcb.name, tmp,strlen(tmp));
    root.fcb.is_dir = 1;
    int ret;
    ret = DiskDriver_writeBlock(fs->disk,&root,0);
    if(ret) return handle_error("Error in format: ", ret);
    return 0;

}


// initializes a file system on an already made disk
// returns a handle to the top level directory stored in the first block
DirectoryHandle* SimpleFS_init(SimpleFS* fs, DiskDriver* disk){

    FirstDirectoryBlock* firstDir = calloc(1,sizeof(FirstDirectoryBlock));
    DiskDriver_readBlock(fs->disk, firstDir,0);

    DirectoryHandle* dir = (DirectoryHandle*) malloc(sizeof(DirectoryHandle));
    dir->sfs = fs;
    dir->dcb = firstDir;
    dir->directory = NULL;
    dir->pos_in_block = 0;
    dir->pos_in_dir = 0;

    return dir;

}

// creates an empty file in the directory d
// returns null on error (file existing, no free blocks)
// an empty file consists only of a block of type FirstBlock
FileHandle* SimpleFS_createFile(DirectoryHandle* d, const char* filename){

    DiskDriver* disk = d->sfs->disk;
    int destBlock = DiskDriver_getFreeBlock(disk,0);
    printf("destBlock: %d\n", destBlock);
    FirstFileBlock* ffb = calloc(1,sizeof(FirstFileBlock));
    ffb->fcb.directory_block = d->pos_in_block;
    ffb->fcb.block_in_disk = destBlock;
    memcpy(ffb->fcb.name, filename, strlen(filename));
    ffb->fcb.size_in_bytes = 0;
    ffb->fcb.size_in_blocks = 0;
    ffb->fcb.is_dir = 0;

    DiskDriver_writeBlock(disk,ffb,destBlock);
    // TODO: quanti elementi abbiamo nel FirstDirectoryBlock?
    FirstDirectoryBlock* destDir = d->dcb;
    destDir->file_blocks[destDir->num_entries] = destBlock;
    destDir->num_entries++;
    DiskDriver_writeBlock(disk,destDir,d->pos_in_block);

    FileHandle* file = calloc(1,sizeof(FileHandle));

    file->sfs = d->sfs;
    file->fcb = ffb;
    file->directory = destDir;
    file->pos_in_file = 0;

    return file;

}
