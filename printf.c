#include "types.h"
#include "stat.h"
#include "user.h"

static void
putc(int fd, char c)
{
  write(fd, &c, 1);
}

static void
printint(int fd, int xx, int base, int sgn, int width, char padding_char)
{
  static char digits[] = "0123456789ABCDEF";
  char buf[16];
  int i, neg, k;
  uint x;

  neg = 0;
  if(sgn && xx < 0){
    neg = 1;
    x = -xx;
  } else {
    x = xx;
  }

  i = 0;
  do{
    buf[i++] = digits[x % base];
  }while((x /= base) != 0);
  if(neg)
    buf[i++] = '-';
  k = width;
  while(--k >= i)
    putc(fd, padding_char);

  while(--i >= 0)
    putc(fd, buf[i]);
}

// Print to the given fd. Only understands %d, %x, %p, %s.
void
printf(int fd, char *fmt, ...)
{
  char *s;
  int c, i, state;
  uint *ap;

  state = 0;
  ap = (uint*)(void*)&fmt + 1;
  for(i = 0; fmt[i]; i++){
    c = fmt[i] & 0xff;
    if(state == 0){
      if(c == '%'){
        state = '%';
      } else {
        putc(fd, c);
      }
    } else if(state == '%'){
      char padding_char = ' ';
      if(c == '0'){
        padding_char = '0';
        i++;
        c = fmt[i] & 0xff;
      }
      int width = 0;
      while('0' <= c && c <= '9'){
        width *= 10;
        width += c - '0';
        i++;
        c = fmt[i] & 0xff;
      }
      if(c == 'd'){
        printint(fd, *ap, 10, 1, width, padding_char);
        ap++;
      } else if(c == 'x' || c == 'p'){
        printint(fd, *ap, 16, 0, width, padding_char);
        ap++;
      } else if(c == 'o'){
        printint(fd, *ap, 8, 0, width, padding_char);
        ap++;
      } else if(c == 's'){
        s = (char*)*ap;
        ap++;
        if(s == 0)
          s = "(null)";
        while(*s != 0){
          putc(fd, *s);
          s++;
        }
      } else if(c == 'c'){
        putc(fd, *ap);
        ap++;
      } else if(c == '%'){
        putc(fd, c);
      } else {
        // Unknown % sequence.  Print it to draw attention.
        putc(fd, '%');
        putc(fd, c);
      }
      state = 0;
    }
  }
}
