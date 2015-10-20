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
 * @author Bernd Meyer <bmeyer@dius.com.au>
 */
#ifndef _RTCFIFO_H_
#define _RTCFIFO_H_

#define INTERNAL // Just for keeping track
#define API      // ditto

#include <stdbool.h>

// 1: FIFO location. First FIFO address in bits 0:7, first non-FIFO address in bits 8:15.
//                   Number of tag spaces in bits 16:23
// 2: Number of samples in FIFO.
// 3: FIFO tail (where next sample will be written. Increments by 1 for each sample)
// 4: FIFO head (where next sample will be read. Increments by 1 for each sample)
// 5: FIFO head timestamp. Used and maintained when pulling things off the FIFO. This is the timestamp of the
//    most recent sample pulled off; I.e. the head samples timestamp is this plus that sample's delta_t
// 6: FIFO tail timestamp. Used and maintained when adding things to the FIFO. This is the timestamp of the
//    most recent sample to have been added. I.e. a new sample's delta-t is calculated relative to this
// (5/6) are meaningless when (3) is zero
//

#define RTC_FIFO_BASE          15
#define RTC_FIFO_MAGIC         0x44695553

// RTCFIFO storage
#define RTC_FIFO_MAGIC_POS     (RTC_FIFO_BASE+0)
#define RTC_FIFOLOC_POS        (RTC_FIFO_BASE+1)
#define RTC_FIFOCOUNT_POS      (RTC_FIFO_BASE+2)
#define RTC_FIFOTAIL_POS       (RTC_FIFO_BASE+3)
#define RTC_FIFOHEAD_POS       (RTC_FIFO_BASE+4)
#define RTC_FIFOTAIL_T_POS     (RTC_FIFO_BASE+5)
#define RTC_FIFOHEAD_T_POS     (RTC_FIFO_BASE+6)

// 32-127: FIFO space. Consisting of a number of tag spaces (see 4), followed by data entries.
//     Data entries consist of:
//     Bits 28:31  -> tag index. 0-15
//     Bits 25:27  -> decimals
//     Bits 16:24  -> delta-t in seconds from previous entry
//     Bits 0:15   -> sample value

#define RTC_DEFAULT_FIFO_START 32
#define RTC_DEFAULT_FIFO_END  128
#define RTC_DEFAULT_TAGCOUNT    5
#define RTC_DEFAULT_FIFO_LOC (RTC_DEFAULT_FIFO_START + (RTC_DEFAULT_FIFO_END<<8) + (RTC_DEFAULT_TAGCOUNT<<16))

#include "fifo.h"

#ifdef UNIT_TEST
#include <stdlib.h>
static uint32_t rtc_mem[128];
static uint32_t rtc_mem_read(uint32_t pos)
{
  if (pos>=128)
    abort();
  return rtc_mem[pos];
}

static void rtc_mem_write(uint32_t pos, uint32_t val)
{
  if (pos>=128)
    abort();
  rtc_mem[pos]=val;
}
#else
#include "rtcaccess.h"
#endif

INTERNAL static inline void rtc_fifo_clear_content(void);

INTERNAL static inline uint32_t rtc_fifo_get_tail(void)
{
  return rtc_mem_read(RTC_FIFOTAIL_POS);
}

INTERNAL static inline void rtc_fifo_put_tail(uint32_t val)
{
  rtc_mem_write(RTC_FIFOTAIL_POS,val);
}

INTERNAL static inline uint32_t rtc_fifo_get_head(void)
{
  return rtc_mem_read(RTC_FIFOHEAD_POS);
}

INTERNAL static inline void rtc_fifo_put_head(uint32_t val)
{
  rtc_mem_write(RTC_FIFOHEAD_POS,val);
}


INTERNAL static inline uint32_t rtc_fifo_get_tail_t(void)
{
  return rtc_mem_read(RTC_FIFOTAIL_T_POS);
}

