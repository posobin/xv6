#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "spinlock.h"

struct free_block
{
  struct free_block* next_block;
};

struct free_mem_list
{
  unsigned int block_size;
  struct free_block* first_free_block;
};

struct free_mem_list free_mem_list[] =
{
  { .block_size = 16,   .first_free_block = 0 },
  { .block_size = 32,   .first_free_block = 0 },
  { .block_size = 64,   .first_free_block = 0 },
  { .block_size = 128,  .first_free_block = 0 },
  { .block_size = 256,  .first_free_block = 0 },
  { .block_size = 512,  .first_free_block = 0 },
  { .block_size = 1024, .first_free_block = 0 },
  { .block_size = 2048, .first_free_block = 0 },
  { .block_size = 4096, .first_free_block = 0 },
};
struct spinlock blocks_lock;

// Returns first free block in mem_list. If there are no free blocks,
// calls kalloc(), adds new free blocks to mem_list and returns
// newly created first free block.
// mem_list->block_size must divide PGSIZE evenly.
static struct free_block*
get_block(struct free_mem_list* mem_list)
{
  if (mem_list->first_free_block) {
    struct free_block* result = mem_list->first_free_block;
    mem_list->first_free_block = result->next_block;
    return result;
  }
  void* new_page = kalloc();
  mem_list->first_free_block = (struct free_block*)new_page;
  unsigned int block_size = mem_list->block_size;
  if (PGSIZE % block_size != 0) {
    panic("get_block");
  }
  for (int i = 0; i < PGSIZE; i += block_size) {
    ((struct free_block*)(new_page + i))->next_block =
      (struct free_block*)(new_page + i + block_size);
  }
  ((struct free_block*)(new_page + (PGSIZE - block_size)))->next_block = 0;
  return (struct free_block*)new_page;
}

static void*
alloc_block(int index) {
  acquire(&blocks_lock);
  struct free_block* result = get_block(&free_mem_list[index]);
  free_mem_list[index].first_free_block = result->next_block;
  result->next_block = 0;
  release(&blocks_lock);
  return (void*)result;
}

static void
put_block(void* mem, int index) {
  acquire(&blocks_lock);
  struct free_block* block = (struct free_block*)mem;
  block->next_block = free_mem_list[index].first_free_block;
  free_mem_list[index].first_free_block = block;
  release(&blocks_lock);
}

void*
kmalloc(unsigned int size)
{
  int i = 0;
  for (i = 0; i < NELEM(free_mem_list); ++i) {
    if (free_mem_list[i].block_size >= size) {
      break;
    }
  }
  if (i == NELEM(free_mem_list)) return 0;
  return alloc_block(i);
}

void
kfreee(void* mem, unsigned int size)
{
  int i = 0;
  for (i = 0; i < NELEM(free_mem_list); ++i) {
    if (free_mem_list[i].block_size >= size) {
      break;
    }
  }
  if (i == NELEM(free_mem_list)) return;
  put_block(mem, i);
}

void
init_blocks(void)
{
  initlock(&blocks_lock, "blocks");
}
