/*
Copyright(c) 2016 Johny Mattsson <johny.mattsson@gmail.com>
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
#include "xmem.h"
#include "c_stdio.h"

/* Wrappers for the SDK Port allocator */
void *_port_malloc(size_t sz) { return pvPortMalloc(sz, "", 0); }
void *_port_zalloc(size_t sz) { return pvPortZalloc(sz, "", 0); }
void *_port_realloc(void *p, size_t sz) { return pvPortRealloc(p, sz, "", 0); }
void _port_free(void *p) { vPortFree(p, "", 0); }


static xmemslot_t *find(xmemdb_t *db, void *p)
{
  xmemslot_t tmp = { .used = 0, .line = 0, .lptr = (uint32_t)p };
  for (xmemblock_t *block = db->block; block; block = block->next)
  {
    for (unsigned i = 0; i < sizeof(block->slots)/sizeof(block->slots[0]); ++i)
    {
      xmemslot_t *slot = block->slots + i;
      if (slot->used && slot->lptr == tmp.lptr)
        return slot;
    }
  }
  return 0;
}

static xmemslot_t *free_slot_in_block (xmemblock_t *block)
{
  for (unsigned i = 0; i < sizeof(block->slots)/sizeof(block->slots[0]); ++i)
  {
    xmemslot_t *slot = block->slots + i;
    if (!slot->used)
      return slot;
  }
}

static xmemslot_t *next_free(xmemdb_t *db)
{
  for (xmemblock_t *block = db->block; block; block = block->next)
  {
    xmemslot_t *slot = free_slot_in_block (block);
    if (slot)
      return slot;
  }
  xmemblock_t *block = os_zalloc(sizeof(xmemblock_t));
  if (block)
  {
    block->next = db->block;
    db->block = block;
    return &block->slots[0];
  }
  else
    return 0;
}

static bool repack (xmemblock_t *from, xmemblock_t *to)
{
  bool packed = false;
  for (unsigned i = 0; i < sizeof(from->slots)/sizeof(from->slots[0]); ++i)
  {
    xmemslot_t *avail = 0;
    if (from->slots[i].used && (avail = free_slot_in_block (to)))
    {
      *avail = from->slots[i];
      from->slots[i].used = 0;
      packed = true;
    }
  }
  return packed;
}

static bool empty_block (xmemblock_t *block)
{
  bool empty = true;
  for (unsigned i = 0; i < sizeof(block->slots)/sizeof(block->slots[0]); ++i)
    empty |= block->slots[i].used;
  return empty;
}

static void xmem_sweep (xmemdb_t *db)
{
  bool again = false;
  xmemblock_t **bptr = &db->block;
  while (*bptr && (*bptr)->next)
  {
    xmemblock_t *block = *bptr;
    if (repack (block, block->next))
    {
      again = true;
      if (empty_block (block))
      {
        *bptr = block->next;
        os_free (block);
        continue;
      }
    }
    bptr = &(*bptr)->next;
  }
  if (again)
    db->sweep--; // sweep again on next erase to continue repacking
  else
    db->sweep = 0;
}


static void note (xmemdb_t *db, uint32_t line, void *p)
{
  xmemslot_t *slot = find (db, p);
  if (slot)
  {
    c_printf("\nXMEM: %p already allocated at line %d, being allocated again without free on line %d!\n", p, slot->line, line);
    return; // Keep things simple, don't store another entry
  }

  slot = next_free (db);
  if (!slot)
  {
    c_printf("\nXMEM: failed to record %p allocated at line %d - no memory!\n", p, line);
    return;
  }

  xmemslot_t tmp = { .used = 1, .line = line, .lptr = (uint32_t)p };
  *slot = tmp;
}

static void erase (xmemdb_t *db, uint32_t line, void *p)
{
  xmemslot_t *slot = find (db, p);
  if (!slot)
    c_printf("\nXMEM: freeing unknown pointer %p from line %d!\n", p, line);
  else
    slot->used = 0;

  if (++db->sweep > 16)
    xmem_sweep (db);
}

#define CANARY_CHK   0xa000
#define CANARY_MASK  0xe000
#define CANARY_VAL   0xa55a
static void canary (char *p, size_t sz)
{
  char *pre = p - 2;
  char *post = p + sz;
  uint16_t val = sz & ~CANARY_MASK;
  if (sz & CANARY_MASK)
  {
    c_printf("\nXMEM: unable to put canary on alloc sz %d!\n", sz);
    val = 0;
  }
  val |= CANARY_CHK;
  pre[0] = val >> 8;
  pre[1] = val & 0xff;
  post[0] = CANARY_VAL >> 8;
  post[1] = CANARY_VAL & 0xff;
}

static size_t canary_get_size (char *p)
{
  char *pre = p - 2;
  uint16_t val = pre[0] << 8 | pre[1];
  if ((val & CANARY_MASK) != CANARY_CHK)
  {
    c_printf("\nXMEM: canary header missing on %p! (buffer underrun?)\n", p);
    return 0;
  }
  return val & ~CANARY_MASK;
}

static void check_canary (char *p)
{
  size_t sz = canary_get_size (p);
  if (sz)
  {
    char *post = p + sz;
    uint16_t canary = post[0] << 8 | post[1];
    if (canary != CANARY_VAL)
      c_printf("\nXMEM: dead canary at %p! %x but expected %x\n", p, canary, CANARY_VAL);
  }
  else
    c_printf("\nXMEM: %p marked as too large for canary, not checking\n", p);
}

void *_xmem_alloc(size_t sz, xmemdb_t *db, uint32_t line, xmem_alloc_fn_t fn)
{
  void *p = fn (sz + 4);
  if (p)
  {
    note(db, line, p);
    canary(p, sz);
  }
  return p;
}


void *_xmem_realloc(void *p, size_t sz, xmemdb_t *db, uint32_t line, xmem_realloc_fn_t fn)
{
  void *p2 = fn (p, sz + 4);
  if (p2)
  {
    canary (p2, sz);
    erase (db, line, p);
    note (db, line, p2);
  }
  return p2;
}


void *_xmem_free(void *p, xmemdb_t *db, uint32_t line, xmem_free_fn_t fn)
{
  if (p)
  {
    check_canary (p);
    erase (db, line, p);
  }
  fn ((char *)p - 4);
}


void xmem_dump_db(xmemdb_t *db)
{
  c_printf("XMEM dump for \"%s\":\n", db->name);
  for (xmemblock_t *block = db->block; block; block = block->next)
  {
    for (unsigned i = 0; i < sizeof(block->slots)/sizeof(block->slots[0]); ++i)
    {
      xmemslot_t slot = block->slots[i];
      if (slot.used)
      {
        char *p = (char *)(0x3fe00000 | slot.lptr);
        size_t sz = canary_get_size (p);
        c_printf("%p (%d) @ L%d\n", p, sz, slot.line);
        check_canary (p);
      }
    }
  }
  c_printf("XMEM end dump\n");
}
