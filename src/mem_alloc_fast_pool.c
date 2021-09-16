#include <assert.h>
#include <stdio.h>

#include "mem_alloc_fast_pool.h"
#include "my_mmap.h"
#include "mem_alloc.h"



void init_fast_pool(mem_pool_t *p, size_t size, size_t min_request_size, size_t max_request_size)
{
    
    int block_size = max_request_size;

    p->start = my_mmap(size);
    p->end = p->start + size;
    p->first_free = p->start;
    mem_fast_free_block_t *p_free_block = p->start;

    int size_allocated = 0;
    while(size_allocated < size)
    {
      p_free_block->next = (void*)p_free_block + block_size;
      size_allocated += block_size;
      p_free_block = p_free_block->next;
    }
    mem_fast_free_block_t *p_last_free_block = (mem_fast_free_block_t*)p->end - block_size;
    p_last_free_block->next = NULL;
}

void *mem_alloc_fast_pool(mem_pool_t *pool, size_t size)
{
  mem_fast_free_block_t *p_first_free_block = (mem_fast_free_block_t*)pool->first_free;
  
  while((void*)p_first_free_block >= pool->start && (void*)p_first_free_block <= pool->end)
  {
    void *payload = pool->first_free;
    p_first_free_block = p_first_free_block->next;
    pool->first_free = p_first_free_block;
    return payload;
  } 
  return NULL;
}

void mem_free_fast_pool(mem_pool_t *pool, void *b)
{
  mem_fast_free_block_t *p_new_free_block = (mem_fast_free_block_t*)(b);
  p_new_free_block->next = pool->first_free;
  pool->first_free = p_new_free_block;
}

size_t mem_get_allocated_block_size_fast_pool(mem_pool_t *pool, void *addr)
{
    size_t res;
    res = pool->max_request_size;
    return res;
}
