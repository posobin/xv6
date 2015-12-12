#ifndef XV6_LIST_H
#define XV6_LIST_H

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

#define list_for_each_entry(pos, head, member) \
  for (pos = list_entry((head)->next, typeof(*pos), member); \
      &pos->member != (head); \
      pos = list_entry(pos->member.next, typeof(*pos), member))

#endif
