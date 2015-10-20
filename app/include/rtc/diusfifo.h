#ifndef DIUS_FIFO_H
#define DIUS_FIFO_H

//
// This is a generic interface to the rtc_fifo and the flash_fifo. When both are selected,
// they will be chained, with data first going into the rtc fifo, and then being dumped
// into flash fifo when the rtc fifo overflows.
//
#ifndef DIUS_FIFO_USE_RTC_MEM
#define DIUS_FIFO_USE_RTC_MEM 1
#endif

#ifndef DIUS_FIFO_USE_RTC_SAMPLER
#define DIUS_FIFO_USE_RTC_SAMPLER 1
#endif

#ifndef DIUS_FIFO_USE_FLASH_MEM
#define DIUS_FIFO_USE_FLASH_MEM 1
#endif

#if DIUS_FIFO_USE_FLASH_MEM
#ifndef ESP8266_FLASH_FIFO_START
#define ESP8266_FLASH_FIFO_START 0xa0000
#endif
#ifndef ESP8266_FLASH_FIFO_SIZE
#define ESP8266_FLASH_FIFO_SIZE  0x40000
#endif
#endif

#include "fifo.h"
#include <stdbool.h>

#define INTERNAL // Just for keeping track
#define API      // ditto

#include "rtcfifo.h"
#include "flashfifo.h"

INTERNAL static inline bool use_rtc_fifo()
{
  return DIUS_FIFO_USE_RTC_MEM;
}

INTERNAL static inline bool use_flash_fifo()
{
  return DIUS_FIFO_USE_FLASH_MEM;
}





/////////////////////////////////////// API STARTS HERE ///////////////////////////////////
API static inline uint32_t dius_fifo_get_count(void)
{
  uint32_t count=0;

  if (use_rtc_fifo())
    count+=rtc_fifo_get_count();
  if (use_flash_fifo())
    count+=flash_fifo_get_count();
  return count;
}

API static inline uint32_t dius_fifo_get_maxval(void)
{
  uint32_t v=0xffffffff;
  if (use_rtc_fifo())
  {
    uint32_t m=rtc_fifo_get_maxval();
    if (m<v)
      v=m;
  }
  if (use_flash_fifo())
  {
    uint32_t m=flash_fifo_get_maxval();
    if (m<v)
      v=m;
  }
  return v;
}

// Minimum amount of samples which can successfully be held in this FIFO. Any more,
// and older data *may* be evicted
API static inline uint32_t dius_fifo_get_size(void)
{
  uint32_t size=0;

  if (use_rtc_fifo())
    size+=rtc_fifo_get_size();
  if (use_flash_fifo())
    size+=flash_fifo_get_size();

  return size;
}

// Maximum amount of samples which can possibly be held in this FIFO. Any more, and
// eviction of older data is guaranteed. Eviction might happen earlier already.
API static inline uint32_t dius_fifo_get_max_size(void)
{
  uint32_t size=0;

  if (use_rtc_fifo())
    size+=rtc_fifo_get_max_size();
  if (use_flash_fifo())
    size+=flash_fifo_get_max_size();

  return size;
}

API static inline void dius_fifo_put_loc(uint32_t first, uint32_t last, uint32_t tagcount)
{
  rtc_fifo_put_loc(first,last,tagcount);
}

// returns true if sample popped, false if not
API static inline bool dius_fifo_pop_sample(sample_t* dst)
{
  bool popped=false;
  if (use_flash_fifo())
    popped=flash_fifo_pop_sample(dst);
  if (!popped && use_rtc_fifo())
    popped=rtc_fifo_pop_sample(dst);

  return popped;
}

// returns true if sample is available, false if not
API static inline bool dius_fifo_peek_sample(sample_t* dst, uint32_t from_top)
{
  if (use_flash_fifo())
  {
    uint32_t count=flash_fifo_get_count();
    if (count>from_top)
      return flash_fifo_peek_sample(dst,from_top);
    from_top-=count;
  }
  if (use_rtc_fifo())
    return rtc_fifo_peek_sample(dst,from_top);
  return false;
}

API static inline bool dius_fifo_drop_samples(uint32_t from_top)
{
  if (use_flash_fifo())
  {
    uint32_t count=flash_fifo_get_count();
    if (count>=from_top)
      return flash_fifo_drop_samples(from_top);
    // Drop all of the flash ones, and whatever is left from the RTC
    if (!flash_fifo_drop_samples(count))
      return false;
    from_top-=count;
  }
  if (use_rtc_fifo())
    return rtc_fifo_drop_samples(from_top);
  return false;
}

API static inline bool dius_fifo_store_sample(const sample_t* s)
{
  // Check whether we use both, and if so, whether the RTC fifo is full....
  while (use_rtc_fifo() && use_flash_fifo() &&
         rtc_fifo_store_will_shuffle(s))
  { // Need to shuffle one sample into the flash
    sample_t tmp;
    if (!rtc_fifo_pop_sample(&tmp))
      return false;
    if (!flash_fifo_store_sample(&tmp))
      return false;
  }

  if (use_rtc_fifo())
    return rtc_fifo_store_sample(s);
  else if (use_flash_fifo())
    return flash_fifo_store_sample(s);
  else
    return false;
}

API static inline bool dius_fifo_check_magic(void)
{
  bool ok=true;
  if (use_rtc_fifo() && !rtc_fifo_check_magic())
    ok=false;
  if (use_flash_fifo() && !flash_fifo_check_magic())
    ok=false;

  return ok;
}

API static inline bool dius_fifo_prepare(uint32_t tagcount)
{
  if (use_rtc_fifo())
  {
    if (!rtc_fifo_prepare(tagcount))
      return false;
  }
  if (use_flash_fifo())
  {
    if (!flash_fifo_prepare(tagcount))
      return false;
  }
  return true;
}
#endif
