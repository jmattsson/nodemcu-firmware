// Module for interfacing with adc hardware

#include "module.h"
#include "lauxlib.h"

#include "esp_adc/adc_oneshot.h"
#include "hal/adc_types.h"

#include <string.h>

#define ADC_METATABLE "adc.instance"

typedef struct {
  adc_oneshot_unit_handle_t handle;
} adc_ud_t;

typedef enum { STATE_ALIVE, STATE_CLOSED_ACCEPTABLE } userdata_state_t;


// --- Helper functions --------------------------------------

#define CASE_RETURN(x) case x: return x

static adc_unit_t ladc_checkunit(lua_State *L, int idx)
{
  int unit = luaL_checkinteger(L, idx);
  switch(unit)
  {
    CASE_RETURN( ADC_UNIT_1 );
    CASE_RETURN( ADC_UNIT_2 );
    default: return luaL_error(L, "invalid ADC unit: %d", unit);
  }
}

static adc_channel_t ladc_checkchannel(lua_State *L, int idx)
{
  int chan = luaL_checkinteger(L, idx);
  switch(chan)
  {
    CASE_RETURN( ADC_CHANNEL_0 );
    CASE_RETURN( ADC_CHANNEL_1 );
    CASE_RETURN( ADC_CHANNEL_2 );
    CASE_RETURN( ADC_CHANNEL_3 );
    CASE_RETURN( ADC_CHANNEL_4 );
    CASE_RETURN( ADC_CHANNEL_5 );
    CASE_RETURN( ADC_CHANNEL_6 );
    CASE_RETURN( ADC_CHANNEL_7 );
    CASE_RETURN( ADC_CHANNEL_8 );
    CASE_RETURN( ADC_CHANNEL_9 );
    CASE_RETURN( ADC_CHANNEL_10);
    default: return luaL_error(L, "invalid ADC channel: %d", chan);
  }
}

static adc_bitwidth_t ladc_checkoptbits(
  lua_State *L, int idx, adc_bitwidth_t dflt)
{
  if (lua_isnoneornil(L, idx))
    return dflt;
  int bits = luaL_checkinteger(L, idx);
  switch(bits)
  {
    CASE_RETURN( ADC_BITWIDTH_9  );
    CASE_RETURN( ADC_BITWIDTH_10 );
    CASE_RETURN( ADC_BITWIDTH_11 );
    CASE_RETURN( ADC_BITWIDTH_12 );
    CASE_RETURN( ADC_BITWIDTH_13 );
    default: return luaL_error(L, "invalid ADC bits: %d", bits);
  }
}

static adc_atten_t ladc_checkoptatten(lua_State *L, int idx, adc_atten_t dflt)
{
  if (lua_isnoneornil(L, idx))
    return dflt;
  int atten = luaL_checkinteger(L, idx);
  switch(atten)
  {
    CASE_RETURN( ADC_ATTEN_DB_0 );
    CASE_RETURN( ADC_ATTEN_DB_2_5 );
    CASE_RETURN( ADC_ATTEN_DB_6 );
    CASE_RETURN( ADC_ATTEN_DB_12 );
    default: return luaL_error(L, "invalid ADC attenuation: %d", atten);
  }
}

static void ladc_checkerr(lua_State *L, esp_err_t err)
{
  switch(err)
  {
    case ESP_OK: return;
    case ESP_ERR_NOT_FOUND: luaL_error(L, "ADC already in use"); return;
    default: luaL_error(L, "ADC initialisation error: %d", err);
  }
}

static adc_ud_t *ladc_checkuserdata(lua_State *L, int idx, userdata_state_t s)
{
  adc_ud_t *ud = (adc_ud_t *)luaL_checkudata(L, idx, ADC_METATABLE);
  if (s == STATE_ALIVE && ud->handle == NULL)
    luaL_error(L, "ADC instance closed");
  return ud;
}


// --- Lua API implementation ---------------------------------

// Lua: adc.setup(unit)
static int ladc_setup(lua_State *L)
{
  // Sanity check arguments
  adc_unit_t unit = ladc_checkunit(L, 1);

  // Create the user data object
  adc_ud_t *ud = (adc_ud_t *)lua_newuserdata(L, sizeof(adc_ud_t));
  luaL_getmetatable(L, ADC_METATABLE);
  lua_setmetatable(L, -2);
  memset(ud, 0, sizeof(*ud)); // ensure error path doesn't see random data

  // Create ADC unit handle
  adc_oneshot_unit_init_cfg_t unit_cfg = {
    .unit_id = unit,
    .clk_src = 0, // use default
    .ulp_mode = ADC_ULP_MODE_DISABLE,
  };
  ladc_checkerr(L, adc_oneshot_new_unit(&unit_cfg, &ud->handle));

  // Returning userdata object
  return 1;
}


