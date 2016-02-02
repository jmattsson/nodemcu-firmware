#include "module.h"
#include "lua.h"
#include "lauxlib.h"
#ifdef LWIP_OPEN_SRC
#include "lwip/ip_addr.h"
#else
#include "ip_addr.h"
#endif
#include "espconn.h"
#include "mem.h"
#include "../crypto/sha2.h"
#include "../crypto/digests.h"
#include "strbuffer.h"

#define PAYLOAD_LIMIT 1400

#define lstrbuffer_append(x,...) do { if (!strbuffer_append(x,__VA_ARGS__)) luaL_error(sud->L, "no mem"); } while (0)
#define lstrbuffer_add(x,...) do { if (!strbuffer_add(x,__VA_ARGS__)) luaL_error(sud->L, "no mem"); } while (0)

typedef int8_t (*conn_function_t)(struct espconn *conn);
typedef int8_t (*send_function_t)(struct espconn *conn, const void *data, uint16_t len);

typedef struct
{
  conn_function_t connect;
  conn_function_t disconnect;
  send_function_t send;
} esp_funcs_t;

static const esp_funcs_t esp_plain = {
  .connect = espconn_connect,
  // FIXME: need to post the disconnect
  .disconnect = espconn_disconnect,
  .send = (send_function_t)espconn_send
};

static const esp_funcs_t esp_secure = {
  .connect = espconn_secure_connect,
  // FIXME: need to post the disconnect
  .disconnect = espconn_secure_disconnect,
  .send = (send_function_t)espconn_secure_send
};

typedef struct
{
  lua_State *L;
  strbuffer_t *buffer;
  struct espconn conn;
  const esp_funcs_t *funcs;
  ip_addr_t dns;
  int user_ref;
  int key_ref;
  int iter_ref;
  int cb_ref;
  int token_ref;
  int dict_ref;
  int work_ref;
  int err_ref;

  enum {
    S4PP_INIT,
    S4PP_HELLO,
    S4PP_AUTHED,
    S4PP_BUFFERING,
    S4PP_COMMITTING,
    S4PP_DONE,
    S4PP_ERRORED
  } state;

  char *recv_buf;
  uint16_t recv_len;

  int next_idx;
  uint16_t next_seq;
  uint16_t n_max;
  uint16_t n_sent;
  uint32_t lasttime;
  SHA256_CTX ctx;
  bool end_of_data;
} s4pp_userdata;


#define goto_err_with_msg(L, ...) \
  do { \
    lua_pushfstring(L, __VA_ARGS__); \
    goto err; \
  } while (0)


static void make_hmac_pad (s4pp_userdata *sud, uint8_t padval)
{
  lua_State *L = sud->L;
  lua_rawgeti (L, LUA_REGISTRYINDEX, sud->key_ref);
  size_t klen;
  const char *key = lua_tolstring (L, -1, &klen);
  char altkey[SHA256_DIGEST_LENGTH];
  if (klen > SHA256_BLOCK_LENGTH)
  {
    SHA256_CTX ctx;
    SHA256_Init (&ctx);
    SHA256_Update (&ctx, key, klen);
    SHA256_Final (altkey, &ctx);
    key = altkey;
    klen = SHA256_DIGEST_LENGTH;
  }

  uint8_t pad[SHA256_BLOCK_LENGTH];
  os_memset (pad, padval, sizeof (pad));
  unsigned i;
  for (i = 0; i < klen; ++i)
    pad[i] ^= key[i];

  lua_pop (L, 1);
  lua_pushlstring (L, pad, sizeof (pad)); // ..and put the pad on the stack
}


static void update_hmac (s4pp_userdata *sud)
{
  lua_State *L = sud->L;
  size_t len;
  const char *data = lua_tolstring (L, -1, &len);
  SHA256_Update (&sud->ctx, data, len);
}

static void update_hmac_from_buffer (s4pp_userdata *sud)
{
  size_t len;
  const char *data = strbuffer_str (sud->buffer, &len);
  SHA256_Update (&sud->ctx, data, len);
}

