#pragma once
#include "disk_driver.h"

#define MaxElemInBlock (BLOCK_SIZE/sizeof(int)) //Max integers in one block

#define MaxFilenameLen 128 //Max len for the name of files
#define MaxFileInDir (BLOCK_SIZE - sizeof(FileControlBlock) - (MaxInodeInFFB*sizeof(int)) -(sizeof(int))) / (sizeof(int)) //Max file in one First Directory Block
#define MaxDataInFFB (BLOCK_SIZE-sizeof(FileControlBlock)-(MaxInodeInFFB*sizeof(int))) //Max data in First File Block
#define MaxDataInBlock (BLOCK_SIZE/sizeof(char)) //Max char in one block
#define MaxInodeInFFB 32 //Max inode block in the First File Block

//FCB
typedef struct {
  int directory_block; // first block of the parent directory
  int block_in_disk;   // repeated position of the block on the disk
  char name[MaxFilenameLen];
  int  size_in_bytes;
  int size_in_blocks;
  int is_dir;          // 0 for file, 1 for dir
} FileControlBlock;

/******************* stuff on disk BEGIN *******************/
// FirstFileBlock: E' il primo blocco di un file. E' diviso in 3 parti:
// fcb che contiene tutte le info del file
// data che contiene dati opzionali
// inode_block che contiene gli indici della sequenza dei blocchi del file
typedef struct {
  FileControlBlock fcb;
  char data[MaxDataInFFB] ;
  int inode_block[MaxInodeInFFB]; //l'ultimo indice deve essere -1 o indice ad un inodeblock
} FirstFileBlock;

// InodeBlock: blocco contenente solo indici. L'ultimo elemento dell'array,
// è riservato al valore "-1" se il blocco termina, altrimenti avrà come valore
// un indice ad un altro InodeBlock(che conterrà la successiva sequenza dei blocchi del file)
typedef struct {
  int inodeList[BLOCK_SIZE/sizeof(int)];
} InodeBlock; 

//FileBlock: blocco contenente solo dati
typedef struct {
  char  data[BLOCK_SIZE];
} FileBlock;


// FirstDirectoryBlock: E' il primo blocco di una directory. E' suddiviso in:
// fcb che conterrà tutte le info del file(con un campo che ti dice se è un file o directory)
// num_entries che conterrà il numero degli elementi nella directory
// file_blocks è un array contenente gli indici dei blocchi dove sono contenuti i FirstFileBlock della directory
// inode_block è un array contenete gli indici dei DirectoryBlock 
typedef struct {
  FileControlBlock fcb;
  int num_entries;
  int file_blocks[MaxFileInDir];
  int inode_block[MaxInodeInFFB]; //l'ultimo indice deve essere -1 o indice ad un inodeblock
} FirstDirectoryBlock;

typedef struct {
  int file_blocks[ (BLOCK_SIZE)/sizeof(int) ];
} DirectoryBlock;
/******************* stuff on disk END *******************/

// Lista collegata che tiene traccia del percorso della directory
typedef struct DirChain {
  FirstDirectoryBlock* current;
  struct DirChain* prev;
} FdbChain;

typedef struct {
  DiskDriver* disk;
  FirstDirectoryBlock* fdb_current_dir;
  FirstDirectoryBlock* fdb_top_level_dir;
  FdbChain* fdb_chain;
} SimpleFS;

enum block_types {
  FFB,
  InodeBlockFFB,
  ExternalInodeBlock
};

// this is a file handle, used to refer to open files
typedef struct {
  SimpleFS* sfs;                   // pointer to memory file system structure
  FirstFileBlock* ffb;             // pointer to the first block of the file(read it)
  FirstDirectoryBlock* directory;  // pointer to the directory where the file is stored
  int pos_in_block;                // block where is present the cursor
  enum block_types pos_in_block_type;          // type of block of the cursor
  int pos_in_file;                 // position of the cursor
} FileHandle;

typedef struct {
  SimpleFS* sfs;                   // pointer to memory file system structure
  FirstDirectoryBlock* fdb;        // pointer to the first block of the directory(read it)
  FirstDirectoryBlock* directory;  // pointer to the parent directory (null if top level)
  int pos_in_dir;                  // absolute position of the cursor in the directory
  int pos_in_block;                // relative position of the cursor in the block
} DirectoryHandle;

// initializes a file system on an already made disk
// returns a handle to the top level directory stored in the first block
DirectoryHandle* SimpleFS_init(SimpleFS* fs, DiskDriver* disk);

// Unload the SimpleFS allocated
int SimpleFS_unload(SimpleFS* fs, DirectoryHandle * root);

// creates the inital structures, the top level directory
// has name "/" and its control block is in the first position
// it also clears the bitmap of occupied blocks on the disk
// the current_directory_block is cached in the SimpleFS struct
// and set to the top level directory
int SimpleFS_format(SimpleFS* fs);

// creates an empty file in the directory d
// returns null on error (file existing, no free blocks)
// an empty file consists only of a block of type FirstBlock
FileHandle* SimpleFS_createFile(DirectoryHandle* d, const char* filename);

// reads in the (preallocated) blocks array, the name of all files in a directory 
int SimpleFS_readDir(char** names, DirectoryHandle* d);


// opens a file in the  directory d. The file should be exisiting
FileHandle* SimpleFS_openFile(DirectoryHandle* d, const char* filename);


// closes a file handle (destroyes it)
int SimpleFS_close(FileHandle* f);

// writes in the file, at current position for size bytes stored in data
// overwriting and allocating new space if necessary
// returns the number of bytes written
int SimpleFS_write(FileHandle* f, void* data, int size);

// writes in the file, at current position size bytes stored in data
// overwriting and allocating new space if necessary
// returns the number of bytes read
int SimpleFS_read(FileHandle* f, void* data, int size);

// returns the number of bytes read (moving the current pointer to pos)
// returns pos on success
// -1 on error (file too short)
int SimpleFS_seek(FileHandle* f, int pos);

// seeks for a directory in d. If dirname is equal to ".." it goes one level up
// 0 on success, negative value on error
// it does side effect on the provided handle
 int SimpleFS_changeDir(DirectoryHandle* d, char* dirname);

// creates a new directory in the current one (stored in fs->current_directory_block)
// 0 on success
// -1 on error
int SimpleFS_mkDir(DirectoryHandle* d, char* dirname);

// removes the file in the current directory
// returns -1 on failure 0 on success
// if a directory, it removes recursively all contained files
int SimpleFS_remove(SimpleFS* fs, char* filename);


