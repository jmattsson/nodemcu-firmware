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
#include "lua.h"
#include "lauxlib.h"
#include "auxmods.h"
#ifdef LWIP_OPEN_SRC
#include "lwip/ip_addr.h"
#else
#include "ip_addr.h"
#endif
#include "espconn.h"
#include "../crypto/digests.h"

#ifndef RT_MAX_PLAIN_LENGTH
#define RT_MAX_PLAIN_LENGTH 4096
#endif

#define MAX_ERROR_BODY 256

enum { ILI_USER_IDX, ILI_SECRET_IDX, ILI_SERVER_IDX, ILI_MAX_IDX };
static const char *const config_keys[] = { "user", "secret", "server" };
static int config_refs[ILI_MAX_IDX] = { LUA_NOREF, LUA_NOREF, LUA_NOREF };

#define SEND_OFFSET_ALL_DONE -1
#define SEND_OFFSET_ERROR_REPORTED -2
typedef struct
{
  lua_State *L;
  uint32_t tstamp;
  struct espconn conn;
  ip_addr_t dns; // do we need this, or would on the stack be ok?
  int cb_ref;
  int send_offset;
} ili_userdata;


static void ensure_full_config (lua_State *L)
{
  int i;
  for (i = 0; i < ILI_MAX_IDX; ++i)
  {
    if (config_refs[i] == LUA_NOREF)
      luaL_error (L, "missing configuration item: %s", config_keys[i]);
  }
}


static inline lua_State *push_callback (ili_userdata *iliud)
{
  lua_rawgeti (iliud->L, LUA_REGISTRYINDEX, iliud->cb_ref);
  return iliud->L;
}


static void cleanup (ili_userdata *iliud)
{
  lua_State *L = iliud->L;

  // release the associated data-to-be-posted
  lua_pushlightuserdata (L, iliud);
  lua_pushnil (L);
  lua_settable (L, LUA_REGISTRYINDEX);
  // release the callback reference
  luaL_unref (L, LUA_REGISTRYINDEX, iliud->cb_ref);

  espconn_delete (&iliud->conn);

  mem_free (iliud->conn.proto.tcp);
  mem_free (iliud);
}


static void abort_conn (ili_userdata *iliud)
{
  iliud->send_offset = SEND_OFFSET_ERROR_REPORTED;
  espconn_secure_disconnect (&iliud->conn);
}


static void on_connect (void *arg)
{
  ili_userdata *iliud = ((struct espconn *)arg)->reverse;

  lua_State *L = iliud->L;
  lua_pushliteral (L, "POST/api/v2/streams");

  lua_pushlightuserdata (L, iliud);
  lua_gettable (L, LUA_REGISTRYINDEX);

  const digest_mech_info_t *md5 = crypto_digest_mech ("MD5");
  const digest_mech_info_t *sha256 = crypto_digest_mech ("SHA256");

  union {
    uint8_t md5[md5->digest_size *2];
    uint8_t hmac[sha256->digest_size *2];
  } digest;

  size_t body_len;
  const char *samples = lua_tolstring (L, -1, &body_len);
  int err = crypto_hash (md5, samples, body_len, digest.md5);
  crypto_encode_asciihex (digest.md5, md5->digest_size, digest.md5);
  lua_pop (L, 1); // discard md5 input string
  lua_pushlstring (L, digest.md5, sizeof (digest.md5));


  lua_pushnumber (L, iliud->tstamp);
  lua_concat (L, 3); // POSTpath|md5(samples)|timestamp

  size_t len;
  const char *text = lua_tolstring (L, -1, &len);
  lua_rawgeti (L, LUA_REGISTRYINDEX, config_refs[ILI_SECRET_IDX]);
  size_t klen;
  const char *key = lua_tolstring (L, -1, &klen);
  if (!err)
    err = crypto_hmac (sha256, text, len, key, klen, digest.hmac);
  crypto_encode_asciihex (digest.hmac, sha256->digest_size, digest.hmac);
  lua_pop (L, 2); // discard hmac input string and key

  // Would a luaL_Buffer be more efficient than lua_concat?
  lua_pushliteral (L, "POST /api/v2/streams HTTP/1.0\r\nHost: ");
  lua_rawgeti (L, LUA_REGISTRYINDEX, config_refs[ILI_SERVER_IDX]);
  lua_pushliteral (L, "\r\nUnix-time: ");
  lua_pushnumber (L, iliud->tstamp);
  lua_pushliteral (L, "\r\nUser-key: ");
  lua_rawgeti (L, LUA_REGISTRYINDEX, config_refs[ILI_USER_IDX]);
  lua_pushliteral (L, "\r\nUser-token: ");
  lua_pushlstring (L, digest.hmac, sizeof (digest.hmac));
  lua_pushliteral (L, "\r\nContent-length: ");
  lua_pushnumber (L, body_len);
  lua_pushliteral (L, "\r\n\r\n");
  lua_concat (L, 11);
  const char *hdrs = lua_tolstring (L, -1, &len);

  if (!err)
    err = espconn_secure_sent (&iliud->conn, (uint8_t *)hdrs, len);

  lua_pop (L, 1); // restore stack to its original state

  if (err)
  {
    abort_conn (iliud);
    push_callback (iliud);
    lua_pushfstring (L, "headers failed: %d", err);
    lua_call (L, 1, 0);
  }
}


