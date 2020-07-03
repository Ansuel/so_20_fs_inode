#include "disk_driver.h"
#include "utils.h"
#include <assert.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

static int indexToOffset(DiskDriver *disk, int index) {
  return (index + disk->reserved_blocks) * BLOCK_SIZE;
}

int DiskDriver_init(DiskDriver *disk, const char *filename, int num_blocks) {
  struct stat statbuf;
  DiskHeader header;
  int ret;

  int fd = open(filename, O_CREAT | O_RDWR, 0777);

  ret = fstat(fd, &statbuf);
  if (ret)
    return handle_error("Error fstate: ", ret);

  // Fs is not initialized
  if (!statbuf.st_size) {
    ret = ftruncate(fd, num_blocks * BLOCK_SIZE);
    if (ret)
      return handle_error("Error ftruncate: ", ret);

    // Compile header in the fd
    header.num_blocks = num_blocks;
    if (MaxBitMapEntryInBlock >= num_blocks) {
      header.bitmap_blocks = 1;
    } else {
      header.bitmap_blocks =
          (num_blocks + MaxBitMapEntryInBlock - 1) / MaxBitMapEntryInBlock;
    }

    header.bitmap_entries = num_blocks - 1 - header.bitmap_blocks;
    header.free_blocks = num_blocks - 1 - header.bitmap_blocks;
    header.first_free_block = 1 + header.bitmap_blocks;

    write(fd, &header, sizeof(DiskHeader));
  }

  disk->header =
      mmap(NULL, sizeof(DiskHeader), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

  if (disk->header == MAP_FAILED)
    return handle_error("Error mmap: ", -1);

  disk->bitmap_data = mmap(NULL, sizeof(bitmapEntry) * (disk->header->bitmap_entries),
                           PROT_READ | PROT_WRITE, MAP_SHARED, fd, BLOCK_SIZE);
  if (disk->bitmap_data == MAP_FAILED)
    return handle_error("Error mmap: ", -1);

  disk->reserved_blocks = disk->header->bitmap_blocks + 1;
  disk->fd = fd;

  return ret;
};

int DiskDriver_writeBlock(DiskDriver *disk, void *src, int block_num) {
  // char *tmp;
  int ret;
  assert(disk);

  // Can't go outside the disk size
  if (block_num > disk->header->num_blocks)
    return -1;

  // printf("Offset Calc %d\n",indexToOffset(disk, block_num));

  ret = lseek(disk->fd, indexToOffset(disk, block_num), SEEK_SET);
  if (ret < 0)
    return handle_error("Error lseek: ", ret);

  // Allocate empty buf to fill a block size
  // tmp = calloc(BLOCK_SIZE, sizeof(char));
  // memcpy(tmp, src, strlen(src));

  ret = write(disk->fd, src, BLOCK_SIZE);
  // free(tmp);

  if (ret < 0)
    return handle_error("Error write: ", ret);

  disk->bitmap_data[block_num].used = 1;

  return 0;
}

int DiskDriver_readBlock(DiskDriver *disk, void *dest, int block_num) {
  int ret;

  // Disk must be present
  assert(disk);

  // Can't go outside the disk size
  if (block_num > disk->header->num_blocks)
    return -1;

  // Can't read empty block
  if (!disk->bitmap_data[block_num].used)
    return -1;

  ret = lseek(disk->fd, indexToOffset(disk, block_num), SEEK_SET);
  if (ret < 0)
    return handle_error("Error lseek: ", ret);

  ret = read(disk->fd, dest, BLOCK_SIZE);
  if (ret < 0)
    return handle_error("Error read: ", ret);

  return 0;
}

int DiskDriver_freeBlock(DiskDriver *disk, int block_num) {

  assert(disk);

  // Can't write header or bitmap data or outside the disk size
  if (block_num > disk->header->num_blocks)
    return -1;

  // Block already empty
  if (!disk->bitmap_data[block_num].used)
    return -1;

  // ret = lseek(disk->fd, indexToOffset(disk, block_num), SEEK_SET);
  // if (ret < 0)
  //   return handle_error("Error lseek: ", ret);

  // Just set the block to be overwritable
  // The write will take care of empty the block if the data to be
  // written doesn't fill the entire block
  disk->bitmap_data[block_num].used = 0;

  return 0;
}

int DiskDriver_getFreeBlock(DiskDriver *disk, int start) {
  // Search the first empty block from start
  for (; start < disk->header->bitmap_entries; start++) {
    if (!disk->bitmap_data[start].used)
      return start;
  }

  // Disk is full
  return -1;
}

int DiskDriver_flush(DiskDriver *disk) {
  int ret;
  ret = munmap(disk->header, sizeof(DiskHeader *));
  if(ret) return handle_error("Error in munmap: ", ret); 
  ret = munmap(disk->bitmap_data, sizeof(char) * disk->header->bitmap_blocks);
  if(ret) return handle_error("Error in munmap: ", ret);

  return 0;
}