INTERNAL static inline void rtc_fifo_put_tail_t(uint32_t val)
{
  rtc_mem_write(RTC_FIFOTAIL_T_POS,val);
}

INTERNAL static inline uint32_t rtc_fifo_get_head_t(void)
{
  return rtc_mem_read(RTC_FIFOHEAD_T_POS);
}

INTERNAL static inline void rtc_fifo_put_head_t(uint32_t val)
{
  rtc_mem_write(RTC_FIFOHEAD_T_POS,val);
}


INTERNAL static inline uint32_t rtc_fifo_get_count_internal(void)
{
  return rtc_mem_read(RTC_FIFOCOUNT_POS);
}

INTERNAL static inline uint32_t rtc_fifo_get_tagcount(void)
{
  return (rtc_mem_read(RTC_FIFOLOC_POS)>>16)&0xff;
}

INTERNAL static inline uint32_t rtc_fifo_get_tagpos(void)
{
  return (rtc_mem_read(RTC_FIFOLOC_POS)>>0)&0xff;
}

INTERNAL static inline uint32_t rtc_fifo_get_last(void)
{
  return (rtc_mem_read(RTC_FIFOLOC_POS)>>8)&0xff;
}

INTERNAL static inline uint32_t rtc_fifo_get_first(void)
{
  return rtc_fifo_get_tagpos()+rtc_fifo_get_tagcount();
}

INTERNAL static inline uint32_t rtc_fifo_get_size_internal(void)
{
  return rtc_fifo_get_last()-rtc_fifo_get_first();
}

INTERNAL static inline void rtc_fifo_put_count(uint32_t val)
{
  rtc_mem_write(RTC_FIFOCOUNT_POS,val);
}

INTERNAL static inline uint32_t rtc_fifo_normalise_index(uint32_t index)
{
  if (index>=rtc_fifo_get_last())
    index=rtc_fifo_get_first();
  return index;
}

INTERNAL static inline void rtc_fifo_increment_count(void)
{
  rtc_fifo_put_count(rtc_fifo_get_count_internal()+1);
}

INTERNAL static inline void rtc_fifo_decrement_count(void)
{
  rtc_fifo_put_count(rtc_fifo_get_count_internal()-1);
}

INTERNAL static inline uint32_t rtc_fifo_get_value(uint32_t entry)
{
  return entry&0xffff;
}

INTERNAL static inline uint32_t rtc_fifo_get_decimals(uint32_t entry)
{
  return (entry>>25)&0x07;
}

INTERNAL static inline uint32_t rtc_fifo_get_deltat(uint32_t entry)
{
  return (entry>>16)&0x1ff;
}

INTERNAL static inline uint32_t rtc_fifo_get_tagindex(uint32_t entry)
{
  return (entry>>28)&0x0f;
}

INTERNAL static inline uint32_t rtc_fifo_get_tag_from_entry(uint32_t entry)
{
  uint32_t index=rtc_fifo_get_tagindex(entry);
  uint32_t tags_at=rtc_fifo_get_tagpos();
  return rtc_mem_read(tags_at+index);
}

INTERNAL static inline void rtc_fifo_fill_sample(sample_t* dst, uint32_t entry, uint32_t timestamp)
{
  dst->timestamp=timestamp;
  dst->value=rtc_fifo_get_value(entry);
  dst->decimals=rtc_fifo_get_decimals(entry);
  dst->tag=rtc_fifo_get_tag_from_entry(entry);
}

INTERNAL static int32_t rtc_fifo_delta_t(uint32_t t, uint32_t ref_t)
{
  uint32_t delta=t-ref_t;
  if (delta>0x1ff)
    return -1;
  return delta;
}

INTERNAL static uint32_t rtc_fifo_construct_entry(uint32_t val, uint32_t tagindex, uint32_t decimals, uint32_t deltat)
{
  return (val & 0xffff) + ((deltat & 0x1ff) <<16) +
         ((decimals & 0x7)<<25) + ((tagindex & 0xf)<<28);
}