static void init_hmac (s4pp_userdata *sud)
{
  SHA256_Init (&sud->ctx);
  make_hmac_pad (sud, 0x36);
  update_hmac (sud);
  lua_pop (sud->L, 1); // drop the pad
}


static void append_final_hmac_hex (s4pp_userdata *sud)
{
  uint8_t digest[SHA256_DIGEST_LENGTH*2];
  SHA256_Final (digest, &sud->ctx);
  SHA256_Init (&sud->ctx);
  make_hmac_pad (sud, 0x5c);
  update_hmac (sud);
  lua_pop (sud->L, 1); // drop the pad
  SHA256_Update (&sud->ctx, digest, SHA256_DIGEST_LENGTH);
  SHA256_Final (digest, &sud->ctx);
  crypto_encode_asciihex (digest, SHA256_DIGEST_LENGTH, digest);
  lstrbuffer_append (sud->buffer, digest, sizeof (digest));
}


static lua_State *push_callback (s4pp_userdata *sud)
{
  lua_rawgeti (sud->L, LUA_REGISTRYINDEX, sud->cb_ref);
  return sud->L;
}


static void cleanup (s4pp_userdata *sud)
{
  lua_State *L = sud->L;

  luaL_unref (L, LUA_REGISTRYINDEX, sud->cb_ref);
  luaL_unref (L, LUA_REGISTRYINDEX, sud->token_ref);
  luaL_unref (L, LUA_REGISTRYINDEX, sud->user_ref);
  luaL_unref (L, LUA_REGISTRYINDEX, sud->key_ref);
  luaL_unref (L, LUA_REGISTRYINDEX, sud->iter_ref);
  luaL_unref (L, LUA_REGISTRYINDEX, sud->dict_ref);
  luaL_unref (L, LUA_REGISTRYINDEX, sud->work_ref);
  luaL_unref (L, LUA_REGISTRYINDEX, sud->err_ref);

  espconn_delete (&sud->conn);

  strbuffer_free (sud->buffer);

  os_free (sud->conn.proto.tcp);
  os_free (sud->recv_buf);
  os_free (sud);
}


static void abort_conn (s4pp_userdata *sud)
{
  sud->state = S4PP_ERRORED;
  sud->err_ref = luaL_ref (sud->L, LUA_REGISTRYINDEX);
  sud->funcs->disconnect (&sud->conn);
}


static void prepare_seq_hmac (s4pp_userdata *sud)
{
  init_hmac (sud);
  lua_State *L = sud->L;
  lua_rawgeti (L, LUA_REGISTRYINDEX, sud->token_ref);
  update_hmac (sud);
  lua_pop (L, 1);
}


static void handle_auth (s4pp_userdata *sud, char *token, uint16_t len)
{
  lua_State *L = sud->L;

  lua_checkstack (L, 5);
  lua_pushlstring (L, token, len);
  sud->token_ref = luaL_ref (L, LUA_REGISTRYINDEX);
  lua_rawgeti (L, LUA_REGISTRYINDEX, sud->user_ref);
  lua_pushlstring (L, token, len);
  lua_concat (L, 2);
  size_t slen;
  const char *str = lua_tolstring (L, -1, &slen);

  lua_rawgeti (L, LUA_REGISTRYINDEX, sud->key_ref);
  size_t klen;
  const char *key = lua_tolstring (L, -1, &klen);
  const digest_mech_info_t *hmac256 = crypto_digest_mech ("SHA256");
  uint8_t digest[hmac256->digest_size * 2];
  os_memset (digest, 0, sizeof (digest));
  crypto_hmac (hmac256, str, slen, key, klen, digest);
  crypto_encode_asciihex (digest, hmac256->digest_size, digest);

  lua_pop (L, 2);
  lua_pushliteral (L, "AUTH:SHA256,");
  lua_rawgeti (L, LUA_REGISTRYINDEX, sud->user_ref);
  lua_pushliteral (L, ",");
  lua_pushlstring (L, digest, sizeof (digest));
  lua_pushliteral (L, "\n");
  lua_concat (L, 5);
  size_t alen;
  const char *auth = lua_tolstring (L, -1, &alen);
  int err = sud->funcs->send (&sud->conn, (uint8_t *)auth, alen);
  lua_pop (L, 1);
  if (err)
    goto_err_with_msg (sud->L, "auth send failed: %d", err);

  sud->state = S4PP_AUTHED;
  prepare_seq_hmac (sud);

  return;
err:
  abort_conn (sud);
}


