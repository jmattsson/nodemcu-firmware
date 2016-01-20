# LOCP Module

Module for receiving Cisco LOcation Control Protocol frames.

LOCP is a proprietary protocol used by Cisco. It enables determining the location of LOCP-enabled mobile devices through measuring RSSI at multiple APs or forwarders.

The protocol appears to be largely undocumented, with only minimal information being [publicly available](http://www.cisco.com/c/en/us/td/docs/solutions/Enterprise/Mobility/WiFiLBS-DG/AppdxC.html).

Espressif does however support the receiving of LOCP frames, for a quite loose definition of "LOCP". All frames matching the following constraints are accepted:
  - `To-WDS` flag in the `frame control` field of the frame must be set
  - `From-WDS` flag in the `frame control` field of the frame must be set 
  - `Receiver Address` must be a multicast address

To send frames which can be received through this interface, the [`raw80211.send()`](#raw80211send) function can be used.

!!! note "Note:"

This is for the faint of heart. Having the knowledge and ability to do packet traces on a WiFi network, as well as a solid understanding of the 802.11 MAC header format is required to make good use of this module.

#### See also
  - [`raw80211.send()`](#raw80211send)
  - [`macframe.parse()`](#macframeparse)
  - [`macframe.create()`](#macframecreate)


## locp.register()

Registers a callback function to receive LOCP frames.

#### Syntax
`locp.register(cb)`

#### Parameters
  - `cb` the callback function of the form `function(frame, rssi) end`

The `frame` argument is the 802.11 frame (header + payload) as a binary string.

Signal strength is provided in the `rssi` argument.

#### Returns
`nil`

#### Example
```lua
function on_locp(frame, rssi)
  print("Received LOCP frame with length", frame:len(), "RSSI", rssi)
end
locp.register(on_locp)
```

#### See also
  - [`macframe.parse()`](#macframeparse)


## locp.unregister()

Unregisters the previously registered LOCP callback function.

#### Syntax
`locp.unregister()`

#### Parameters
None

#### Returns
`nil`

#### Example
```lua
locp.unregister()
```
