#pragma once
#include <stdint.h>
typedef struct{
  int num_bits;
  char* entries;
}  BitMap;

typedef struct {
  int entry_num;
  char bit_num;
} BitMapEntryKey;

// converts a block index to an index in the array,
// and a char that indicates the offset of the bit inside the array
BitMapEntryKey BitMap_blockToIndex(int num);

// converts a bit to a linear index
// Converte l'indice nella bitmap (entry) nell'offset del fd rappresentate il nostro disco (contenuto in un file statico)
int BitMap_indexToBlock(int entry, uint8_t bit_num);

// returns the index of the first bit having status "status"
// in the bitmap bmap, and starts looking from position start
// Da quanto abbiamo capito questo serve per trovare il primo blocco libero all'interno della bitmap
int BitMap_get(BitMap* bmap, int start, int status);

// sets the bit at index pos in bmap to status
// Setta il blocco in posizione pos libero o occupato (status da quanto abbiamo capito viene utilizzato per settare il blocco con dati o sovrascrivibile(senza dati))
int BitMap_set(BitMap* bmap, int pos, int status);
