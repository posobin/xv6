#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "spinlock.h"
#include "list.h"

struct cache_info
{
  unsigned int block_size;
  struct list_head partial_list;
  struct list_head full_list;
  struct list_head empty_list;
};

struct page_header
{
  struct cache_info* cache_info;
  void* empty_block;
  unsigned int empty_count;
  struct list_head list;
};

struct big_page_hash_info
{
  void* page;
  struct page_header header;
  struct big_page_hash_info* next;
};

struct cache_info* cache_table;
unsigned int cache_count = 0;
struct big_page_hash_info** pages_hash_table;
struct cache_info* big_page_hash_info_cache;
struct spinlock caches_lock;

// Given a free page and a cache, add the page to that cache and
// initialize the page as needed for small objects.
// Always succeeds and returns 0.
// page must be page-aligned.
int
init_small_page(void* page, struct cache_info* info)
{
  if ((unsigned int)page % PGSIZE) {
    panic("page address not page-aligned (init_small_page)");
  }
  unsigned int block_size = info->block_size;
  unsigned int count = (PGSIZE - sizeof(struct page_header)) / block_size;
  *(struct page_header*)page = (struct page_header) {
    .cache_info = info,
    .empty_block = page + sizeof(struct page_header),
    .empty_count = count
  };
  list_add(&((struct page_header*)page)->list, &info->empty_list);
  void* last_ptr;
  for (void* ptr = page + sizeof(struct page_header);
      ptr <= page + PGSIZE - block_size;
      ptr += block_size) {
    *(void**)ptr = ptr + block_size;
    last_ptr = ptr;
  }
  *(void**)last_ptr = 0;
  return 0;
}

unsigned int
get_number_of_blocks(int block_size)
{
  if ((block_size * 8) >= PGSIZE) return PGSIZE / block_size;
  return (PGSIZE - sizeof(struct page_header)) / block_size;
}

// Given a not full page header, get next free block from it.
void*
get_empty_block_from_page(struct page_header* header)
{
  if (header->empty_count == 0) {
    panic("Allocating memory from empty page");
  }
  void* result = header->empty_block;
  header->empty_block = *(void**)result;
  header->empty_count--;
  list_del(&header->list);
  list_add_tail(&header->list, &header->cache_info->partial_list);
  if (header->empty_count == 0) {
    list_del(&header->list);
    list_add(&header->list, &header->cache_info->full_list);
  }
  return result;
}

// Adds page_hash_info to the hash table.
void
add_to_hash_table(struct big_page_hash_info* page_hash_info)
{
  unsigned int index = (((unsigned int)page_hash_info->page) / PGSIZE) % 1024;
  struct big_page_hash_info* next = pages_hash_table[index];
  page_hash_info->next = next;
  pages_hash_table[index] = page_hash_info;
}

int init_big_page(void*, struct cache_info*);

// Returns pointer to the next empty block from the cache, in case of failure
// returns 0.
void*
get_empty_block(struct cache_info* cache)
{
  int is_big = ((cache->block_size * 8) >= PGSIZE);

  struct page_header* header;
  if (!list_empty(&cache->partial_list)) {
    header = list_entry(cache->partial_list.next, struct page_header, list);
  } else if (!list_empty(&cache->empty_list)) {
    header = list_entry(cache->empty_list.next, struct page_header, list);
  } else {
    void* page = kalloc();
    if (page == 0) return 0;
    int result = 0;
    if (is_big) result = init_big_page(page, cache);
    else result = init_small_page(page, cache);
    if (result < 0) return 0;
    header = list_entry(cache->empty_list.next, struct page_header, list);
  }
  return get_empty_block_from_page(header);
}

// Given a page and a cache, initialize the page for use with that cache.
// Returns a negative value if allocation was not successful.
// page must be page-aligned.
int
init_big_page(void* page, struct cache_info* cache)
{
  if ((unsigned int)page % PGSIZE) {
    panic("page address not page-aligned (init_big_page)");
  }
  struct big_page_hash_info* hash_info =
    get_empty_block(big_page_hash_info_cache);
  if (hash_info == 0) return -1;
  unsigned int block_size = cache->block_size;
  *hash_info = (struct big_page_hash_info) {
    .page = page,
    .header = {
      .cache_info = cache,
      .empty_block = page,
      .empty_count = PGSIZE / block_size,
    },
    .next = 0,
  };
  list_add(&hash_info->header.list, &cache->empty_list);
  add_to_hash_table(hash_info);
  void* last;
  for (unsigned int i = 0; i < PGSIZE; i += block_size) {
    *(void**)(page + i) = page + i + block_size;
    last = page + i;
  }
  *(void**)last = 0;
  return 0;
}

// Get hash_info structure for the given address
// (which must be page-aligned).
// Returns 0 when the corresponding hash_info was not found.
struct big_page_hash_info*
get_big_page_hash_info(void* page)
{
  if ((unsigned int)page % PGSIZE) {
    panic("page address not page-aligned (get_big_page_hash_info)");
  }
  unsigned int index = (((unsigned int)page) / PGSIZE) % 1024;
  struct big_page_hash_info* next = pages_hash_table[index];
  while (next != 0) {
    if (next->page == (void*)PGROUNDDOWN((unsigned int)page)) break;
    next = next->next;
  }
  return next;
}