static void on_sent (void *arg)
{
  ili_userdata *iliud = ((struct espconn *)arg)->reverse;
  lua_State *L = iliud->L;

  lua_pushlightuserdata (L, iliud);
  lua_gettable (L, LUA_REGISTRYINDEX);
  size_t body_len;
  const char *samples = lua_tolstring (L, -1, &body_len);

  if (iliud->send_offset >= body_len)
  {
    lua_pop (L, 1);
    return; // nothing else to send, just wait for response
  }
  else
  {
    int len = body_len > RT_MAX_PLAIN_LENGTH ? RT_MAX_PLAIN_LENGTH : body_len;
    int err = espconn_secure_sent (&iliud->conn, (uint8_t *)samples, len);
    lua_pop (L, 1);

    if (!err)
      iliud->send_offset += len;
    else
    {
      abort_conn (iliud);
      push_callback (iliud);
      lua_pushfstring (L, "body failed at %d: %d", iliud->send_offset, err);
      lua_call (L, 1, 0);
    }
  }
}


static void on_recv (void *arg, char *data, unsigned short len)
{
  ili_userdata *iliud = ((struct espconn *)arg)->reverse;

  // Check for "HTTP/1.x 20x "
  if (len >= 13 &&
      data[0] == 'H' && data[1] == 'T' && data[2] == 'T' && data[3] == 'P' &&
      data[4] == '/' && data[5] == '1' && data[6] == '.' && /* skip minor */
      data[8] == ' ' && data[9] == '2' && data[10] == '0' && /* skip 0/1 */
      data[12] == ' ')
  {
    iliud->send_offset = SEND_OFFSET_ALL_DONE;
    espconn_secure_disconnect (&iliud->conn);
  }
  else
  {
    abort_conn (iliud);
    lua_State *L = push_callback (iliud);
    lua_pushlstring (L, data, len > MAX_ERROR_BODY ? MAX_ERROR_BODY : len);
    lua_call (L, 1, 0);
  }
}

static void on_reconnect (void *arg, sint8 err)
{
  ili_userdata *iliud = ((struct espconn *)arg)->reverse;

  lua_State *L = push_callback (iliud);
  cleanup (iliud); // all over, okay to cleanup
  lua_pushfstring (L, "post failed: %d", (int)err);
  lua_call (L, 1, 0);
}


static void on_disconnect (void *arg)
{
  ili_userdata *iliud = ((struct espconn *)arg)->reverse;

  lua_State *L = iliud->L;
  switch (iliud->send_offset)
  {
    case SEND_OFFSET_ALL_DONE:
      push_callback (iliud);
      cleanup (iliud);
      lua_call (L, 0, 0); // success!
      break;
    case SEND_OFFSET_ERROR_REPORTED:
      cleanup (iliud);
      break;
    default:
      push_callback (iliud);
      cleanup (iliud);
      lua_pushliteral (L, "unexpected disconnect");
      lua_call (L, 1, 0);
      break;
  }
}


