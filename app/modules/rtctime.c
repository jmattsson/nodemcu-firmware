// Module for RTC time keeping

/*
 * Copyright (c) 2015, DiUS Computing Pty Ltd (jmattsson@dius.com.au)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTOR(S) ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTOR(S) BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include "module.h"
#include "lauxlib.h"

#include "rtc/rtctime_internal.h"
#include "rtc/rtctime.h"


/* seconds per day */
#define SPD 24*60*60

/* days per month -- nonleap! */
static const short __spm[13] =
  { 0,
    (31),
    (31+28),
    (31+28+31),
    (31+28+31+30),
    (31+28+31+30+31),
    (31+28+31+30+31+30),
    (31+28+31+30+31+30+31),
    (31+28+31+30+31+30+31+31),
    (31+28+31+30+31+30+31+31+30),
    (31+28+31+30+31+30+31+31+30+31),
    (31+28+31+30+31+30+31+31+30+31+30),
    (31+28+31+30+31+30+31+31+30+31+30+31),
  };

static int __isleap (int year) {
  /* every fourth year is a leap year except for century years that are
   * not divisible by 400. */
  /*  return (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0)); */
  return (!(year % 4) && ((year % 100) || !(year % 400)));
}

// ******* C API functions *************
void __attribute__((noreturn)) TEXT_SECTION_ATTR rtc_time_enter_deep_sleep_final (void)
{
  ets_intr_lock();
  Cache_Read_Disable();
  rtc_reg_write(0x18,8);
  rtc_reg_write_and_loop(0x08,0x00100000); //  go to sleep
  __builtin_unreachable();
}

void rtctime_early_startup (void)
{
  Cache_Read_Enable (0, 0, 1);
  rtc_time_register_bootup ();
  rtc_time_switch_clocks ();
  Cache_Read_Disable ();
}

void rtctime_late_startup (void)
{
  rtc_time_switch_system ();
}

void rtctime_gettimeofday (struct rtc_timeval *tv)
{
  rtc_time_gettimeofday (tv);
}

void rtctime_settimeofday (const struct rtc_timeval *tv)
{
  if (!rtc_time_check_magic ())
    rtc_time_prepare ();
  rtc_time_settimeofday (tv);
}

bool rtctime_have_time (void)
{
  return rtc_time_have_time ();
}

void rtctime_deep_sleep_us (uint32_t us)
{
  rtc_time_deep_sleep_us (us);
}

void rtctime_deep_sleep_until_aligned_us (uint32_t align_us, uint32_t min_us, uint32_t rand_us)
{
  rtc_time_deep_sleep_until_aligned (align_us, min_us,rand_us);
}

void rtctime_gmtime (const int32 stamp, struct rtc_tm *r)
{
  int32_t i;
  int32_t work = stamp % (SPD);
  r->tm_sec = work % 60; work /= 60;
  r->tm_min = work % 60; r->tm_hour = work / 60;
  work = stamp / (SPD);
  r->tm_wday = (4 + work) % 7;
  for (i = 1970; ; ++i) {
    int32_t k = __isleap (i) ? 366 : 365;
    if (work >= k) {
      work -= k;
    } else {
      break;
    }
  }
  r->tm_year = i - 1900;
  r->tm_yday = work;

  r->tm_mday = 1;
  if (__isleap (i) && (work > 58)) {
    if (work == 59) r->tm_mday = 2; /* 29.2. */
    work -= 1;
  }

  for (i = 11; i && (__spm[i] > work); --i) ;
  r->tm_mon = i;
  r->tm_mday += work - __spm[i];
}



// ******* Lua API functions *************

//  rtctime.set (sec, usec)
static int rtctime_set (lua_State *L)
{
  if (!rtc_time_check_magic ())
    rtc_time_prepare ();

  uint32_t sec = luaL_checknumber (L, 1);
  uint32_t usec = 0;
  if (lua_isnumber (L, 2))
    usec = lua_tonumber (L, 2);

  struct rtc_timeval tv = { sec, usec };
  rtctime_settimeofday (&tv);
  return 0;
}


// sec, usec = rtctime.get ()
static int rtctime_get (lua_State *L)
{
  struct rtc_timeval tv;
  rtctime_gettimeofday (&tv);
  lua_pushnumber (L, tv.tv_sec);
  lua_pushnumber (L, tv.tv_usec);
  return 2;
}

static void do_sleep_opt (lua_State *L, int idx)
{
  if (lua_isnumber (L, idx))
  {
    uint32_t opt = lua_tonumber (L, idx);
    if (opt < 0 || opt > 4)
      luaL_error (L, "unknown sleep option");
    system_deep_sleep_set_option (opt);
  }
}

// rtctime.dsleep (usec, option)
static int rtctime_dsleep (lua_State *L)
{
  uint32_t us = luaL_checknumber (L, 1);
  do_sleep_opt (L, 2);
  rtctime_deep_sleep_us (us); // does not return
  return 0;
}


// rtctime.dsleep_aligned (aligned_usec, min_usec, option)
static int rtctime_dsleep_aligned (lua_State *L)
{
  if (!rtctime_have_time ())
    return luaL_error (L, "time not available, unable to align");

  uint32_t align_us = luaL_checknumber (L, 1);
  uint32_t min_us = luaL_checknumber (L, 2);
  uint32_t rand_us = 0;
  if (lua_isnumber (L, 3))
    rand_us = lua_tonumber (L, 3);

  do_sleep_opt (L, 3);
  rtctime_deep_sleep_until_aligned_us (align_us, min_us, rand_us); // does not return
  return 0;
}


#define ADD_TABLE_ITEM(L, key, val) \
  lua_pushinteger (L, val);	    \
  lua_setfield (L, -2, key);

// rtctime.epoch2cal (stamp)
static int rtctime_epoch2cal (lua_State *L)
{
  struct rtc_tm date;
  int32_t stamp = luaL_checkint (L, 1);
  luaL_argcheck (L, stamp >= 0, 1, "wrong arg range");

  rtctime_gmtime (stamp, &date);

  /* construct Lua table */
  lua_createtable (L, 0, 8);
  ADD_TABLE_ITEM (L, "yday", date.tm_yday + 1);
  ADD_TABLE_ITEM (L, "wday", date.tm_wday + 1);
  ADD_TABLE_ITEM (L, "year", date.tm_year + 1900);
  ADD_TABLE_ITEM (L, "mon",  date.tm_mon + 1);
  ADD_TABLE_ITEM (L, "day",  date.tm_mday);
  ADD_TABLE_ITEM (L, "hour", date.tm_hour);
  ADD_TABLE_ITEM (L, "min",  date.tm_min);
  ADD_TABLE_ITEM (L, "sec",  date.tm_sec);

  return 1;
}

// Module function map
static const LUA_REG_TYPE rtctime_map[] = {
  { LSTRKEY("set"),            LFUNCVAL(rtctime_set) },
  { LSTRKEY("get"),            LFUNCVAL(rtctime_get) },
  { LSTRKEY("dsleep"),         LFUNCVAL(rtctime_dsleep)  },
  { LSTRKEY("dsleep_aligned"), LFUNCVAL(rtctime_dsleep_aligned) },
  { LSTRKEY("epoch2cal"),      LFUNCVAL(rtctime_epoch2cal) },
  { LNILKEY, LNILVAL }
};

NODEMCU_MODULE(RTCTIME, "rtctime", rtctime_map, NULL);
