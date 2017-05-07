/*
 * Copyright (c) 2017 Johny Mattsson
 * All rights reserved.
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
 */

#include "module.h"
#include "lauxlib.h"
#include "lmem.h"

#include "esp_vfs_fat.h"
#include "wear_levelling.h"
#include "ff.h"
#include "diskio.h"
#include "diskio_spiflash.h"

#include <string.h>

// ---- Typedefs & constants ----------------------------------------------

typedef struct mounted_fs
{
  int meta_key; // registry key for [ fstype, partid, mountpt ]
  struct mounted_fs *next;
} mounted_fs_t;

typedef mounted_fs_t * (*mount_fn)(lua_State *L, const char *partid, const char *mountpt);


// The struct-in-struct construct is used so that luaM_new() can be used
// safely, and avoids complicated memory handling in the face of luaL_error()s
// being raised. Each mount is thus reduced to a single memory allocation.
typedef struct {
  mounted_fs_t common;
  wl_handle_t wl;
} mounted_fatfs_wl_t;


typedef void (*format_fn)(lua_State *L, const char *partid);

// Important: `mt` is sunk into the unmount function, it MUST be free'd there!
typedef void (*unmount_fn)(lua_State *L, mounted_fs_t *mt, const char *mountpt);

// ---- Forward declarations ----------------------------------------------

static mounted_fs_t *do_mount_spiffs (lua_State *L, const char *partid, const char *mountpt);
static mounted_fs_t *do_mount_fatfs (lua_State *L, const char *partid, const char *mountpt);
static mounted_fs_t *do_mount_fatfs_wl (lua_State *L, const char *partid, const char *mountpt);

static void do_format_spiffs (lua_State *L, const char *partid);
static void do_format_fatfs (lua_State *L, const char *partid);
static void do_format_fatfs_wl (lua_State *L, const char *partid);

static void do_unmount_spiffs (lua_State *L, mounted_fs_t *mt, const char *mountpt);
static void do_unmount_fatfs (lua_State *L, mounted_fs_t *mt, const char *mountpt);
static void do_unmount_fatfs_wl (lua_State *L, mounted_fs_t *mt, const char *mountpt);


// ---- Constants ---------------------------------------------------------

static const char *known_fstypes[] = {
  "spiffs",
  "fatfs",     // TODO: conditionally include based on #CONFIG_... ?
  "fatfs+wl",
  0
};

// Important: order MUST match known_fstypes above!
static const mount_fn mount_functions[] =
{
  do_mount_spiffs,
  do_mount_fatfs,
  do_mount_fatfs_wl,
};

static const format_fn format_functions[] =
{
  do_format_spiffs,
  do_format_fatfs,
  do_format_fatfs_wl,
};

static const unmount_fn unmount_functions[] =
{
  do_unmount_spiffs,
  do_unmount_fatfs,
  do_unmount_fatfs_wl,
};

// ---- Local variables ---------------------------------------------------


// Tracking of current mounts, so they may be unmounted (and listed)
static mounted_fs_t *mounts;


// ---- Local functions ---------------------------------------------------

static void do_format_spiffs (lua_State *L, const char *partid)
{
  // TODO
  luaL_error (L, "not yet implemented");
}


static void do_format_fatfs (lua_State *L, const char *partid)
{
  // TODO
  luaL_error (L, "not yet implemented");
}


