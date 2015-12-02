#include "list.h"

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
  INIT_LIST_HEAD(entry);
}

int
list_empty(struct list_head* head)
{
  return head->next == head;
}
