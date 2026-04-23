# ADC Module
| Since  | Origin / Contributor  | Maintainer  | Source  |
| :----- | :-------------------- | :---------- | :------ |
| 2017-04-22 | [zelll](https://github.com/zelll) | [jmattsson](https://github.com/jmattsson) | [adc.c](../../components/modules/adc.c)|

The ADC module provides access to the in-built ADC hardware.

Due to the original Espressif ADC driver being removed in ESP IDF 6.0, this
module had to be rewritten in a not backwards compatible manner. Also, the
old module was written for only the ESP32, and the series has grown
substantially since then.

Instead of the old behaviour which was locked to ADC1, the current `adc`
module offers more flexibility. On chips where ADC2 is available, it too
may be used.

To begin, first claim the ADC hardware unit by calling `adc.setup(adc.ADC1)`
(or `adc.ADC2`). This returns an adc instance which can then be configured and
read from. When no further ADC readings are required, the instance can
either be manually closed or left to be garbage collected, at which point
it will be closed automatically.

Quick example:
```lua
myadc = adc.setup(adc.ADC1)
myadc:configure(adc.CHANNEL_0, adc.BITS_9, adc.ATTEN_6)
val1 = myadc:read(adc.CHANNEL_0)
-- later
val2 = myadc:read(adc.CHANNEL_0)
-- when done (if ever)
myadc:close()
```

## adc.setup()

Claim the hardware ADC unit and initialise it.

#### Syntax
`adc.setup(adc_unit`)

#### Parameters
- `adc_unit` One of `adc.ADC1` or `adc.ADC2` (chip/module dependent).

#### Returns
An adc instance object. If the ADC has already been claimed, or cannot be
initialised for another reason, then an error will be raised.

#### Example
```lua
myadc = adc.setup(adc.ADC2)
```

## adc.obj:configure()

ADC channel configuration on a set up adc instance.

#### Syntax
`myadc:configure(adc_channel, bits, attenuation)`

#### Parameters
- `adc_channel` One of `adc.CHANNEL_0` through to `adc.CHANNEL_10` (chip/module dependent). Check the datasheet for what channel corresponds to what pin.
- `bits` The bit width for the ADC, one of `adc.BITS_9` through to `adc.BITS_13`, or `nil` to use the default (maximum) the hardware supports.
- `attenuation` Signal attenuation, one of:
  - `adc.ATTEN_0` for no attentuation
  - `adc.ATTEN_2_5` for 2.5dB attenuation (reduce signal by ~25%)
  - `adc.ATTEN_6` for 6dB attenuation (reduce signal by ~50%)
  - `adc.ATTEN_12` for 12dB attenuation (reduce signal by ~75%)
  - `nil` for the default (no attenuation)

#### Returns
`nil`

#### Examples
Default configuration of channel 0:
```lua
myadc:configure(adc.CHANNEL_0)
```

Bitwidth configuration on channel 2, with no attenuation:
```lua
myadc:configure(adc.CHANNEL_2, adc.BITS_12)
```

Attenuation on channel 5, with default bit width.
```lua
myadc:configure(adc.CHANNEL_5, nil, adc.ATTEN_2_5)
```

Custom bit width and attentuation of channel 8:
```lua
myadc:configure(adc.CHANNEL_8, adc.BITS_9, adc.ATTEN_12)
```


## adc.obj:read()

Obtains a reading of the ADC. Prior to reading, ensure the channel has been
configured as needed.

#### Syntax
`myadc:read(adc_channel)`

#### Parameters
- `adc_channel` One of `adc.CHANNEL_0` through to `adc.CHANNEL_10` (chip/module dependent). Check the datasheet for what channel corresponds to what pin.

#### Returns
The raw ADC value, as an integer. An error is raised in case of failure.

#### Example
```lua
val = myadc:read(adc.CHANNEL_0)
```


## adc.obj:close()

Closes the adc instance and releases the underlying hardware. No further
reading or configuring can be done on this object after this. To use the
ADC again, a new instance must be obtained via `adc.setup()`.

It is optional to call this function. If the adc instance gets garbage
collected, it will be automatically closed then.

A common, valid usage scenario sees the ADC configured at start and then never
closed/released.

#### Syntax
`myadc:close()`

#### Parameters
None.

#### Returns
`nil`