// top of stack = { name=... }
static int get_dict_idx (s4pp_userdata *sud)
{
  lua_State *L = sud->L;
  int ret;
  int top = lua_gettop (L);

  lua_rawgeti (L, LUA_REGISTRYINDEX, sud->dict_ref);
  lua_getfield (L, -2, "name");
  if (!lua_isstring (L, -1))
    ret = -2;
  else
  {
    lua_gettable (L, -2);
    if (lua_isnumber (L, -1))
      ret = lua_tonumber (L, -1);
    else
      ret = -1;
  }
  lua_settop (L, top);
  return ret;
}


static void get_optional_field (lua_State *L, int table, const char *key, const char *dfl)
{
  lua_getfield (L, table, key);
  if (lua_isnoneornil (L, -1))
  {
    lua_pop (L, 1);
    lua_pushstring (L, dfl);
  }
}


// top of stack = { name=..., unit=..., unitdiv=... }
static void prepare_dict (s4pp_userdata *sud)
{
  lua_State *L = sud->L;
  int sample_table = lua_gettop (L);
  lua_checkstack (L, 9);

  int idx = sud->next_idx++;
  lua_rawgeti (L, LUA_REGISTRYINDEX, sud->dict_ref);
  lua_getfield (L, sample_table, "name"); // we know this exists by now
  lua_pushinteger (L, idx);
  lua_settable (L, -3);
  lua_pop (L, 1); // drop dict from stack

  lua_pushliteral (L, "DICT:");
  lua_pushinteger (L, idx);
  lua_pushliteral (L, ",");
  get_optional_field (L, sample_table, "unit", "");
  lua_pushliteral (L, ",");
  get_optional_field (L, sample_table, "unitdiv", "1");
  lua_pushliteral (L, ",");
  lua_getfield (L, sample_table, "name");
  lua_pushliteral (L, "\n");
  lua_concat (L, 9); // DICT:<idx>,<unit>,<unitdiv>,<name>\n
  size_t len;
  const char *str = lua_tolstring (L, -1, &len);

  lstrbuffer_append (sud->buffer, str, len);
  lua_pop (L, 1);
}


// top of stack = { time=..., value=... }
static bool prepare_data (s4pp_userdata *sud, int idx)
{
  lua_State *L = sud->L;
  int sample_table = lua_gettop (L);
  lua_checkstack (L, 6);

  lua_getfield (L, sample_table, "time");
  if (!lua_isnumber (L, -1))
    goto failed;
  uint32_t timestamp = lua_tonumber (L, -1);
  int delta_t = timestamp - sud->lasttime;
  sud->lasttime = timestamp;
  lua_pop (L, 1);

  lua_pushinteger (L, idx);
  lua_pushliteral (L, ",");
  lua_pushinteger (L, delta_t);
  lua_pushliteral (L, ",");
  lua_getfield (L, sample_table, "value");
  if (!lua_isnumber (L, -1))
    goto failed;
  lua_pushliteral (L, "\n");

  lua_concat (L, 6);
  size_t len;
  const char *str = lua_tolstring (L, -1, &len);

  lstrbuffer_append (sud->buffer, str, len);
  lua_pop (L, 1);

  return true;
failed:
  lua_settop (L, sample_table);
  return false;
}