static void do_format_fatfs_wl (lua_State *L, const char *partid)
{
  esp_partition_t *part =
    (esp_partition_t *)esp_partition_find_first (
        ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_FAT, partid);
  if (!part)
    luaL_error (L, "partition '%s' not found", partid);

  wl_handle_t wl;
  esp_err_t err = wl_mount (part, &wl);
  if (err != ESP_OK)
    luaL_error (L, "failed to attach wear leveling layer");

  BYTE pdrv = 0xFF;
  if ((ff_diskio_get_drive (&pdrv) != ESP_OK) || (pdrv == 0xFF))
    luaL_error (L, "too many FAT volumes in use");
  char drv[3] = { (char)('0' + pdrv), ':', 0 };

  const size_t workbuf_sz = 4096; // magic number for now :(
  void *workbuf = luaM_malloc (L, workbuf_sz);

  err = ff_diskio_register_wl_partition (pdrv, wl);
  if (err != ESP_OK)
  {
    ff_diskio_unregister (pdrv);
    luaL_error (L, "too many wear leveling volumes in use");
  }

  FRESULT fr = f_mkfs (drv, FM_ANY | FM_SFD, workbuf_sz, workbuf, workbuf_sz);
  ff_diskio_unregister (pdrv);
  wl_unmount (wl);
  if (fr != FR_OK)
    luaL_error (L, "FAT format failed: %d", fr);
}


static mounted_fs_t *do_mount_spiffs (lua_State *L, const char *partid, const char *mountpt)
{
  // TODO
  luaL_error (L, "not yet implemented");
  return 0;
}


static mounted_fs_t *do_mount_fatfs (lua_State *L, const char *partid, const char *mountpt)
{
  // TODO
  luaL_error (L, "not yet implemented");
  return 0;
}


static mounted_fs_t *do_mount_fatfs_wl (lua_State *L, const char *partid, const char *mountpt)
{
  mounted_fatfs_wl_t *mt = luaM_new (L, mounted_fatfs_wl_t);
  esp_vfs_fat_mount_config_t mount_config;
  mount_config.format_if_mount_failed = false;
  mount_config.max_files = 4; // TODO: make this tuneable in Kconfig

  esp_err_t err = esp_vfs_fat_spiflash_mount (
      mountpt, partid, &mount_config, &mt->wl);
  switch (err)
  {
    case ESP_OK: return (mounted_fs_t *)mt;
    case ESP_ERR_NOT_FOUND: luaL_error (L, "partition not found"); break;
    case ESP_ERR_INVALID_STATE: luaL_error (L, "already mounted"); break;
    case ESP_ERR_NO_MEM: luaL_error (L, "out of memory"); break;
    default: luaL_error (L, "driver reported failure"); break;
  }
  __builtin_unreachable();
}


static void do_unmount_spiffs (lua_State *L, mounted_fs_t *mt, const char *mountpt)
{
  // TODO
  luaL_error (L, "not yet implemented");
}

static void do_unmount_fatfs (lua_State *L, mounted_fs_t *mt, const char *mountpt)
{
  // TODO
  luaL_error (L, "not yet implemented");
}

static void do_unmount_fatfs_wl (lua_State *L, mounted_fs_t *mt, const char *mountpt)
{
  mounted_fatfs_wl_t *mtwl = (mounted_fatfs_wl_t *)mt;

  esp_vfs_fat_spiflash_unmount (mountpt, mtwl->wl);
  luaM_free (L, mtwl);
}


// Match on one of more of fstype, partid, mountpt
static mounted_fs_t *find_mounted (lua_State *L, const char *fstype, const char *partid, const char *mountpt)
{
  for (mounted_fs_t *mt = mounts; mt; mt = mt->next)
  {
    lua_rawgeti (L, LUA_REGISTRYINDEX, mt->meta_key);
    lua_rawgeti (L, -1, 1);
    const char *mt_fstype = lua_tostring (L, -1);
    lua_rawgeti (L, -2, 2);
    const char *mt_partid = lua_tostring (L, -1);
    lua_rawgeti (L, -3, 3);
    const char *mt_mountpt = lua_tostring (L, -1);
    bool found = 
      (((fstype == 0) || (strcmp (mt_fstype, fstype) == 0)) &&
       ((partid == 0) || (strcmp (mt_partid, partid) == 0)) &&
       ((mountpt == 0) || (strcmp (mt_mountpt, mountpt) == 0)));
    lua_pop (L, 4);
    if (found)
      return mt;
  }
  return NULL;
}