static void on_dns_found (const char *name, ip_addr_t *ip, void *arg)
{
  (void)name;
  ili_userdata *iliud = ((struct espconn *)arg)->reverse;
  lua_State *L = push_callback (iliud);
  if (ip)
  {
    os_memcpy (&iliud->conn.proto.tcp->remote_ip, ip, 4);
    int res = espconn_secure_connect (&iliud->conn);
    if (res == 0)
    {
      lua_pop (L, 1); // didn't need the callback after all
      return;
    }
    else
      lua_pushfstring (iliud->L, "connect failed: %d", res);
  }
  else
    lua_pushliteral (iliud->L, "DNS failed: host not found");

  cleanup (iliud); // no socket active, okay to cleanup
  lua_call (L, 1, 0);
}


static int intelligentli_config (lua_State *L)
{
  lua_pushnil (L); // nil key for start of iteration
  while (lua_next (L, 1) != 0)
  {
    int pop = 1;
    if (lua_type (L, -2) == LUA_TSTRING && lua_type (L, -1) == LUA_TSTRING)
    {
      const char *k = lua_tostring (L, -2);
      int i;
      for (i = 0; i < ILI_MAX_IDX; ++i)
      {
        if (strcmp (k, config_keys[i]) == 0)
        {
          luaL_unref (L, LUA_REGISTRYINDEX, config_refs[i]);
          config_refs[i] = luaL_ref (L, LUA_REGISTRYINDEX);
          pop = 0;
          break;
        }
      }
    }
    if (pop)
      lua_pop (L, 1); // leave key to continue iteration
  }
  return 0;
}

static int intelligentli_post (lua_State *L)
{
  ensure_full_config (L);
  luaL_checknumber (L, 1);
  luaL_checkstring (L, 2);
  luaL_checkanyfunction (L, 3);

  ili_userdata *iliud = (ili_userdata *)mem_zalloc (sizeof (ili_userdata));
  iliud->L = L;
  iliud->tstamp = lua_tonumber (L, 1);
  iliud->cb_ref = LUA_NOREF;
  iliud->conn.type = ESPCONN_TCP;
  iliud->conn.proto.tcp = (esp_tcp *)mem_zalloc (sizeof (esp_tcp));
  iliud->conn.proto.tcp->remote_port = 443;
  iliud->conn.reverse = iliud;
  espconn_regist_reconcb   (&iliud->conn, on_reconnect);
  espconn_regist_connectcb (&iliud->conn, on_connect);
  espconn_regist_disconcb  (&iliud->conn, on_disconnect);
  espconn_regist_recvcb    (&iliud->conn, on_recv);
  espconn_regist_sentcb    (&iliud->conn, on_sent);

  // TODO: check against system_get_free_heap_size()

  lua_rawgeti (L, LUA_REGISTRYINDEX, config_refs[ILI_SERVER_IDX]);
  const char *server = lua_tostring (L, -1);
  int res = espconn_gethostbyname (
    &iliud->conn, server, &iliud->dns, on_dns_found);
  lua_pop (L, 1);

  switch (res)
  {
    case ESPCONN_OK: // already resolved!
    case ESPCONN_INPROGRESS: break;
    default:
      mem_free (iliud->conn.proto.tcp);
      mem_free (iliud);
      return luaL_error (L, "DNS lookup error: %d", res);
  }

  // Store the data to be posted
  lua_pushlightuserdata (L, iliud);
  lua_pushvalue (L, 2);
  lua_settable (L, LUA_REGISTRYINDEX);


  lua_pushvalue (L, 3);
  iliud->cb_ref = luaL_ref (L, LUA_REGISTRYINDEX);

  if (res == ESPCONN_OK) // synthesize DNS callback
    on_dns_found (0, &iliud->dns, iliud);

  return 0;
}


#define MIN_OPT_LEVEL 2
#include "lrodefs.h"
const LUA_REG_TYPE intelligentli_map[] =
{
  { LSTRKEY( "config" ), LFUNCVAL( intelligentli_config ) },
  { LSTRKEY( "post"   ), LFUNCVAL( intelligentli_post   ) },
  { LNILKEY, LNILVAL }
};

LUALIB_API int luaopen_intelligentli (lua_State *L)
{
  LREGISTER (L, AUXLIB_INTELLIGENTLI, intelligentli_map);
}