static void progress_work (s4pp_userdata *sud)
{
  lua_State *L = sud->L;
  switch (sud->state)
  {
    case S4PP_AUTHED:
    {
      lstrbuffer_add (sud->buffer, "SEQ:%u,0,1,0\n", sud->next_seq++); // seq:N time:0 timediv:1 datafmt:0
      sud->next_idx = 0;
      sud->n_sent = 0;
      sud->lasttime = 0;
      luaL_unref (L, LUA_REGISTRYINDEX, sud->dict_ref);
      lua_newtable (L);
      sud->dict_ref = luaL_ref (L, LUA_REGISTRYINDEX);
      sud->state = S4PP_BUFFERING;
      // fall through
    }
    case S4PP_BUFFERING:
    {
      size_t sz_estimate;
      while (strbuffer_str (sud->buffer, &sz_estimate) &&
             sz_estimate < PAYLOAD_LIMIT &&
             sud->state == S4PP_BUFFERING)
      {
        if (!lua_checkstack (L, 1))
          goto_err_with_msg (L, "out of stack");

        bool sig = false;
        if (sud->n_sent >= sud->n_max)
          sig = true;
        else
        {
          if (sud->work_ref != LUA_NOREF) // did dict last, now do data
          {
            lua_rawgeti (L, LUA_REGISTRYINDEX, sud->work_ref);
            luaL_unref (L, LUA_REGISTRYINDEX, sud->work_ref);
            sud->work_ref = LUA_NOREF;
          }
          else // get the next sample
          {
            lua_rawgeti (L, LUA_REGISTRYINDEX, sud->iter_ref);
            lua_call (L, 0, 1);
          }
          if (lua_istable (L, -1))
          {
            // send dict and/or data
            int idx = get_dict_idx (sud);
            if (idx == -2)
              goto_err_with_msg (L, "no 'name'");
            else if (idx == -1)
            {
              lua_pushvalue (L, -1); // store "copy" of sample for data round
              sud->work_ref = luaL_ref (L, LUA_REGISTRYINDEX);
              prepare_dict (sud);
            }
            else
            {
              if (!prepare_data (sud, idx))
                goto_err_with_msg (L, "no 'time' or 'value'");
              ++sud->n_sent;
            }
            lua_pop (L, 1); // drop table
          }
          else if (lua_isnoneornil (L, -1))
          {
            sig = true;
            lua_pop (L, 1); // TODO: check if this is ok with noneornil
            sud->end_of_data = true;
          }
          else
            goto_err_with_msg (L, "iterator returned garbage");
        }
        if (sig)
        {
          update_hmac_from_buffer (sud);
          lstrbuffer_add (sud->buffer, "SIG:");
          append_final_hmac_hex (sud);
          lstrbuffer_add (sud->buffer, "\n");
          sud->state = S4PP_COMMITTING;
        }
      }

      if (sud->state != S4PP_COMMITTING)
        update_hmac_from_buffer (sud);

      size_t len;
      char *str = strbuffer_str (sud->buffer, &len);
      int res = sud->funcs->send (&sud->conn, str, len);
      strbuffer_reset (sud->buffer);

      if (res != 0)
        goto_err_with_msg (L, "send failed: %d", res);
      break;
    }
    case S4PP_COMMITTING:
      break; // just waiting for OK/NOK now
    default:
      goto_err_with_msg (L, "bad state: %d", sud->state);
  }
  return;
err:
  abort_conn (sud);
}


static bool handle_line (s4pp_userdata *sud, char *line, uint16_t len)
{
  if (line[len -1] == '\n')
    line[len -1] = 0;
  else
    goto_err_with_msg (sud->L, "missing newline");
  if (strncmp ("S4PP/", line, 5) == 0)
  {
    // S4PP/x.y <algos,algo..> <max_samples>
    if (sud->state > S4PP_INIT)
      goto_err_with_msg (sud->L, "unexpected S4pp hello");

    char *algos = strchr (line, ' ');
    if (!algos || !strstr (algos, "SHA256"))
      goto_err_with_msg (sud->L, "server does not support SHA256");

    char *maxn = strchr (algos + 1, ' ');
    if (maxn)
      sud->n_max = strtol (maxn, NULL, 0);
    if (!sud->n_max)
      goto_err_with_msg (sud->L, "bad hello");

    sud->state = S4PP_HELLO;
  }
  else if (strncmp ("TOK:", line, 4) == 0)
  {
    if (sud->state == S4PP_HELLO)
      handle_auth (sud, line + 4, len - 5); // ditch \0
    else
      goto_err_with_msg (sud->L, "bad tok");
  }
  else if (strncmp ("REJ:", line, 4) == 0)
    goto_err_with_msg (sud->L, "protocol error: %s", line + 4);
  else if (strncmp ("NOK:", line, 4) == 0)
    // we don't pipeline, so don't need to check the seqno
    goto_err_with_msg (sud->L, "commit failed");
  else if (strncmp ("OK:", line, 3) == 0)
  {
    if (sud->end_of_data)
    {
      sud->state = S4PP_DONE;
      sud->funcs->disconnect (&sud->conn);
    }
    else
    {
      sud->state = S4PP_AUTHED;
      prepare_seq_hmac (sud);
      progress_work (sud);
    }
  }
  else
    goto_err_with_msg (sud->L, "unexpected response: %s", line);
  return true;
err:
  abort_conn (sud);
  return false;
}


