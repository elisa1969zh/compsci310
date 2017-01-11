#include <stdio.h>  // needed for size_t
#include <unistd.h> // needed for sbrk
#include <assert.h> // needed for asserts
#include "dmm.h"

/* You can improve the below metadata structure using the concepts from Bryant
 * and OHallaron book (chapter 9).
 */

typedef struct metadata {
  /* size_t is the return type of the sizeof operator. Since the size of an
   * object depends on the architecture and its implementation, size_t is used
   * to represent the maximum size of any object in the particular
   * implementation. size contains the size of the data object or the number of
   * free bytes
   */
  size_t size;
  struct metadata* next;
  struct metadata* prev;
} metadata_t;

typedef struct footer {
  size_t size;
} footer_t;

#define FOOTER_T_ALIGNED (ALIGN(sizeof(footer_t)))
#define PACK(size, alloc) ((size) | (alloc))
#define GET_H(p) ((metadata_t *)(p))->size
#define GET_F(p) ((footer_t *)(p))->size
#define GET_SIZE_H(p) (GET_H(p) & ~0x7)
#define GET_ALLOC_H(p) (GET_H(p) & 0x1)
#define GET_SIZE_F(p) (GET_F(p) & ~0x7)
#define GET_ALLOC_F(p) (GET_F(p) & 0x1)
#define HDRP(bp) ((char*)(bp) - METADATA_T_ALIGNED)
#define FTRP(bp) ((char*)(bp) + GET_SIZE_H(HDRP(bp)))

/* freelist maintains all the blocks which are not in use; freelist is kept
 * sorted to improve coalescing efficiency 
 */

static metadata_t* freelist = NULL;

void* dmalloc(size_t numbytes) {
  /* initialize through sbrk call first time */
  if(freelist == NULL) {      
    if(!dmalloc_init())
      return NULL;
  }

  assert(numbytes > 0);

  /* your code here */
  metadata_t* freelistIterator = freelist;
  while(freelistIterator->next != NULL) {
    if(GET_SIZE_H(freelistIterator) >= ALIGN(numbytes)) {
      //If it is not worth splitting the block (need additional space for another header and footer and data) just allocate the block
      if((GET_SIZE_H(freelistIterator) - ALIGN(numbytes)) < (METADATA_T_ALIGNED + FOOTER_T_ALIGNED + ALIGNMENT)) {
        metadata_t* prevBlock = freelistIterator->prev;
        metadata_t* nextBlock = freelistIterator->next;
        prevBlock->next = nextBlock;
        nextBlock->prev= prevBlock;
        freelistIterator->size = PACK(GET_SIZE_H(freelistIterator), 1);
        footer_t* freelistIteratorF = (footer_t*) ((void*) freelistIterator + METADATA_T_ALIGNED + GET_SIZE_H(freelistIterator));
        freelistIteratorF->size = freelistIterator->size;
        return (void*) freelistIterator + METADATA_T_ALIGNED;
      }
      //Otherwise split
      else {
        metadata_t* prevBlock = freelistIterator->prev;
        metadata_t* nextBlock = freelistIterator->next;
        metadata_t* blockToAllocate = freelistIterator;
        //Update footer of new block
        footer_t* newBlockF = (footer_t*) ((void*) blockToAllocate + METADATA_T_ALIGNED + GET_SIZE_H(blockToAllocate));
        newBlockF->size = PACK(GET_SIZE_H(blockToAllocate) - ALIGN(numbytes) - FOOTER_T_ALIGNED - METADATA_T_ALIGNED, 0);
        //Update header of allocated block
        blockToAllocate->size = PACK(ALIGN(numbytes), 1);
        //Create footer of allocated block
        footer_t* blockToAllocateF = (footer_t*) ((void*) blockToAllocate + METADATA_T_ALIGNED + GET_SIZE_H(blockToAllocate));
        blockToAllocateF->size = blockToAllocate->size;
        //Create header of new block
        metadata_t* newBlock = (metadata_t*) ((void*) blockToAllocateF + FOOTER_T_ALIGNED);
        newBlock->prev = prevBlock;
        newBlock->next = nextBlock;
        newBlock->size = newBlockF->size;
        //Clean up pointers to the new block
        prevBlock->next = newBlock;
        nextBlock->prev = newBlock;
        return (void*) blockToAllocate + METADATA_T_ALIGNED;
      }
    }
    freelistIterator = freelistIterator->next;
  }
  
  return NULL;
}

