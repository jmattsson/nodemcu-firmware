/*
 * Copyright 2015 Dius Computing Pty Ltd. All rights reserved.
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
#include <stdint.h>
#include <stdbool.h>

/* Note: the correct flash size *must* be specified when installing the
 * boot loader, or you'll likely get cyclic reboots.
 */

#ifndef BAUD_RATE
# define BAUD_RATE 115200
#endif

#ifndef VERBOSE
# define VERBOSE 1
#endif

#define OTA_HDR_MAGIC 0x4d4a

#define BOOT_STATUS_INVALID   0x10 /* No valid image in this slot */
#define BOOT_STATUS_PREFERRED 0x20 /* If two images available, use this one */
#define BOOT_STATUS_IN_TEST   0x40 /* Boot possible, provided bootbits free */
#define NUM_SECTIONS_MASK     0x0f

#define FLASH_BASE 0x40200000

/* ROM prototypes */
void ets_delay_us (uint32_t us);
int ets_printf (const char *fmt, ...) __attribute__((format(printf, 1, 2)));
void uart_div_modify (int no, unsigned freq);
int SPIRead (uint32_t addr, void *dst, uint32_t len);
int SPIWrite (uint32_t addr, const void *src, uint32_t len);
void Cache_Read_Disable (void);
void Cache_Read_Enable (uint32_t b0, uint32_t b1, uint32_t use_40108000);
/* End ROM prototypes */


typedef struct
{
  uint16_t magic;
  uint8_t  boot_bits;
  uint8_t  flags_num_sections;
  uint32_t entry;
} ota_header;


#if VERBOSE
const char banner[] = "\nNodeMCU Bootloader\n";
const char slot_prefix[] = "  Slot %c: ";
const char booting_slot[] = "Booting slot %c...\n";
const char invalid[] = "invalid\n";
const char in_test[] = "in-test (%u attempts left)\n";
const char preferred[] = "valid, preferred\n";
const char valid[] = "valid\n";
const char nothing_to_boot[] = "NO VALID IMAGE!\n";
const char write_failed[] =
  "ERROR: Failed to update test count, skipping image!\n";


void prepare_uart_output (void)
{
#if BAUD_RATE != 0
  uart_div_modify (0, 52000000/BAUD_RATE);
#endif
  ets_delay_us (80); /* prevent garbling of early prints */
}


void print_image_info (ota_header oh, uint32_t which)
{
  ets_printf (slot_prefix, 'A' + which);

  if (oh.magic != OTA_HDR_MAGIC ||
     (oh.flags_num_sections & BOOT_STATUS_INVALID))
  {
    ets_printf (invalid);
    return;
  }
  if (oh.flags_num_sections & BOOT_STATUS_IN_TEST)
  {
    uint32_t n = 0;
    while (oh.boot_bits)
    {
      ++n;
      oh.boot_bits &= (oh.boot_bits -1);
    }
    ets_printf (in_test, n);
  }
  else if (oh.flags_num_sections & BOOT_STATUS_PREFERRED)
    ets_printf (preferred);
  else
    ets_printf (valid);
}
#else
const uint32_t disable_ets_printf[2] __attribute__((used,unused)) = { 0, 0 };
#endif

uint32_t pick_image (uint32_t a, uint32_t b)
{
  /* Shift flags back into their usual places */
  uint32_t flags[2] = { a << 4, b << 4};

  uint32_t weights[2] = { 1, 1 };
  for (int i = 0; i < 2; ++i)
  {
    weights[i] *= (flags[i] & BOOT_STATUS_INVALID) ? 0 : 1;
    weights[i] *= (flags[i] & BOOT_STATUS_PREFERRED) ? 2 : 1;
    weights[i] *= (flags[i] & BOOT_STATUS_IN_TEST) ? 4 : 1;
  }
  return weights[0] < weights[1];
}