// Lua: adcObj:close() -- or garbage collection
static int ladc_close(lua_State *L)
{
  adc_ud_t *ud = ladc_checkuserdata(L, 1, STATE_CLOSED_ACCEPTABLE);
  // On GC, may already be closed, so have to check here
  if (ud->handle)
  {
    adc_oneshot_del_unit(ud->handle);
    ud->handle = NULL;
  }

  return 0;
}

// Lua: adcObj:configure(channel, bits, atten)
static int ladc_configure(lua_State *L)
{
  adc_ud_t *ud = ladc_checkuserdata(L, 1, STATE_ALIVE);
  adc_channel_t channel = ladc_checkchannel(L, 2);
  adc_bitwidth_t bits = ladc_checkoptbits(L, 3, ADC_BITWIDTH_DEFAULT);
  adc_atten_t atten = ladc_checkoptatten(L, 4, ADC_ATTEN_DB_0);

  adc_oneshot_chan_cfg_t cfg = {
    .atten = atten,
    .bitwidth = bits,
  };
  ladc_checkerr(L, adc_oneshot_config_channel(ud->handle, channel, &cfg));

  return 0;
}

// Lua: adcObj:read(channel)
static int ladc_read(lua_State *L)
{
  adc_ud_t *ud = ladc_checkuserdata(L, 1, STATE_ALIVE);
  adc_channel_t channel = ladc_checkchannel(L, 2);

  int raw = 0;
  ladc_checkerr(L, adc_oneshot_read(ud->handle, channel, &raw));

  lua_pushinteger(L, raw);
  return 1;
}


// Module function map
LROT_BEGIN(adc_dyn, NULL, LROT_MASK_GC_INDEX)
  LROT_FUNCENTRY( __gc,             ladc_close )
  LROT_TABENTRY ( __index,          adc_dyn )
  LROT_FUNCENTRY( close,            ladc_close)
  LROT_FUNCENTRY( configure,        ladc_configure )
  LROT_FUNCENTRY( read,             ladc_read)
LROT_END(adc_dyn, NULL, LROT_MASK_GC_INDEX)

LROT_BEGIN(adc, NULL, 0)
  LROT_FUNCENTRY( setup,            ladc_setup )
  LROT_NUMENTRY ( ADC1,             ADC_UNIT_1 )
  LROT_NUMENTRY ( ADC2,             ADC_UNIT_2 )
  LROT_NUMENTRY ( ATTEN_0,          ADC_ATTEN_DB_0 )
  LROT_NUMENTRY ( ATTEN_2_5,        ADC_ATTEN_DB_2_5 )
  LROT_NUMENTRY ( ATTEN_6,          ADC_ATTEN_DB_6 )
  LROT_NUMENTRY ( ATTEN_12,         ADC_ATTEN_DB_12 )
  LROT_NUMENTRY ( BITS_9,           ADC_BITWIDTH_9 )
  LROT_NUMENTRY ( BITS_10,          ADC_BITWIDTH_10 )
  LROT_NUMENTRY ( BITS_11,          ADC_BITWIDTH_11 )
  LROT_NUMENTRY ( BITS_12,          ADC_BITWIDTH_12 )
  LROT_NUMENTRY ( BITS_13,          ADC_BITWIDTH_13 )
  LROT_NUMENTRY ( CHANNEL_0,        ADC_CHANNEL_0 )
  LROT_NUMENTRY ( CHANNEL_1,        ADC_CHANNEL_1 )
  LROT_NUMENTRY ( CHANNEL_2,        ADC_CHANNEL_2 )
  LROT_NUMENTRY ( CHANNEL_3,        ADC_CHANNEL_3 )
  LROT_NUMENTRY ( CHANNEL_4,        ADC_CHANNEL_4 )
  LROT_NUMENTRY ( CHANNEL_5,        ADC_CHANNEL_5 )
  LROT_NUMENTRY ( CHANNEL_6,        ADC_CHANNEL_6 )
  LROT_NUMENTRY ( CHANNEL_7,        ADC_CHANNEL_7 )
  LROT_NUMENTRY ( CHANNEL_8,        ADC_CHANNEL_8 )
  LROT_NUMENTRY ( CHANNEL_9,        ADC_CHANNEL_9 )
  LROT_NUMENTRY ( CHANNEL_10,       ADC_CHANNEL_10 )
LROT_END(adc, NULL, 0)

static int ladc_init(lua_State *L)
{
  luaL_rometatable(L, ADC_METATABLE, LROT_TABLEREF(adc_dyn));
  return 0;
}

NODEMCU_MODULE(ADC, "adc", adc, ladc_init);