// Delete hash info for the given page from pages_hash_table.
// hash_info object itself is not deleted.
// If page is not found, returns 0.
// page address must be page-aligned.
struct big_page_hash_info*
delete_big_page_hash_info(void* page)
{
  if ((unsigned int)page % PGSIZE) {
    panic("page address not page-aligned (delete_big_page_hash_info)");
  }
  unsigned int index = (PGROUNDDOWN((unsigned int)page) / PGSIZE) % 1024;
  struct big_page_hash_info* current = pages_hash_table[index];
  struct big_page_hash_info* previous = 0;
  while (current != 0) {
    if (current->page == (void*)PGROUNDDOWN((unsigned int)page)) break;
    previous = current;
    current = current->next;
  }
  if (current != 0) {
    struct big_page_hash_info* next = current->next;
    if (previous == 0) {
      pages_hash_table[index] = next;
    } else {
      previous->next = next;
    }
    current->next = 0;
  }
  return current;
}

// Mark the given block from the given page as free.
// The block must be from the same page that the page_header 
// describes.
void
free_page_block(void* block, struct page_header* page_header)
{
  void* previous_empty = page_header->empty_block;
  *(void**)block = previous_empty;
  if (previous_empty != 0 &&
      (PGROUNDDOWN((unsigned int)block) != PGROUNDDOWN((unsigned int)previous_empty))) {
    panic("block is not in the page");
  } else if ((page_header->cache_info->block_size * 8) < PGSIZE) {
    if (PGROUNDDOWN((unsigned int)block) != PGROUNDDOWN((unsigned int)page_header)) {
      panic("block is not in the page");
    } else if ((unsigned int)page_header % PGSIZE) {
      panic("page_header address is not page aligned");
    }
  }
  page_header->empty_block = block;
  struct cache_info* cache_info = page_header->cache_info;
  unsigned int block_size = cache_info->block_size;
  if (++page_header->empty_count == get_number_of_blocks(block_size)) {
    // All the blocks in the page are free, we will free the page now.
    list_del(&page_header->list);
    kfree((void*)PGROUNDDOWN((unsigned int)block));
    return;
    if (!list_empty(&cache_info->empty_list)) {
    } else {
      list_add(&page_header->list, &cache_info->empty_list);
    }
  } else if (page_header->empty_count == 1) {
    list_del(&page_header->list);
    list_add(&page_header->list, &cache_info->partial_list);
  }
}

// Mark the given block as free.
void
free_block(void* block)
{
  void* page = (void*)PGROUNDDOWN((unsigned int)block);
  struct big_page_hash_info* hash_info = get_big_page_hash_info(page);
  if (hash_info) {
    free_page_block(block, &hash_info->header);
  } else {
    free_page_block(block, (struct page_header*)page);
  }
}

// Delete the page from all the lists and kfree() it.
// page address must be page-aligned.
void
free_page(void* page)
{
  if ((unsigned int)page % PGSIZE) {
    panic("page address not page-aligned (free_page)");
  }
  struct page_header* header;
  struct big_page_hash_info* hash_info = delete_big_page_hash_info(page);
  if (hash_info == 0) {
    header = (struct page_header*)PGROUNDDOWN((unsigned int)page);
    if (header->empty_count != 0)
    {
      panic("Freeing non-empty small page in free_page");
    }
    list_del(&header->list);
  } else {
    header = &hash_info->header;
    if (header->empty_count != 0)
    {
      panic("Freeing non-empty big page in free_page");
    }
    list_del(&header->list);
    free_block(hash_info);
  }
  kfree((void*)PGROUNDDOWN((unsigned int)page));
}

// Creates new cache with given block size and returns a pointer to it.
struct cache_info*
kmem_cache_create(unsigned int block_size)
{
  if (PGSIZE < block_size) {
    return 0;
  }
  if (cache_count >= PGSIZE / sizeof(struct cache_info)) {
    return 0;
  }
  if (block_size < sizeof(struct page_header)) {
    block_size = sizeof(struct page_header);
  }
  acquire(&caches_lock);
  struct cache_info* result = &cache_table[cache_count++];
  result->block_size = block_size;
  INIT_LIST_HEAD(&result->partial_list);
  INIT_LIST_HEAD(&result->full_list);
  INIT_LIST_HEAD(&result->empty_list);
  release(&caches_lock);
  return result;
}

// Allocate one block from memory.
// Returns 0 on failure.
void*
kmem_cache_alloc(struct cache_info* cache)
{
  acquire(&caches_lock);
  void* result = get_empty_block(cache);
  release(&caches_lock);
  return result;
}

// Mark the given block as free.
void
kmem_cache_free(void* block)
{
  acquire(&caches_lock);
  free_block(block);
  release(&caches_lock);
}

// Initialize all cache data structures.
void
init_caches(void)
{
  cache_table = (struct cache_info*)kalloc();
  pages_hash_table = (struct big_page_hash_info**)kalloc();
  memset(cache_table, 0, PGSIZE);
  memset(pages_hash_table, 0, PGSIZE);
  big_page_hash_info_cache =
    kmem_cache_create(sizeof(struct big_page_hash_info));
  if (big_page_hash_info_cache == 0) {
    panic("Can't allocate cache?!");
  }

  initlock(&caches_lock, "caches");
}