void boot_main (void)
{
  __asm__ (
    "call0 ets_wdt_disable\n"      /* Get WDT into a good state */
    "movi a2, 1\n"
    "call0 ets_wdt_restore\n"      /* Enable the WDT */
#if VERBOSE
    "call0 prepare_uart_output\n"
    "movi a2, banner\n"
    "call0 ets_printf\n"
#endif
    "movi a2, 0x1000\n"
    "call0 load_header\n"          /* Load header from 1st image */
    "extui a12, a2, 28, 3\n"        /* Extract boot flags for 1st image */
#if VERBOSE
    "movi a4, 0\n"
    "call0 print_image_info\n"
#endif
    "movi a2, 0x101000\n"
    "call0 load_header\n"          /* Load header from 2nd image */
    "extui a13, a2, 28, 3\n"        /* Extract boot flags for 2nd image */
#if VERBOSE
    "movi a4, 1\n"
    "call0 print_image_info\n"
#endif
    "5:and a4, a12, a13\n"
    "bbsi a4, 0, 1f\n"             /* Both images have BOOT_STATUS_INVALID? */
    "mov a2, a12\n"
    "mov a3, a13\n"
    "call0 pick_image\n"
    "bgei a2, 1, 2f\n"             /* Did we pick the second image? */
    "movi a2, 0x1000\n"            /* Load 1st image */
    "movi a14, 0\n"
    "mov a3, a12\n"
    "j 3f\n"
    "1: j no_bootable_image\n"     /* Does not return */
    "2: movi a2, 0x101000\n"       /* Load 2nd image */
    "movi a14, 1\n"
    "mov a3, a13\n"
    "3: extui a3, a3, 2, 1\n"      /* Is this an in-test image? */
    "call0 update_header_and_load_sections\n"
    "beqz a2, 4f\n"                /* Fail to clear a test bit? */
    "mov a0, a2\n"                 /* Good to boot, load entry point into a0 */
    "mov a2, a14\n"                /* Pass active image id as arg to entry pt */
    "jx a0\n"                      /* Jump straight to entry point */
    "4:\n"                         /* Failed to load & boot image */
    "movi a5, 0\n"
    "movi a6, %0\n"
    "moveqz a5, a6, a14\n"         /* Which image failed? */
    "or a12, a12, a5\n"            /* ... ether note 1st image invalid */
    "xor a5, a5, a6\n"
    "or a13, a13, a5\n"            /* ... or the 2nd image */
    "j 5b\n"                       /* Try to boot other image, possibly */
    ::"I"(BOOT_STATUS_INVALID)
  );
  __builtin_unreachable ();
}


ota_header load_header (uint32_t addr)
{
  ota_header oh;
  if ((SPIRead (addr, &oh, sizeof (oh)) != 0) ||
      (oh.magic != OTA_HDR_MAGIC) ||
      ((oh.flags_num_sections & BOOT_STATUS_IN_TEST) && !oh.boot_bits))
    oh.flags_num_sections |= BOOT_STATUS_INVALID;
  return oh;
}


uint32_t update_header_and_load_sections (uint32_t hdr_addr, uint32_t in_test)
{
  uint32_t which = (hdr_addr >= 0x100000);
#if VERBOSE
  ets_printf (booting_slot, 'A' + which);
#endif

  if (in_test)
  {
    ota_header oh;
    SPIRead (hdr_addr, &oh, sizeof (oh));
    oh.boot_bits &= (oh.boot_bits -1); /* clear lowest set bit */
    if (SPIWrite (hdr_addr, &oh, sizeof (oh)) != 0)
    {
#if VERBOSE
      ets_printf (write_failed);
#endif
      return 0;
    }
  }

  Cache_Read_Enable (which, 0, 0);

  hdr_addr &= 0xfffff;
  hdr_addr += FLASH_BASE;
  ota_header oh = *(volatile ota_header *)hdr_addr;

  /* Important: No printing from here on - we have probably overwritten
   * the format strings from within here! */
  uint32_t num_sections = oh.flags_num_sections & NUM_SECTIONS_MASK;
  uint32_t *src = (uint32_t *)(hdr_addr + sizeof (ota_header));
  for (unsigned i = 0; i < num_sections; ++i)
  {
    uint32_t *dst = (uint32_t *)(*src++);
    uint32_t sz = *src++;
    for (; sz; sz -= 4)
      *dst++ = *src++;
  }

  Cache_Read_Disable ();
  return oh.entry;
}


void __attribute__((noreturn)) no_bootable_image (void)
{
#if VERBOSE
  ets_printf (nothing_to_boot);
#endif
  while (1) {}
}
