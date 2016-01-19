# MACFrame Module

MAC frame parsing & creation module.

## macframe.parse()

Parses a MAC frame and splits it into its components.

#### Syntax
`macframe.parse(rawframe)`

#### Parameters
  - `rawframe` the raw frame data in a Lua string

#### Returns
A table containing the individual parts, appropriately named.

All MAC address are strings in aa-bb-cc-dd-ee-ff format.

The following keys may be found in the table:
  - `framecontrol` the frame control field, as a number
  - `duration` the duration field, as a number
  - `sequencecontrol` the sequence control field, as a number
  - `payload` the payload data, as a string
  - `destination` the destination MAC address
  - `source` the source MAC address
  - `bssid` if present, contains the BSS ID
  - `transmitter` if present, contains the WDS transmitter MAC address
  - `receiver` if present, contains the WDS receiver MAC address

Raises an error if the raw frame data is too short.

#### Example
```lua
frameparts = macframe.parse(rawframe)
```

#### See also
  - [`macframe.create()`](#macframecreate)

## macframe.create()

Assembles a MAC frame from a table containing the individual parts.

Feeding the output from `macframe.parse()` as input to this function recreates the original MAC frame, and vice versa.

#### Syntax
`macframe.create(frameparts)`

#### Parameters
  - `frameparts` a table describing the MAC frame; see [`macframe.parse()`](#macframeparse) for key description

#### Returns
A string containing the raw MAC frame data.

#### Example
```lua
rawframe2 = macframe.create(macframe.parse(rawframe))
```

#### See also
  - [`macframe.parse()`](#macframeparse)
