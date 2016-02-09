# RomCfg Module

Provides read-only access to the 4k page of flash before the second OTA slot.

This module is linked to the otaupgrade module, and enabling either
automatically enables the other.

Note that reading from the romcfg area is only possible when the nodemcu-boot
loader is being used (as otherwise the romcfg area is not guaranteed to exist).

## romcfg.read()

Reads raw configuration data from the ROM.

#### Syntax
`romcfg.read(offs, len)`

#### Parameters
- `offs` The offset to start reading from (0-4095).
- `len` The number of bytes to read.

#### Returns
A string, possibly containing binary data.