static void on_recv (void *arg, char *data, uint16_t len)
{
  if (!len)
    return;

  s4pp_userdata *sud = ((struct espconn *)arg)->reverse;

  char *nl = memchr (data, '\n', len);

  // deal with joining with previous chunk
  if (sud->recv_len)
  {
    char *end = nl ? nl : data + len -1;
    uint16_t dlen = (end - data);
    uint16_t newlen = sud->recv_len + dlen;
    char *p = (char *)os_realloc (sud->recv_buf, newlen);
    if (!p)
    {
      os_free (sud->recv_buf);
      sud->recv_buf = 0;
      sud->recv_len = 0;
      goto_err_with_msg (sud->L, "no memory for recv buffer");
    }
    else
      sud->recv_buf = p;
    os_memcpy (sud->recv_buf + sud->recv_len, data, newlen - sud->recv_len);
    sud->recv_len = newlen;
    data += dlen;
    len -= dlen;

    if (nl)
    {
      if (!handle_line (sud, sud->recv_buf, sud->recv_len))
        return; // we've ditched the connection
      else
      {
        os_free (sud->recv_buf);
        sud->recv_buf = 0;
        sud->recv_len = 0;
        nl = memchr (data, '\n', len);
      }
    }
  }
  // handle full lines inside 'data'
  while (nl)
  {
    uint16_t dlen = (nl - data) +1;
    if (!handle_line (sud, data, dlen))
      return;

    data += dlen;
    len -= dlen;
    nl = memchr (data, '\n', len);
  }

  // deal with left-over pieces
  if (len)
  {
    sud->recv_buf = (char *)os_malloc (len);
    if (!sud->recv_buf)
      goto_err_with_msg (sud->L, "no memory for recv buffer");
    sud->recv_len = len;
    os_memcpy (sud->recv_buf, data, len);
  }
  return;

err:
  abort_conn (sud);
}


static void on_sent (void *arg)
{
  s4pp_userdata *sud = ((struct espconn *)arg)->reverse;
  progress_work (sud);
}


static void on_disconnect (void *arg)
{
  s4pp_userdata *sud = ((struct espconn *)arg)->reverse;
  lua_State *L = push_callback (sud);
  if (sud->state == S4PP_DONE)
  {
    cleanup (sud);
    lua_call (L, 0, 0);
  }
  else
  {
    if (sud->err_ref != LUA_NOREF)
      lua_rawgeti (L, LUA_REGISTRYINDEX, sud->err_ref);
    else
      lua_pushstring (L, "unexpected disconnect");
    cleanup (sud);
    lua_call (L, 1, 0);
  }
}


static void on_reconnect (void *arg, int8_t err)
{
  s4pp_userdata *sud = ((struct espconn *)arg)->reverse;
  lua_State *L = push_callback (sud);
  cleanup (sud);
  lua_pushfstring (L, "error: %d", err);
  lua_call (L, 1, 0);
}


static void on_dns_found (const char *name, ip_addr_t *ip, void *arg)
{
  (void)name;
  s4pp_userdata *sud = ((struct espconn *)arg)->reverse;
  lua_State *L = push_callback (sud);
  if (ip)
  {
    os_memcpy (&sud->conn.proto.tcp->remote_ip, ip, 4);
    int res = sud->funcs->connect (&sud->conn);
    if (res == 0)
    {
      lua_pop (L, 1);
      return;
    }
    else
      lua_pushfstring (L, "connect failed: %d", res);
  }
  else
    lua_pushliteral (L, "DNS failed: host not found");

  cleanup (sud);
  lua_call (L, 1, 0);
}