INTERNAL static inline int rtc_fifo_find_tag_index(uint32_t tag)
{
  uint32_t tags_at=rtc_fifo_get_tagpos();
  uint32_t count=rtc_fifo_get_tagcount();
  uint32_t i;

  for (i=0;i<count;i++)
  {
    uint32_t stag=rtc_mem_read(tags_at+i);

    if (stag==tag)
      return i;
    if (stag==0)
    {
      rtc_mem_write(tags_at+i,tag);
      return i;
    }
  }
  return -1;
}

INTERNAL static inline void rtc_fifo_clear_tags(void)
{
  uint32_t tags_at=rtc_fifo_get_tagpos();
  uint32_t count=rtc_fifo_get_tagcount();
  while (count--)
    rtc_mem_write(tags_at++,0);
}

INTERNAL static inline void rtc_fifo_clear_content(void)
{
  uint32_t first=rtc_fifo_get_first();
  rtc_fifo_put_tail(first);
  rtc_fifo_put_head(first);
  rtc_fifo_put_count(0);
  rtc_fifo_put_tail_t(0);
  rtc_fifo_put_head_t(0);
  rtc_fifo_clear_tags();
}

INTERNAL static inline void rtc_fifo_put_loc_internal(uint32_t first, uint32_t last, uint32_t tagcount)
{
  rtc_mem_write(RTC_FIFOLOC_POS,first+(last<<8)+(tagcount<<16));
}

INTERNAL static inline void rtc_fifo_init(uint32_t first, uint32_t last, uint32_t tagcount)
{
  rtc_fifo_put_loc_internal(first,last,tagcount);
  rtc_fifo_clear_content();
}

INTERNAL static inline void rtc_fifo_init_default(uint32_t tagcount)
{
  if (tagcount==0)
    tagcount=RTC_DEFAULT_TAGCOUNT;

  rtc_fifo_init(RTC_DEFAULT_FIFO_START,RTC_DEFAULT_FIFO_END,tagcount);
}

INTERNAL static inline void rtc_fifo_set_magic(void)
{
  rtc_mem_write(RTC_FIFO_MAGIC_POS,RTC_FIFO_MAGIC);
}

INTERNAL static inline void rtc_fifo_unset_magic(void)
{
  rtc_mem_write(RTC_FIFO_MAGIC_POS,0);
}


/////////////////////////////////////// API STARTS HERE ///////////////////////////////////
API static inline uint32_t rtc_fifo_get_count(void)
{
  return rtc_fifo_get_count_internal();
}

API static inline uint32_t rtc_fifo_get_maxval(void)
{
  return 0xffff;
}

// Minimum amount of samples which can successfully be held in this FIFO. Any more,
// and older data *may* be evicted
API static inline uint32_t rtc_fifo_get_size(void)
{
  return rtc_fifo_get_size_internal();
}

// Maximum amount of samples which can possibly be held in this FIFO. Any more, and
// eviction of older data is guaranteed. Eviction might happen earlier already.
API static inline uint32_t rtc_fifo_get_max_size(void)
{
  return rtc_fifo_get_size_internal();
}

API static inline void rtc_fifo_put_loc(uint32_t first, uint32_t last, uint32_t tagcount)
{
  rtc_fifo_put_loc_internal(first,last,tagcount);
}

// returns true if sample popped, false if not
API static inline bool rtc_fifo_pop_sample(sample_t* dst)
{
  uint32_t count=rtc_fifo_get_count_internal();

  if (count==0)
    return false;
  uint32_t head=rtc_fifo_get_head();
  uint32_t timestamp=rtc_fifo_get_head_t();
  uint32_t entry=rtc_mem_read(head);
  timestamp+=rtc_fifo_get_deltat(entry);
  rtc_fifo_fill_sample(dst,entry,timestamp);

  head=rtc_fifo_normalise_index(head+1);

  rtc_fifo_put_head(head);
  rtc_fifo_put_head_t(timestamp);
  rtc_fifo_decrement_count();
  return true;
}

