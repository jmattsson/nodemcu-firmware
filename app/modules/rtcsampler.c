// Module for RTC sample acquisition timing/counting
//
// (This module provides persistent storage of sampling parameters
//  in the rtc memory, which may be accessed by non-nodemcu samplers
//  that act as a boot loader. It also provides a method to deep-sleep
//  until a next scheduled sample in a convenient manner)


#include "lauxlib.h"
#include "user_modules.h"
#include "rtc/rtctime.h"
#define RTCTIME_SLEEP_ALIGNED rtctime_deep_sleep_until_aligned_us
#include "rtc/rtcsampler.h"

// rtcsampler.prepare ([{interval_us=m, samples_per=p}])
static int rtcsampler_prepare (lua_State *L)
{
  uint32_t interval_us = 0;
  uint32_t samples_per_boot=0;

  if (lua_istable (L, 1))
  {
#ifdef LUA_USE_MODULES_RTCTIME
    lua_getfield (L, 1, "interval_us");
    if (lua_isnumber (L, -1))
      interval_us = lua_tonumber (L, -1);
    lua_pop (L, 1);

    lua_getfield (L, 1, "samples_per");
    if (lua_isnumber (L, -1))
      samples_per_boot = lua_tonumber (L, -1);
    lua_pop (L, 1);
#endif
  }
  else if (!lua_isnone (L, 1))
    return luaL_error (L, "expected table as arg #1");

  rtc_sampler_prepare (samples_per_boot, interval_us);
  return 0;
}


// ready = rtcsampler.ready ()
static int rtcsampler_ready (lua_State *L)
{
  lua_pushnumber (L, rtc_sampler_check_magic ());
  return 1;
}

static void check_sampler_magic (lua_State *L)
{
  if (!rtc_sampler_check_magic ())
    luaL_error (L, "rtcsampler not prepared!");
}


// rtcsampler.request_samples([sample_count])
static int rtcsampler_request_samples (lua_State *L)
{
  check_sampler_magic (L);

  if (lua_isnumber (L, 1))
  {
    uint32_t count = lua_tonumber (L, 1);
    rtc_put_samples_to_take(count);
  }
  else
  {
    rtc_restart_samples_to_take();
  }
}

#ifdef LUA_USE_MODULES_RTCTIME
// rtcsampler.dsleep_until_sample (min_sleep_us)
static int rtcsampler_dsleep_until_sample (lua_State *L)
{
  check_sampler_magic (L);

  uint32_t min_us = luaL_checknumber (L, 1);
  rtc_sampler_deep_sleep_until_sample (min_us); // no return
  return 0;
}
#endif

// Module function map
#define MIN_OPT_LEVEL 2
#include "lrodefs.h"
const LUA_REG_TYPE rtcsampler_map[] =
{
  { LSTRKEY("prepare"),             LFUNCVAL(rtcsampler_prepare) },
  { LSTRKEY("ready"),               LFUNCVAL(rtcsampler_ready) },
#ifdef LUA_USE_MODULES_RTCTIME
  { LSTRKEY("dsleep_until_sample"), LFUNCVAL(rtcsampler_dsleep_until_sample) },
#endif
  { LSTRKEY("request_samples"),     LFUNCVAL(rtcsampler_request_samples) },
  { LNILKEY, LNILVAL }
};

LUALIB_API int luaopen_rtcsampler (lua_State *L)
{
#if LUA_OPTIMIZE_MEMORY > 0
  return 0;
#else
  luaL_register (L, AUXLIB_RTCSAMPLER, rtcsampler_map);
  return 1;
#endif
}