// s4pp.upload({server:, port:, secure:, user:, key:}, iterator, callback)
static int s4pp_do_upload (lua_State *L)
{
  luaL_checktype(L, 1, LUA_TTABLE);
  luaL_checkanyfunction (L, 2);
  luaL_checkanyfunction (L, 3);

  const char *err_msg = 0;
#define err_out(msg) do { err_msg = msg; goto err; } while (0)

  s4pp_userdata *sud = (s4pp_userdata *)os_zalloc (sizeof (s4pp_userdata));
  if (!sud)
    err_out ("no memory");
  sud->buffer = strbuffer_create (PAYLOAD_LIMIT);
  if (!sud->buffer)
    err_out ("no memory");
  sud->L = L;
  sud->cb_ref = sud->user_ref = sud->key_ref = sud->token_ref = sud->err_ref = sud->work_ref = sud->dict_ref = LUA_NOREF;
  // TODO: also support a progress callback for each seq commit?

  lua_getfield (L, 1, "user");
  if (!lua_isstring (L, -1))
    err_out ("no 'user' cfg");
  sud->user_ref = luaL_ref (L, LUA_REGISTRYINDEX);

  lua_getfield (L, 1, "key");
  if (!lua_isstring (L, -1))
    err_out ("no 'key' cfg");
  sud->key_ref = luaL_ref (L, LUA_REGISTRYINDEX);

  sud->conn.type = ESPCONN_TCP;
  sud->conn.proto.tcp = (esp_tcp *)os_zalloc (sizeof (esp_tcp));
  if (!sud->conn.proto.tcp)
    err_out ("no memory");

  lua_getfield (L, 1, "port");
  if (lua_isnumber (L, -1))
    sud->conn.proto.tcp->remote_port = lua_tonumber (L, -1);
  else
    sud->conn.proto.tcp->remote_port = 22226;
  lua_pop (L, 1);

  sud->conn.reverse = sud;
  espconn_regist_disconcb  (&sud->conn, on_disconnect);
  espconn_regist_reconcb   (&sud->conn, on_reconnect);
  espconn_regist_recvcb    (&sud->conn, on_recv);
  espconn_regist_sentcb    (&sud->conn, on_sent);

  lua_getfield (L, 1, "secure");
  if (lua_isnumber (L, -1) && lua_tonumber (L, -1) > 0)
    sud->funcs = &esp_secure;
  else
    sud->funcs = &esp_plain;
  lua_pop (L, 1);

  lua_pushvalue (L, 2);
  sud->iter_ref = luaL_ref (L, LUA_REGISTRYINDEX);
  lua_pushvalue (L, 3);
  sud->cb_ref = luaL_ref (L, LUA_REGISTRYINDEX);

  lua_getfield (L, 1, "server");
  if (!lua_isstring (L, -1))
    err_out ("no 'server' cfg");
  int res = espconn_gethostbyname (
    &sud->conn, lua_tostring (L, -1), &sud->dns, on_dns_found);
  lua_pop (L, 1);
  switch (res)
  {
    case ESPCONN_OK: // already resolved!
    case ESPCONN_INPROGRESS: break;
    default:
     os_free (sud->conn.proto.tcp);
     os_free (sud);
     return luaL_error (L, "DNS lookup error: %d", res);
  }

  if (res == ESPCONN_OK) // synthesize DNS callback
    on_dns_found (0, &sud->dns, &sud->conn);

  return 0;

err:
  if (sud)
    os_free (sud->conn.proto.tcp);
  os_free (sud);
  return luaL_error (L, err_msg);
}


static const LUA_REG_TYPE s4pp_map[] =
{
  { LSTRKEY("upload"), LFUNCVAL(s4pp_do_upload) },
  { LNILKEY, LNILVAL }
};

NODEMCU_MODULE(S4PP, "s4pp", s4pp_map, NULL);
