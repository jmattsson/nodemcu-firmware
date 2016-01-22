#ifndef RTC_SAMPLER_H
#define RTC_SAMPLER_H

//
// Sample-taking support. This provides state storage for deep-sleeping samping applications that
// take samples without a full bootup most of the time.
//
// 0: magic
// 1: measurement alignment, in microseconds
// 2: randomisation of aligned sleep, in us
// 3: Number of samples to take before doing a "real" boot. Decremented as samples are obtained
// 4: Reload value for (10). Needs to be applied by the firmware in the real boot (rtc_restart_samples_to_take())

#define RTC_SAMPLER_BASE          10
#define RTC_SAMPLER_MAGIC         0x64695553

#define RTC_SAMPLER_MAGIC_POS  (RTC_SAMPLER_BASE+0)
#define RTC_ALIGNMENT_POS      (RTC_SAMPLER_BASE+1)
#define RTC_RANDOMISE_POS      (RTC_SAMPLER_BASE+2)
#define RTC_SAMPLESTOTAKE_POS  (RTC_SAMPLER_BASE+3)
#define RTC_SAMPLESPERBOOT_POS (RTC_SAMPLER_BASE+4)

#include "rtctime.h"
#include "rtcaccess.h"

#define INTERNAL // Just for keeping track
#define API      // ditto

#ifndef RTCTIME_SLEEP_ALIGNED
# define RTCTIME_SLEEP_ALIGNED rtc_time_deep_sleep_until_aligned
#endif

INTERNAL static inline void rtc_sampler_set_magic(void)
{
  rtc_mem_write(RTC_SAMPLER_MAGIC_POS,RTC_SAMPLER_MAGIC);
}

INTERNAL static inline void rtc_sampler_unset_magic(void)
{
  rtc_mem_write(RTC_SAMPLER_MAGIC_POS,0);
}




API static inline uint32_t rtc_get_samples_to_take(void)
{
  return rtc_mem_read(RTC_SAMPLESTOTAKE_POS);
}

API static inline void rtc_put_samples_to_take(uint32_t val)
{
  rtc_mem_write(RTC_SAMPLESTOTAKE_POS,val);
}

API static inline void rtc_decrement_samples_to_take(void)
{
  uint32_t stt=rtc_get_samples_to_take();
  if (stt)
    rtc_put_samples_to_take(stt-1);
}

API static inline void rtc_restart_samples_to_take(void)
{
  rtc_put_samples_to_take(rtc_mem_read(RTC_SAMPLESPERBOOT_POS));
}


API static inline uint8_t rtc_sampler_check_magic(void)
{
  if (rtc_mem_read(RTC_SAMPLER_MAGIC_POS)==RTC_SAMPLER_MAGIC)
    return 1;
  return 0;
}

API static inline void rtc_sampler_deep_sleep_until_sample(uint32_t min_sleep_us)
{
  uint32_t align=rtc_mem_read(RTC_ALIGNMENT_POS);
  uint32_t rand_us=rtc_mem_read(RTC_RANDOMISE_POS);
  RTCTIME_SLEEP_ALIGNED(align,min_sleep_us,rand_us);
}


API static inline void rtc_sampler_prepare(uint32_t samples_per_boot, uint32_t us_per_sample, uint32_t us_rand)
{
  rtc_mem_write(RTC_SAMPLESPERBOOT_POS,samples_per_boot);
  rtc_mem_write(RTC_ALIGNMENT_POS,us_per_sample);
  rtc_mem_write(RTC_RANDOMISE_POS,us_rand);
  rtc_put_samples_to_take(0);
  rtc_sampler_set_magic();
}

API static inline void rtc_sampler_trash(void)
{
  rtc_sampler_unset_magic();
}


#endif
