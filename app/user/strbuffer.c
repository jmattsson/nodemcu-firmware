#include "strbuffer.h"
#include <stdarg.h>
#include <stdio.h>
#include "osapi.h"
#include "mem.h"

#ifndef STRBUFFER_MIN_GROWSIZE
#define STRBUFFER_MIN_GROWSIZE 64
#endif

struct strbuffer
{
  char *start;
  char *cur;
  char *end;
};


strbuffer_t *strbuffer_create (size_t sz)
{
  strbuffer_t *sb = os_malloc (sizeof (strbuffer_t));
  if (!sb)
    return 0;
  sb->start = sb->cur = os_malloc (sz ? sz : STRBUFFER_MIN_GROWSIZE);
  sb->end = sb->start + sz;
  if (!sb->start)
  {
    os_free (sb);
    sb = 0;
  }
  return sb;
}


void strbuffer_free (strbuffer_t *sb)
{
  os_free (sb->start);
  os_free (sb);
}


void strbuffer_reset (strbuffer_t *sb)
{
  sb->cur = sb->start;
  *sb->cur = 0;
}


char *strbuffer_str (strbuffer_t *sb, size_t *len)
{
  if (len)
    *len = sb->cur - sb->start;
  return sb->start;
}


bool strbuffer_resize (strbuffer_t *sb, size_t sz)
{
  size_t oldsz = sb->end - sb->start;
  size_t used = sb->cur - sb->start;
  if (used > sz)
    return false;
  char *p = os_realloc (sb->start, sz);
  if (!p)
    return false;
  sb->start = p;
  sb->end = sb->start + sz;
  sb->cur = sb->start + used;
  return true;
}


static size_t grow_for (strbuffer_t *sb, size_t needed)
{
  size_t growby =
    needed*2 < STRBUFFER_MIN_GROWSIZE ? STRBUFFER_MIN_GROWSIZE : needed*2;
  return (sb->end - sb->start) + growby;
}


bool strbuffer_add (strbuffer_t *sb, const char *fmt, ...)
{
  size_t avail = sb->end - sb->cur;
  int n;
  do {
    va_list args;
    va_start (args, fmt);
    n = vsnprintf (sb->cur, avail, fmt, args);
    va_end (args);
    if (n >= avail)
    {
      /* truncated output, undo it, resize the bufferfer and try again */
      *sb->cur = 0;
      if (!strbuffer_resize (sb, grow_for (sb, n - avail + 1)))
        return false;
      avail = sb->end - sb->cur;
    }
  } while (!*sb->cur);
  if (n < 0)
  {
    *sb->cur = 0; /* roll back any change */
    return false;
  }
  else
  {
    sb->cur += n;
    return true;
  }
}


bool strbuffer_append (strbuffer_t *sb, const char *str, size_t len)
{
  size_t avail = sb->end - sb->cur;
  if (len >= avail && !strbuffer_resize (sb, grow_for (sb, len - avail + 1)))
    return false;
  os_memcpy (sb->cur, str, len);
  sb->cur += len;
  *sb->cur = 0;
  return true;
}