void dfree(void* ptr) {
  /* your code here */
  int before, after;
  metadata_t* block = (metadata_t*) HDRP(ptr);
  footer_t* blockF = (footer_t*) FTRP(ptr);
  before = GET_ALLOC_F((void*) block - FOOTER_T_ALIGNED);
  after = GET_ALLOC_H((void*) blockF + FOOTER_T_ALIGNED);

  //Case 1: prev and next are both allocated
  if((before == 1) && (after == 1)) {
    metadata_t* freelistIterator = freelist;
    while(freelistIterator->next != NULL) {
      if((freelistIterator < block) && (freelistIterator->next > block)) {
        block->prev = freelistIterator;
        block->next = freelistIterator->next;  
        freelistIterator->next->prev = block;      
        freelistIterator->next = block;
        block->size = PACK(GET_SIZE_H(block), 0);
        blockF->size = PACK(GET_SIZE_H(block), 0);
        return;
      }
      freelistIterator = freelistIterator->next;
    }
  }
  //Case 2: prev is allocated but next is free
  else if((before == 1) && (after == 0)) {
    metadata_t* freelistIterator = freelist;
    while(freelistIterator->next != NULL) {
      if((freelistIterator < block) && (freelistIterator->next > block)) {
        metadata_t* curNext = freelistIterator->next;
        metadata_t* newNext = freelistIterator->next->next;
        size_t newSize = GET_SIZE_H(block) + GET_SIZE_H((void*) blockF + FOOTER_T_ALIGNED) + METADATA_T_ALIGNED + FOOTER_T_ALIGNED;
        block->size = PACK(newSize, 0);
        footer_t* nextF = (footer_t*) ((void*) curNext + METADATA_T_ALIGNED + GET_SIZE_H(curNext));
        nextF->size = block->size;
        freelistIterator->next = block;
        block->prev = freelistIterator;
        block->next = newNext;
        newNext->prev = block;
        return;
      }
      freelistIterator = freelistIterator->next;
    }
  }
  //Case 3: prev is free but next is allocated
  else if((before == 0) && (after == 1)) {
    metadata_t* freelistIterator = freelist;
    while(freelistIterator->next != NULL) {
      if((freelistIterator < block) && (freelistIterator->next > block)) {
        size_t newSize = GET_SIZE_H(block) + GET_SIZE_F((void*) block - FOOTER_T_ALIGNED) + METADATA_T_ALIGNED + FOOTER_T_ALIGNED;
        freelistIterator->size = PACK(newSize, 0);
        blockF->size = PACK(newSize, 0);
        return;
      }
      freelistIterator = freelistIterator->next;
    }
  }
  //Case 4: prev and next are both free
  else {
    metadata_t* freelistIterator = freelist;
    while(freelistIterator->next != NULL) {
      if((freelistIterator < block) && (freelistIterator->next > block)) {
        metadata_t* curNext = freelistIterator->next;
        metadata_t* newNext = freelistIterator->next->next;
        size_t newSize = GET_SIZE_H(block) + GET_SIZE_H((void*) blockF + FOOTER_T_ALIGNED) + GET_SIZE_F((void*) block - FOOTER_T_ALIGNED) + 2*METADATA_T_ALIGNED + 2*FOOTER_T_ALIGNED;
        freelistIterator->size = PACK(newSize, 0);
        footer_t* nextF = (footer_t*) ((void*) curNext + METADATA_T_ALIGNED + GET_SIZE_H(curNext));
        nextF->size = freelistIterator->size;
        freelistIterator->next = newNext;
        newNext->prev = freelistIterator;
        return;
      }
      freelistIterator = freelistIterator->next;
    }
  }
}

bool dmalloc_init() {

  /* Two choices: 
   * 1. Append prologue and epilogue blocks to the start and the
   * end of the freelist 
   *
   * 2. Initialize freelist pointers to NULL
   *
   * Note: We provide the code for 2. Using 1 will help you to tackle the 
   * corner cases succinctly.
   */

  size_t max_bytes = ALIGN(MAX_HEAP_SIZE);

  //Create prologue
  metadata_t* prologue = (metadata_t*) sbrk(max_bytes); //Header
  if(prologue == (void *)-1) //Not sure what this does...
    return false; //...
  prologue->prev = NULL;
  prologue->size = PACK(0, 1);
  footer_t* prologueF = (footer_t*) ((void*) prologue + METADATA_T_ALIGNED); //Footer
  prologueF->size = prologue->size;

  //Create freelist
  metadata_t* firstBlock = (metadata_t*) ((void*) prologueF + FOOTER_T_ALIGNED); //Need to cast?
  firstBlock->prev = prologue;
  firstBlock->size = PACK(max_bytes - 3*METADATA_T_ALIGNED - 2*FOOTER_T_ALIGNED, 0);
  footer_t* firstBlockF = (footer_t*) ((void*) firstBlock + METADATA_T_ALIGNED + GET_SIZE_H(firstBlock));
  firstBlockF->size = firstBlock->size;
  prologue->next = firstBlock;

  //Create epilogue
  metadata_t* epilogue = (metadata_t*) ((void*) prologue + max_bytes - METADATA_T_ALIGNED); //Header
  epilogue->next = NULL;
  epilogue->prev = firstBlock;
  epilogue->size = PACK(0, 1);
  firstBlock->next = epilogue;

  freelist = prologue;
  return true;
}

/* for debugging; can be turned off through -NDEBUG flag*/
void print_freelist() {
  metadata_t *freelist_head = freelist;
  while(freelist_head != NULL) {
    DEBUG("\tFreelist Size:%zd, Head:%p, Prev:%p, Next:%p\t",
    freelist_head->size,
    freelist_head,
    freelist_head->prev,
    freelist_head->next);
    freelist_head = freelist_head->next;
  }
  DEBUG("\n");
}
