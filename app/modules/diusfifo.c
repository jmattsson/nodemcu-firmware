// Module for DIUS sample FIFO storage

#include "lauxlib.h"
#include "user_modules.h"
#include "rtc/diusfifo.h"

// diusfifo.prepare ([{sensor_count=n, interval_us=m, samples_per=p storage_begin=x, storage_end=y}])
static int diusfifo_prepare (lua_State *L)
{
  uint32_t sensor_count = RTC_DEFAULT_TAGCOUNT;
  int first = -1, last = -1;

  if (lua_istable (L, 1))
  {
    lua_getfield (L, 1, "sensor_count");
    if (lua_isnumber (L, -1))
      sensor_count = lua_tonumber (L, -1);
    lua_pop (L, 1);

    lua_getfield (L, 1, "storage_begin");
    if (lua_isnumber (L, -1))
      first = lua_tonumber (L, -1);
    lua_pop (L, 1);
    lua_getfield (L, 1, "storage_end");
    if (lua_isnumber (L, -1))
      last = lua_tonumber (L, -1);
    lua_pop (L, 1);
  }
  else if (!lua_isnone (L, 1))
    return luaL_error (L, "expected table as arg #1");

  dius_fifo_prepare (sensor_count);

  if (first != -1 && last != -1)
    dius_fifo_put_loc (first, last, sensor_count);

  return 0;
}


// ready = diusfifo.ready ()
static int diusfifo_ready (lua_State *L)
{
  lua_pushnumber (L, dius_fifo_check_magic ());
  return 1;
}

static void check_fifo_magic (lua_State *L)
{
  if (!dius_fifo_check_magic ())
    luaL_error (L, "diusfifo not prepared!");
}


// diusfifo.put (timestamp, value, decimals, sensor_name)
static int diusfifo_put (lua_State *L)
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

  dius_fifo_store_sample (&s);
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


// timestamp, value, decimals, sensor_name = diusfifo.pop ()
static int diusfifo_pop (lua_State *L)
{
  check_fifo_magic (L);

  sample_t s;
  if (!dius_fifo_pop_sample (&s))
    return 0;
  else
    return extract_sample (L, &s);
}


// timestamp, value, decimals, sensor_name = diusfifo.peek ([offset])
static int diusfifo_peek (lua_State *L)
{
  check_fifo_magic (L);

  sample_t s;
  uint32_t offs = 0;
  if (lua_isnumber (L, 1))
    offs = lua_tonumber (L, 1);
  if (!dius_fifo_peek_sample (&s, offs))
    return 0;
  else
    return extract_sample (L, &s);
}

// diusfifo.drop (num)
static int diusfifo_drop (lua_State *L)
{
  check_fifo_magic (L);

  dius_fifo_drop_samples (luaL_checknumber (L, 1));
  return 0;
}


// num = diusfifo.count ()
static int diusfifo_count (lua_State *L)
{
  check_fifo_magic (L);

  lua_pushnumber (L, dius_fifo_get_count ());
  return 1;
}

// The "size" of a fifo cannot necessarily be described by a single number. On overflow, more than one
// old sample may be lost....

// num = diusfifo.size () --- provides guaranteed capacity; Data *may* be lost if more entries
static int diusfifo_size (lua_State *L)
{
  check_fifo_magic (L);

  lua_pushnumber (L, dius_fifo_get_size ());
  return 1;
}

// num = diusfifo.maxsize () --- provides maximum capacity; Data *will* be lost if more entries
static int diusfifo_maxsize (lua_State *L)
{
  check_fifo_magic (L);

  lua_pushnumber (L, dius_fifo_get_max_size ());
  return 1;
}

static int diusfifo_maxval(lua_State *L)
{
  check_fifo_magic (L);

  lua_pushnumber (L, dius_fifo_get_maxval ());
  return 1;
}


// Module function map
#define MIN_OPT_LEVEL 2
#include "lrodefs.h"
const LUA_REG_TYPE diusfifo_map[] =
{
  { LSTRKEY("prepare"),             LFUNCVAL(diusfifo_prepare) },
  { LSTRKEY("ready"),               LFUNCVAL(diusfifo_ready) },
  { LSTRKEY("put"),                 LFUNCVAL(diusfifo_put) },
  { LSTRKEY("pop"),                 LFUNCVAL(diusfifo_pop) },
  { LSTRKEY("peek"),                LFUNCVAL(diusfifo_peek) },
  { LSTRKEY("drop"),                LFUNCVAL(diusfifo_drop) },
  { LSTRKEY("count"),               LFUNCVAL(diusfifo_count) },
  { LSTRKEY("size"),                LFUNCVAL(diusfifo_size) },
  { LSTRKEY("maxsize"),             LFUNCVAL(diusfifo_maxsize) },
  { LSTRKEY("maxval"),              LFUNCVAL(diusfifo_maxval) },
  { LNILKEY, LNILVAL }
};

LUALIB_API int luaopen_diusfifo (lua_State *L)
{
#if LUA_OPTIMIZE_MEMORY > 0
  return 0;
#else
  luaL_register (L, AUXLIB_DIUSFIFO, diusfifo_map);
  return 1;
#endif
}
