#include "types.h"

struct list_head {
  struct list_head *prev, *next;
};

#define list_entry(ptr, type, member) \
  ((type *)((char *)(ptr)-(unsigned long)(&((type *)0)->member)))

#define list_for_each(pos, head) \
  for (pos = (head)->next; pos != (head); pos = pos->next)

#define list_for_each_safe(pos, n, head) \
  for (pos = (head)->next, n = pos->next; \
      pos != (head); \
      pos = n, n = pos->next)

void
INIT_LIST_HEAD(struct list_head* list)
{
  list->next = list;
  list->prev = list;
}

void
list_add(struct list_head* new, struct list_head* head)
{
  struct list_head* next = head->next;
  next->prev = new;
  new->next = next;
  head->next = new;
  new->prev = head;
}

void
list_add_tail(struct list_head* new, struct list_head* head)
{
  struct list_head* prev = head->prev;
  prev->next = new;
  new->prev = prev;
  head->prev = new;
  new->next = head;
}

void
list_del(struct list_head* entry)
{
  entry->next->prev = entry->prev;
  entry->prev->next = entry->next;
}

void
list_del_init(struct list_head* entry)
{
  list_del(entry);
  entry->next = entry->prev = 0;
}

int
list_empty(struct list_head* head)
{
  return head->next == head;
}
