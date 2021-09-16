#include <stdlib.h>
#include <assert.h>
#include <stdio.h>

#include "mem_alloc_types.h"
#include "mem_alloc_standard_pool.h"
#include "my_mmap.h"
#include "mem_alloc.h"

/////////////////////////////////////////////////////////////////////////////

#ifdef STDPOOL_POLICY
std_pool_placement_policy_t std_pool_policy = STDPOOL_POLICY;
#else
std_pool_placement_policy_t std_pool_policy = DEFAULT_STDPOOL_POLICY;
#endif
void print_block_info(void *payldAddr);
mem_pool_t *pTest = NULL;
/////////////////////////////////////////////////////////////////////////////

void init_standard_pool(mem_pool_t *p, size_t size, size_t min_request_size, size_t max_request_size)
{
  void *pool = my_mmap(size);
  p->start = pool;
  p->first_free = pool;
  p->end = pool + size;

  mem_std_free_block_t *p_std_free = (mem_std_free_block_t *)pool; //contains the header and the 2 pointers
  p_std_free->prev = NULL;
  p_std_free->next = NULL;

  size_t freesize = size - 2 * sizeof(mem_std_block_header_footer_t);
  set_block_free(&(p_std_free->header));
  set_block_size(&(p_std_free->header), freesize);

  mem_std_block_header_footer_t *p_std_footer = (mem_std_block_header_footer_t *)((void *)pool + size - sizeof(mem_std_block_header_footer_t));

  set_block_free(p_std_footer);
  set_block_size(p_std_footer, freesize);

  pTest = p;
  print_block_info(pool + 8);
}

size_t adjustPerAlignment(size_t size){
  size_t ret;
  if(size % MEM_ALIGNMENT == 0){
    ret = size;
  } else {
    ret = ( (size / MEM_ALIGNMENT) + 1 ) * MEM_ALIGNMENT;
  }
  return ret;
}

void *mem_alloc_standard_pool(mem_pool_t *pool, size_t size){
  size = adjustPerAlignment(size); // Memory alignment
  mem_std_free_block_t *p_block_to_split = (mem_std_free_block_t *)pool->first_free;
  size_t availSpace = get_block_size(&(p_block_to_split->header));
  
  while ((availSpace < size) && (p_block_to_split != NULL)){
    mem_std_free_block_t *temp = p_block_to_split->next;
    temp->prev = p_block_to_split;
    temp->next = p_block_to_split->next->next;
    p_block_to_split = temp;
    availSpace = get_block_size(&(p_block_to_split->header));
  }

  if (p_block_to_split == NULL){
    return NULL;
  }

  void *alloc_payload = (void *)((size_t)p_block_to_split + sizeof(mem_std_block_header_footer_t));
  
  char blkFullyConsumed = 0;
  int temp = (signed)availSpace - (signed)(size + 2 * sizeof(mem_std_block_header_footer_t));
  size_t new_free_payload_size = 0;
  if( temp <= (int)(4 * sizeof(mem_std_block_header_footer_t))){
    new_free_payload_size = 0;
    // not enough space in the current free block to create a new free block of non-zero size, 
    // so don't split the block, allocate whole and update the free list
    blkFullyConsumed = 1;
    size = availSpace;
  } else {
    new_free_payload_size = temp;
  }
  

  set_block_used(&p_block_to_split->header);
  set_block_size(&p_block_to_split->header, size);
  mem_std_block_header_footer_t *new_alloc_footer = (mem_std_block_header_footer_t *)((size_t)p_block_to_split + sizeof(mem_std_block_header_footer_t) + size);
  set_block_used(new_alloc_footer);
  set_block_size(new_alloc_footer, size);

  mem_std_free_block_t *new_free_block = NULL;
  if(blkFullyConsumed){ 
    // not enough space in the current free block to create a new free block of non-zero size, 
    // so don't split the block, allocate whole and update the free list
    if(p_block_to_split->prev == NULL){ // first one
      new_free_block = p_block_to_split->next;
      new_free_block->prev = NULL;
      // new_free_block->next = No need to consider
    } else if(p_block_to_split->next == NULL){ // last one
      new_free_block = p_block_to_split->prev;
      // new_free_block->prev = No need to consider
      new_free_block->next = NULL;
    } else { //  middle one
      new_free_block = p_block_to_split->next;
      new_free_block->prev = p_block_to_split->prev;
      // new_free_block->next = No need to consider
    }
  } else {
    set_block_size(&p_block_to_split->header, size);
    // split the block and create new free block within the current free block
    new_free_block = (void *)((size_t)p_block_to_split + (2 * sizeof(mem_std_block_header_footer_t)) + size);
    set_block_free(&new_free_block->header);
    set_block_size(&new_free_block->header, new_free_payload_size);

    mem_std_block_header_footer_t *new_free_block_footer = (void *)((size_t)new_free_block + sizeof(mem_std_block_header_footer_t) + new_free_payload_size);
    set_block_free(new_free_block_footer);
    set_block_size(new_free_block_footer, new_free_payload_size);

    // so free list doesn't change
    new_free_block->next = p_block_to_split->next;
    new_free_block->prev = p_block_to_split->prev;
    
  }
  
  if(new_free_block->prev == NULL){ 
    pool->first_free = new_free_block;
  }

  print_block_info(alloc_payload);
  for (size_t i = (size_t)alloc_payload; i < (size_t)alloc_payload + size; i++){
    *((char *)i) = 0xAA;
  }

  return alloc_payload;
}

