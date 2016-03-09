#ifndef _XMEM_H_
#define _XMEM_H_

#include "os_type.h"
#include "mem.h"
#include "lua.h"

/* XMEM features:
 *
 *  p = xmalloc(sz);      -- malloc
 *  p = xzalloc(sz);      -- zero-init alloc
 *  p = xrealloc(p, sz);  -- realloc
 *  xfree(p);             -- free
 *
 *  When XMEM_TRACK is defined (to a "name"), tracking is enabled, and the
 *  following are also available:
 *
 *  xmem_dump_db(&_xmem); -- print current allocations
 *
 *  Convenience macro for registering a '.xmemshow()' function in a module,
 *  if XMEM_TRACK is enabled (expands to nothing otherwise).
 *
 *  static const LUA_REG_TYPE mod_map[] = {
 *    { LSTRKEY("somefunc"),  LFUNCVAL(mod_somefunc)  },
 *     XMEM_LUA_TABLE_ENTRY
 *     { LNILKEY, LNILVAL }
 *   };
 */

typedef struct xmemslot
{
  uint32_t used :  1;
  uint32_t line : 10;
  uint32_t lptr : 21;
} xmemslot_t;

typedef struct xmemblock
{
  xmemslot_t slots[15];
  struct xmemblock *next;
} xmemblock_t;


typedef struct xmemdb
{
  xmemblock_t *block;
  const char *name;
  uint8_t sweep;
} xmemdb_t;

typedef void *(*xmem_alloc_fn_t)(size_t sz);
typedef void *(*xmem_realloc_fn_t)(void *p, size_t sz);
typedef void (*xmem_free_fn_t)(void *p);

void *_xmem_alloc(size_t sz, xmemdb_t *db, uint32_t line, xmem_alloc_fn_t fn);
void *_xmem_realloc(void *p, size_t sz, xmemdb_t *db, uint32_t line, xmem_realloc_fn_t fn);
void *_xmem_free(void *p, xmemdb_t *db, uint32_t line, xmem_free_fn_t fn);

void xmem_dump_db(xmemdb_t *db);

/* Wrappers for the SDK Port allocator, so we can pass function pointers */
void *_port_malloc(size_t sz);
void *_port_zalloc(size_t sz);
void *_port_realloc(void *p, size_t sz);
void _port_free(void *p);

// TODO: add choice between os_?alloc() and luaM_alloc(L, xxx)

#ifdef XMEM_TRACK

static xmemdb_t _xmem = { 0, XMEM_TRACK, 0 };

# define xmalloc(sz)    _xmem_alloc(sz, &_xmem, __LINE__, _port_malloc)
# define xzalloc(sz)    _xmem_alloc(sz, &_xmem, __LINE__, _port_zalloc)
# define xrealloc(p,sz) _xmem_realloc(p, sz, &_xmem, __LINE__, _port_realloc)
# define xfree(p)       _xmem_free(p, &_xmem, __LINE__, _port_free)

static int _xmem_lua_show(lua_State *L)
{
  (void)L;
  xmem_dump_db(&_xmem);
  return 0;
}

# define XMEM_LUA_TABLE_ENTRY { LSTRKEY("xmemshow"), LFUNCVAL(_xmem_lua_show) },

#else

# define xmalloc(sz)    os_malloc(sz)
# define xzalloc(sz)    os_zalloc(sz)
# define xrealloc(p,sz) os_realloc(p, sz)
# define xfree(p)       os_free(p)
# define XMEM_LUA_TABLE_ENTRY

#endif

#endif
