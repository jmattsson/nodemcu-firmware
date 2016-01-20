/*
 * Copyright 2016 Dius Computing Pty Ltd. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * - Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the
 *   distribution.
 * - Neither the name of the copyright holders nor the names of
 *   its contributors may be used to endorse or promote products derived
 *   from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL
 * THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * @author Johny Mattsson <jmattsson@dius.com.au>
 */
#include "module.h"
#include "lauxlib.h"
#include "user_interface.h"

static int cb_ref = LUA_NOREF;


static void locp_cb (uint8 *frm, int len, int rssi)
{
  if (cb_ref != LUA_NOREF)
  {
    lua_State *L = lua_getstate ();
    lua_rawgeti (L, LUA_REGISTRYINDEX, cb_ref);
    lua_pushlstring (L, frm, len);
    lua_pushinteger (L, rssi);
    lua_call (L, 2, 0);
  }
}


static int locp_register (lua_State *L)
{
  if (lua_type (L, 1) != LUA_TFUNCTION && lua_type (L, 1) != LUA_TLIGHTFUNCTION)
    return luaL_error (L, "expected callback arg");

  luaL_unref (L, LUA_REGISTRYINDEX, cb_ref);

  lua_pushvalue (L, 1);
  cb_ref = luaL_ref (L, LUA_REGISTRYINDEX);

  wifi_register_rfid_locp_recv_cb (locp_cb);

  int ret = wifi_rfid_locp_recv_open ();
  if (ret != 0)
    return luaL_error (L, "failed to start listening for LOCP frames: %u\n", ret);

  return 0;
}


static int locp_unregister (lua_State *L)
{
  wifi_rfid_locp_recv_close ();
  wifi_unregister_rfid_locp_recv_cb ();
  luaL_unref (L, LUA_REGISTRYINDEX, cb_ref);
  cb_ref = LUA_NOREF;
  return 0;
}


static const LUA_REG_TYPE locp_map[] =
{
  { LSTRKEY("register"),    LFUNCVAL(locp_register) },
  { LSTRKEY("unregister"),  LFUNCVAL(locp_unregister) },

  { LNILKEY, LNILVAL }
};

NODEMCU_MODULE(LOCP, "locp", locp_map, NULL);
