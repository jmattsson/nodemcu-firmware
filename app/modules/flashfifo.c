// Module for FLASH sample FIFO storage

#include "lauxlib.h"
#include "user_modules.h"
#include "user_config.h"
#include "rtc/flashfifo.h"

// flashfifo.prepare ()
static int flashfifo_prepare (lua_State *L)
{
  flash_fifo_prepare (0); // dummy "tagcount" argument

  return 0;
}


// ready = flashfifo.ready ()
static int flashfifo_ready (lua_State *L)
{
  lua_pushnumber (L, flash_fifo_check_magic ());
  return 1;
}

static void check_fifo_magic (lua_State *L)
{
  if (!flash_fifo_check_magic ())
    luaL_error (L, "flashfifo not prepared!");
}


// flashfifo.put (timestamp, value, decimals, sensor_name)
static int flashfifo_put (lua_State *L)
{
  check_fifo_magic (L);

  sample_t s;
  s.timestamp = luaL_checknumber (L, 1);
  s.value = luaL_checknumber (L, 2);
  s.decimals = luaL_checknumber (L, 3);
  size_t len;
  const char *str = luaL_checklstring (L, 4, &len);
  union {
    uint32_t u;
    char s[4];
  } conv = { 0 };
  strncpy (conv.s, str, len > 4 ? 4 : len);
  s.tag = conv.u;

  flash_fifo_store_sample (&s);
  return 0;
}


static int extract_sample (lua_State *L, const sample_t *s)
{
  lua_pushnumber (L, s->timestamp);
  lua_pushnumber (L, s->value);
  lua_pushnumber (L, s->decimals);
  union {
    uint32_t u;
    char s[4];
  } conv = { s->tag };
  if (conv.s[3] == 0)
    lua_pushstring (L, conv.s);
  else
    lua_pushlstring (L, conv.s, 4);
  return 4;
}


// timestamp, value, decimals, sensor_name = flashfifo.pop ()
static int flashfifo_pop (lua_State *L)
{
  check_fifo_magic (L);

  sample_t s;
  if (!flash_fifo_pop_sample (&s))
    return 0;
  else
    return extract_sample (L, &s);
}


// timestamp, value, decimals, sensor_name = flashfifo.peek ([offset])
static int flashfifo_peek (lua_State *L)
{
  check_fifo_magic (L);

  sample_t s;
  uint32_t offs = 0;
  if (lua_isnumber (L, 1))
    offs = lua_tonumber (L, 1);
  if (!flash_fifo_peek_sample (&s, offs))
    return 0;
  else
    return extract_sample (L, &s);
}

// flashfifo.drop (num)
static int flashfifo_drop (lua_State *L)
{
  check_fifo_magic (L);

  flash_fifo_drop_samples (luaL_checknumber (L, 1));
  return 0;
}


// num = flashfifo.count ()
static int flashfifo_count (lua_State *L)
{
  check_fifo_magic (L);

  lua_pushnumber (L, flash_fifo_get_count ());
  return 1;
}

// The "size" of a fifo cannot necessarily be described by a single number. On overflow, more than one
// old sample may be lost....

// num = flashfifo.size () --- provides guaranteed capacity; Data *may* be lost if more entries
static int flashfifo_size (lua_State *L)
{
  check_fifo_magic (L);

  lua_pushnumber (L, flash_fifo_get_size ());
  return 1;
}

// num = flashfifo.maxsize () --- provides maximum capacity; Data *will* be lost if more entries
static int flashfifo_maxsize (lua_State *L)
{
  check_fifo_magic (L);

  lua_pushnumber (L, flash_fifo_get_max_size ());
  return 1;
}

static int flashfifo_maxval(lua_State *L)
{
  check_fifo_magic (L);

  lua_pushnumber (L, flash_fifo_get_maxval ());
  return 1;
}


// Module function map
#define MIN_OPT_LEVEL 2
#include "lrodefs.h"
const LUA_REG_TYPE flashfifo_map[] =
{
  { LSTRKEY("prepare"),             LFUNCVAL(flashfifo_prepare) },
  { LSTRKEY("ready"),               LFUNCVAL(flashfifo_ready) },
  { LSTRKEY("put"),                 LFUNCVAL(flashfifo_put) },
  { LSTRKEY("pop"),                 LFUNCVAL(flashfifo_pop) },
  { LSTRKEY("peek"),                LFUNCVAL(flashfifo_peek) },
  { LSTRKEY("drop"),                LFUNCVAL(flashfifo_drop) },
  { LSTRKEY("count"),               LFUNCVAL(flashfifo_count) },
  { LSTRKEY("size"),                LFUNCVAL(flashfifo_size) },
  { LSTRKEY("maxsize"),             LFUNCVAL(flashfifo_maxsize) },
  { LSTRKEY("maxval"),              LFUNCVAL(flashfifo_maxval) },
  { LNILKEY, LNILVAL }
};

LUALIB_API int luaopen_flashfifo (lua_State *L)
{
#if LUA_OPTIMIZE_MEMORY > 0
  return 0;
#else
  luaL_register (L, AUXLIB_FLASHFIFO, flashfifo_map);
  return 1;
#endif
}
