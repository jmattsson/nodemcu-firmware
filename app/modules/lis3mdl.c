// Module for talking to the LIS3MDL in DiUS's gen2 sensors using HSPI
// (Hard-coded to use the HSPI interface)
//
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

#include "lauxlib.h"
#include "driver/spi_register.h"
#include "driver/lis3mdl.h"
#include "eagle_soc.h"
#include "module.h"

// ******* C API functions *************
#define READ_ACCESS  1
#define WRITE_ACCESS 0
#define AUTO_INCREMENT 0x80

#define HSPI 1

static void init_hspi(void)
{
    // Configure GPIO12-15 as SPI bus
    WRITE_PERI_REG(PERIPHS_IO_MUX, 0x005);

    PIN_PULLUP_DIS(PERIPHS_IO_MUX_MTDI_U);     //Disable the stupid pullup on GPIO12
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTDI_U, 2); //GPIO12 is HSPI MISO pin (Master Data In)
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTCK_U, 2); //GPIO13 is HSPI MOSI pin (Master Data Out)
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTMS_U, 2); //GPIO14 is HSPI CLK pin (Clock)
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTDO_U, 2); //GPIO15 is HSPI CS pin (Chip Select / Slave Select)

    // Set up clock
    uint8_t prediv=10; // Base clock 8MHz  (80MHz/10)
    uint8_t cntdiv=32;  // SPI clock 0.5MHz   (8MHz/16)
    WRITE_PERI_REG(SPI_CLOCK(HSPI),
                   (((prediv-1)&SPI_CLKDIV_PRE)<<SPI_CLKDIV_PRE_S)|
                   (((cntdiv-1)&SPI_CLKCNT_N)<<SPI_CLKCNT_N_S)|
                   (((cntdiv>>1)&SPI_CLKCNT_H)<<SPI_CLKCNT_H_S)|
                   ((0&SPI_CLKCNT_L)<<SPI_CLKCNT_L_S));

    // Byte order --- little endian byte order, big-endian bit order in a byte
    CLEAR_PERI_REG_MASK(SPI_USER(HSPI), SPI_WR_BYTE_ORDER);
    CLEAR_PERI_REG_MASK(SPI_USER(HSPI), SPI_RD_BYTE_ORDER);

    // Basic SPI setup
    SET_PERI_REG_MASK(SPI_USER(HSPI), SPI_CS_SETUP|SPI_CS_HOLD);
    CLEAR_PERI_REG_MASK(SPI_USER(HSPI), SPI_FLASH_MODE);
}

#define CMD_READ 0x80
#define CMD_WRITE 0x00
#define CMD_AUTOINC 0x40

static void  do_spi(void)
{
    SET_PERI_REG_MASK(SPI_CMD(HSPI), SPI_USR);
    while (READ_PERI_REG(SPI_CMD(HSPI))&SPI_USR)
    { // busy-loop
    }
}


static void l3_read_regs(uint8_t addr, uint8_t* data, uint8_t len)
{
    uint8_t cmd=CMD_READ | CMD_AUTOINC | addr;

    // We use the "command" phase for sending out the command, and the "miso" phase for reading the response.
    // No "addr", "mosi" or "dummy" phase
    SET_PERI_REG_MASK(SPI_USER(HSPI), SPI_USR_COMMAND|SPI_USR_MISO);
    CLEAR_PERI_REG_MASK(SPI_USER(HSPI), SPI_USR_ADDR|SPI_USR_DUMMY|SPI_USR_MOSI|SPI_USR_MISO_HIGHPART);

    WRITE_PERI_REG(SPI_USER1(HSPI),((8*len-1)&SPI_USR_MISO_BITLEN)<<SPI_USR_MISO_BITLEN_S);

    WRITE_PERI_REG(SPI_USER2(HSPI),
                   (((7&SPI_USR_COMMAND_BITLEN)<<SPI_USR_COMMAND_BITLEN_S) |
                    cmd)); // 8 bit command, with value "cmd"

    // DO IT!
    do_spi();
    uint32_t source=SPI_W0(HSPI)-4;
    uint32_t shift=32;
    uint32_t word=0;

    while (len--)
    {
        if (shift>=32)
        {
            source+=4;
            shift=0;
            word=READ_PERI_REG(source);
        }
        *(data++)=(word>>shift)&0xff;
        shift+=8;
    }
}

static void l3_write_regs(uint8_t addr, uint8_t* data, uint8_t len)
{
    uint8_t cmd=CMD_WRITE | CMD_AUTOINC | addr;

    // We use the "command" phase for sending out the command, and the "mosi" phase for sending the data
    // No "addr", "miso" or "dummy" phase
    SET_PERI_REG_MASK(SPI_USER(HSPI), SPI_USR_COMMAND|SPI_USR_MOSI);
    CLEAR_PERI_REG_MASK(SPI_USER(HSPI), SPI_USR_ADDR|SPI_USR_DUMMY|SPI_USR_MISO|SPI_USR_MISO_HIGHPART);

    WRITE_PERI_REG(SPI_USER1(HSPI),((8*len-1)&SPI_USR_MOSI_BITLEN)<<SPI_USR_MOSI_BITLEN_S);

    WRITE_PERI_REG(SPI_USER2(HSPI),
                   (((7&SPI_USR_COMMAND_BITLEN)<<SPI_USR_COMMAND_BITLEN_S) |
                    cmd)); // 8 bit command, with value "cmd"

    uint32_t dest=SPI_W0(HSPI);
    uint32_t shift=0;
    uint32_t word=0;

    while (len--)
    {
        if (shift>=32)
        {
            dest+=4;
            shift=0;
            word=0;
        }
        word|=((uint32_t)*(data++))<<shift;
        WRITE_PERI_REG(dest,word);
        shift+=8;
    }
    // DO IT!
    do_spi();
}