// ---- Lua interface -----------------------------------------------------

// Lua: fs.format(fstype, partid)
static int lfs_format (lua_State *L)
{
  int fstype = luaL_checkoption (L, 1, NULL, known_fstypes);
  const char *partid = luaL_checkstring (L, 2);

  if (find_mounted (L, lua_tostring (L, 1), partid, NULL))
    return luaL_error (L, "partition currently mounted, unable to format");

  format_functions[fstype] (L, partid);
  return 0;
}


// Lua: fs.mount(fstype, partid, mountpt) // TODO: explict ro/rw/other opts?
static int lfs_mount (lua_State *L)
{
  if (lua_gettop (L) == 0) // only list
  {
    int n = 1;
    lua_createtable (L, 0, 0);
    for (mounted_fs_t *mt = mounts; mt; mt = mt->next)
    {
      lua_checkstack (L, 1);
      lua_rawgeti (L, LUA_REGISTRYINDEX, mt->meta_key);
      lua_rawseti (L, -2, n++);
    }
    return 1;
  }
  else {
    int fstype = luaL_checkoption (L, 1, NULL, known_fstypes);
    const char *partid = luaL_checkstring (L, 2);
    const char *mountpt = luaL_checkstring (L, 3);
    if (*mountpt != '/')
      return luaL_error (L, "mount point must start with /");

    // create [ fstype, partid, mountpt ]
    lua_checkstack (L, 4);
    lua_createtable (L, 3, 0);
    for (int i = 1; i < 4; ++i)
    {
      lua_pushvalue (L, i);
      lua_rawseti (L, -2, i);
    }

    mounted_fs_t *mt = mount_functions[fstype] (L, partid, mountpt);
    mt->meta_key = luaL_ref (L, LUA_REGISTRYINDEX);
    mt->next = mounts;
    mounts = mt;

    return 0;
  }
}


// Lua: fs.unmount(mountpt)
static int lfs_unmount (lua_State *L)
{
  const char *mountpt = luaL_checkstring (L, 1);

  mounted_fs_t *mt = find_mounted (L, NULL, NULL, mountpt);
  if (!mt)
    luaL_error (L, "'%s' is not mounted", mountpt);

  lua_rawgeti (L, LUA_REGISTRYINDEX, mt->meta_key);
  lua_rawgeti (L, -1, 1);
  int fstype = luaL_checkoption (L, -1, NULL, known_fstypes);

  for (mounted_fs_t **pmt = &mounts; *pmt; pmt = &((*pmt)->next))
  {
    if (*pmt == mt)
    {
      *pmt = mt->next;
      break;
    }
  }
  luaL_unref (L, LUA_REGISTRYINDEX, mt->meta_key);
  unmount_functions[fstype] (L, mt, mountpt);
  return 0;
}


// Module function map
static const LUA_REG_TYPE fs_map[] =
{
  { LSTRKEY("format"),     LFUNCVAL(lfs_format)  },
  { LSTRKEY("mount"),      LFUNCVAL(lfs_mount)   },
  { LSTRKEY("unmount"),    LFUNCVAL(lfs_unmount) },
#if 0
  { LSTRKEY("chdir"),      LFUNCVAL(lfs_chdir)   },
  { LSTRKEY("list"),       LFUNCVAL(lfs_list)    },
  { LSTRKEY("delete"),     LFUNCVAL(lfs_delete)  },
  { LSTRKEY("info"),       LFUNVCAL(lfs_info)    },
#endif
  { LNILKEY, LNILVAL }
};


static int luaopen_fs (lua_State *L)
{
  // TODO: register /internal mount here!

  printf("Available file system types:");
  for (const char **ft = known_fstypes; *ft; ++ft)
    printf(" %s", *ft);
  printf("\n");

  return 0;
}

NODEMCU_MODULE(FS, "fs", fs_map, luaopen_fs);
