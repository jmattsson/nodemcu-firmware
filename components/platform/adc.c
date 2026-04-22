#include "platform.h"
#include "driver/adc.h"

// *****************************************************************************
// ADC

int platform_adc_exists( uint8_t adc ) { return adc < 2 && adc > 0; }

int platform_adc_channel_exists( uint8_t adc, uint8_t channel ) {
  return (adc == 1 && channel < 8);
}

uint8_t platform_adc_set_width( uint8_t adc, int bits ) {
  (void)adc;
  bits = bits - 9;
  if (ESP_OK != adc1_config_width( bits ))
    return 0;

  return 1;
}

uint8_t platform_adc_setup( uint8_t adc, uint8_t channel, uint8_t atten ) {
  if (adc == 1 && ESP_OK != adc1_config_channel_atten( channel, atten ))
    return 0;

  return 1;
}

int platform_adc_read( uint8_t adc, uint8_t channel ) {
  int value = -1;
  if (adc == 1) value = adc1_get_raw( channel );
  return value;
}