static uint8_t l3_read_reg(uint8_t addr)
{
    uint8_t tmp;
    l3_read_regs(addr,&tmp,1);
    return tmp;
}

static void l3_write_reg(uint8_t addr, uint8_t val)
{
    l3_write_regs(addr,&val,1);
}

static uint8_t l3_init()
{
  init_hspi();

  int16_t whoami=l3_read_reg(ADDR_WHO_AM_I);
  if (whoami!=WHO_I_AM)
  {
      ets_printf("Got %02x, expected %02x\n",whoami,WHO_I_AM);
      return 0;
  }
  //ets_printf("Pre-reset: %02x/%02x\n",l3_read_reg(0x30),l3_read_reg(0x31));

  l3_write_reg(ADDR_CTRL_REG2,0x0c);

  // ets_printf("Post-reset: %02x/%02x, reg2 is %02x\n",l3_read_reg(0x30),l3_read_reg(0x31),l3_read_reg(ADDR_CTRL_REG2));

  ets_delay_us(100);
  l3_write_reg(ADDR_CTRL_REG1,REG1_FAST_ODR_SELECT | REG1_FAST_ODR_1000 | REG1_TEMP_ENABLE);
  l3_write_reg(ADDR_CTRL_REG2,REG2_FULL_SCALE_4GA);
  l3_write_reg(ADDR_CTRL_REG3,REG3_OFF1_MODE);
  l3_write_reg(ADDR_CTRL_REG4,REG4_OMZ_LP);
  l3_write_reg(ADDR_CTRL_REG5,0);
  l3_write_reg(0x30,0x01);
  // ets_printf("Readback: %02x/%02x\n",l3_read_reg(0x30),l3_read_reg(0x31));
  return 1;
}

static void l3_read_xyz(uint16_t* r)
{
  uint8_t tout[6];

  l3_read_regs(ADDR_OUT_X_L,tout,6);
  r[0]=(((uint16_t)tout[1])<<8)+tout[0];
  r[1]=(((uint16_t)tout[3])<<8)+tout[2];
  r[2]=(((uint16_t)tout[5])<<8)+tout[4];
}

static void l3_start_magnetic_conversion(void)
{
  l3_write_reg(ADDR_CTRL_REG3,REG3_SINGLE_MODE);
}

static uint16_t l3_read_temp(void)
{
  uint8_t tout[2];

  l3_read_regs(ADDR_OUT_TEMP_L,tout,2);
  return (((uint16_t)tout[1])<<8)+tout[0];
}

#define KHZ 80000
#define CCOUNT_PER_SAMPLE  (KHZ*320/265+1)

// ******* Lua API functions *************
static int l3_take_samples(lua_State *L, bool minmax)
{
  uint32_t count = luaL_checknumber (L, 1);
  uint32_t table_size=minmax?2:count;
  if (!l3_init())
    return 0;

  lua_createtable (L, table_size, 0);
  lua_createtable (L, table_size, 0);
  lua_createtable (L, table_size, 0);

  int ind=0;
  int16_t min[3];
  int16_t max[3];
  min[0]=min[1]=min[2]=32767;
  max[0]=max[1]=max[2]=-32768;

  uint32_t before=xthal_get_ccount();
  l3_write_reg(ADDR_CTRL_REG3,REG3_OFF1_MODE);
  // Start first sample
  l3_write_reg(ADDR_CTRL_REG3,REG3_SINGLE_MODE);
  uint32_t next=before;

  while (ind++<count)
  {
    next+=CCOUNT_PER_SAMPLE;
    while (((int32_t)(next-xthal_get_ccount()))>0) {}
    // Start next sample
    l3_write_reg(ADDR_CTRL_REG3,REG3_SINGLE_MODE);
    // read previous sample
    // Note: We do *not* check the status flags; Because all interactions with the magnetometer
    // are strictly timed, we *know* that there will be exactly one sample available.
    int16_t v[3];

    l3_read_xyz(v);

    int i;
    for (i=0;i<3;i++)
    {
      if (!minmax)
      {
        lua_pushinteger(L, ind);
        lua_pushinteger(L, v[i]);
        lua_settable(L,i-5);
      }
      if (v[i]<min[i])
        min[i]=v[i];
      if (v[i]>max[i])
        max[i]=v[i];
    }
  }
  int millicelsius=25000+((int16_t)l3_read_temp())*125;

  if (minmax)
  {
    int i;
    for (i=0;i<3;i++)
    {
      lua_pushinteger(L,1);
      lua_pushinteger(L,min[i]);
      lua_settable(L,i-5);
      lua_pushinteger(L,2);
      lua_pushinteger(L,max[i]);
      lua_settable(L,i-5);
    }
  }
  lua_pushinteger(L, millicelsius);
  return 4;
}

static int l3_take_samples_simple(lua_State *L)
{
  return l3_take_samples(L,false);
}

static int l3_take_samples_minmax(lua_State *L)
{
  return l3_take_samples(L,true);
}

static const LUA_REG_TYPE lis3mdl_map[] =
{
  { LSTRKEY("read"), LFUNCVAL(l3_take_samples_simple) },
  { LSTRKEY("readminmax"), LFUNCVAL(l3_take_samples_minmax) },
  { LNILKEY, LNILVAL }
};

NODEMCU_MODULE(LIS3MDL,"lis3mdl",lis3mdl_map,NULL);