void mem_free_standard_pool(mem_pool_t *pool, void *addr){

  void *header_addr = (void *)((size_t)addr - sizeof(mem_std_block_header_footer_t));
  size_t free_space = get_block_size(header_addr);
  set_block_free(header_addr);
  set_block_size(header_addr, free_space);
  void *footer_addr = (void *)((size_t)header_addr + sizeof(mem_std_block_header_footer_t) + free_space);
  set_block_free(footer_addr);
  set_block_size(footer_addr, free_space);

  mem_std_free_block_t *first_free = pool->first_free;
  
  mem_std_free_block_t *last_free = NULL;
  mem_std_free_block_t *temp = first_free;
  while (temp->next != NULL){
    temp = temp->next;
  }
  last_free = temp;


  mem_std_free_block_t *cur_free = header_addr;
  if (cur_free < first_free){
    pool->first_free = cur_free;
    cur_free->prev = NULL;
    cur_free->next = first_free;
    first_free->prev = cur_free;
    // first_free->next = No need to consider
  } else if (cur_free > last_free){
    cur_free->prev = last_free;
    cur_free->next = NULL;
    // last_free->prev = No need to consider
    last_free->next = cur_free;
  } else {
    // get free before current block
    assert(!(cur_free == first_free || cur_free == last_free)); // double free check
    mem_std_free_block_t *before_cur_free = first_free;
    while (before_cur_free->next <= cur_free){
      before_cur_free = before_cur_free->next;
    }

    // get free after current block
    mem_std_free_block_t *after_cur_free = last_free;
    while (after_cur_free->prev >= cur_free){
      after_cur_free = after_cur_free->prev;
    }

    cur_free->prev = before_cur_free;
    cur_free->next = after_cur_free;
    before_cur_free->next = cur_free;
    // before_cur_free->prev = No need to consider
    after_cur_free->prev = cur_free;
    // after_cur_free->next = No need to consider
  }
  void *info = addr;
  size_t hdrFtrSiz = 2 * sizeof(mem_std_block_header_footer_t);
  if (cur_free->next != NULL){
    if (((size_t)cur_free + (size_t)get_block_size(&cur_free->header) + (size_t)hdrFtrSiz) == (size_t)cur_free->next){
      printf("\033[1;33mNext coalescing\n\033[0m;");
      size_t cur_blk_siz = (size_t)get_block_size(&cur_free->header);
      size_t nxt_blk_siz = (size_t)get_block_size(&cur_free->next->header);
      size_t newSiz = cur_blk_siz + nxt_blk_siz + (2 * sizeof(mem_std_block_header_footer_t));
      set_block_free(&cur_free->header);
      set_block_size(&cur_free->header, newSiz);
      mem_std_block_header_footer_t *ftrAddr = (void *)((size_t)cur_free + (size_t)newSiz + sizeof(mem_std_block_header_footer_t));
      set_block_free(ftrAddr);
      set_block_size(ftrAddr, newSiz);

      // cur_free->prev = No need to consider
      cur_free->next = cur_free->next->next;

      info = (void *)((size_t)cur_free + (size_t)sizeof(mem_std_block_header_footer_t));
    }
  }
  if (cur_free->prev != NULL){
    if (((size_t)cur_free->prev + (size_t)get_block_size(&cur_free->prev->header) + (size_t)hdrFtrSiz) == (size_t)cur_free){
      printf("\033[1;33mPrevious coalescing\n\033[0m;");
      size_t prev_blk_siz = (size_t)get_block_size(&cur_free->prev->header);
      size_t cur_blk_siz = (size_t)get_block_size(&cur_free->header);
      size_t newSiz = prev_blk_siz + cur_blk_siz + (2 * sizeof(mem_std_block_header_footer_t));
      set_block_free(&cur_free->prev->header);
      set_block_size(&cur_free->prev->header, newSiz);
      mem_std_block_header_footer_t *ftrAddr = (void *)((size_t)cur_free->prev + (size_t)newSiz + sizeof(mem_std_block_header_footer_t));
      set_block_free(ftrAddr);
      set_block_size(ftrAddr, newSiz);

      // cur_free->prev->prev = No need to consider
      cur_free->prev->next = cur_free->next;

      info = (void *)((size_t)cur_free->prev + (size_t)sizeof(mem_std_block_header_footer_t));
    }
  }
  print_block_info(info);

}

size_t mem_get_allocated_block_size_standard_pool(mem_pool_t *pool, void *addr)
{
  /* TO BE IMPLEMENTED */
  //printf("%s:%d: Please, implement me!\n", __FUNCTION__, __LINE__);
  return 0;
}

void print_block_info(void *payldAddr)
{
  void *address = payldAddr - 8;
  size_t blkSiz = get_block_size(address);
  printf("\033[1;36mFFree: %p\n\033[0m;", pTest->first_free);
  printf("\033[1;36mP : %p, N : %p\n\033[0m;", ((mem_std_free_block_t *)(pTest->first_free))->prev, ((mem_std_free_block_t *)(pTest->first_free))->next);
  if (is_block_used(address) == 0)
  {
    printf("\033[1;32mBlkType: F : %ld\n\033[0m;", blkSiz);
    printf("\033[1;32mHAddr: %p\n\033[0m;", address);
    mem_std_free_block_t *fBlk = address;
    printf("\033[1;32mP : %p, N : %p\n\033[0m;", fBlk->prev, fBlk->next);
  }
  else
  {
    printf("\033[1;31mBlkType: A : %ld\n\033[0m;", blkSiz);
    printf("\033[1;31mHAddr: %p\n\033[0m;", address);
  }
}