// returns 1 if sample is available, 0 if not
API static inline bool rtc_fifo_peek_sample(sample_t* dst, uint32_t from_top)
{
  if (rtc_fifo_get_count_internal()<=from_top)
    return false;
  uint32_t head=rtc_fifo_get_head();
  uint32_t entry=rtc_mem_read(head);
  uint32_t timestamp=rtc_fifo_get_head_t();
  timestamp+=rtc_fifo_get_deltat(entry);

  while (from_top--)
  {
    head=rtc_fifo_normalise_index(head+1);
    entry=rtc_mem_read(head);
    timestamp+=rtc_fifo_get_deltat(entry);
  }

  rtc_fifo_fill_sample(dst,entry,timestamp);
  return true;
}

API static inline bool rtc_fifo_drop_samples(uint32_t from_top)
{
  uint32_t count=rtc_fifo_get_count_internal();

  if (count<=from_top)
    from_top=count;
  uint32_t head=rtc_fifo_get_head();
  uint32_t head_t=rtc_fifo_get_head_t();

  while (from_top--)
  {
    uint32_t entry=rtc_mem_read(head);
    head_t+=rtc_fifo_get_deltat(entry);
    head=rtc_fifo_normalise_index(head+1);
    rtc_fifo_decrement_count();
  }
  rtc_fifo_put_head(head);
  rtc_fifo_put_head_t(head_t);
  return true;
}

API static inline bool rtc_fifo_store_sample(const sample_t* s)
{
  uint32_t head=rtc_fifo_get_head();
  uint32_t tail=rtc_fifo_get_tail();
  uint32_t count=rtc_fifo_get_count_internal();
  int32_t tagindex=rtc_fifo_find_tag_index(s->tag);

  if (count==0)
  {
    rtc_fifo_put_head_t(s->timestamp);
    rtc_fifo_put_tail_t(s->timestamp);
  }
  uint32_t tail_t=rtc_fifo_get_tail_t();
  int32_t deltat=rtc_fifo_delta_t(s->timestamp,tail_t);

  if (tagindex<0 || deltat<0)
  { // We got something that doesn't fit into the scheme. Might be a long delay, might
    // be some sort of dynamic change. In order to go on, we need to start over....
    // ets_printf("deltat is %d, tagindex is %d\n",deltat,tagindex);

    rtc_fifo_clear_content();
    rtc_fifo_put_head_t(s->timestamp);
    rtc_fifo_put_tail_t(s->timestamp);
    head=rtc_fifo_get_head();
    tail=rtc_fifo_get_tail();
    count=rtc_fifo_get_count_internal();
    tagindex=rtc_fifo_find_tag_index(s->tag); // This should work now
    if (tagindex<0)
      return false; // Uh-oh! This should never happen
  }

  if (head==tail && count>0)
  { // Full! Need to remove a sample
    sample_t dummy;
    rtc_fifo_pop_sample(&dummy);
  }

  rtc_mem_write(tail++,rtc_fifo_construct_entry(s->value,tagindex,s->decimals,deltat));
  rtc_fifo_put_tail(rtc_fifo_normalise_index(tail));
  rtc_fifo_put_tail_t(s->timestamp);
  rtc_fifo_increment_count();
  return true;
}


API static inline bool rtc_fifo_check_magic(void)
{
  if (rtc_mem_read(RTC_FIFO_MAGIC_POS)==RTC_FIFO_MAGIC)
    return true;
  return false;
}

API static inline bool rtc_fifo_prepare(uint32_t tagcount)
{
  rtc_fifo_init_default(tagcount);
  rtc_fifo_set_magic();
  return true;
}
#endif
