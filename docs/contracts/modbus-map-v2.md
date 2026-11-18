# Modbus TCP map v2 — TT10 status and quality

This is a new, explicitly versioned map; it does not redefine
[`modbus-map-v1.md`](modbus-map-v1.md). Central accepts both versions. The
adapter and data-logger now publish v2. Older Central builds reject v2.

FC03/FC02/FC01, addressing, chunking, float order and the eight-register
analog block size remain as in v1.

## Header

- HR0 is `2`.
- HR1–HR6 retain their v1 meanings.
- HR7 specifies the source's label for custom code `03`: `0` = Maintenance,
  `1` = Auto. Unknown values must not be interpreted as Auto.
- HR8 indicates upstream reachability: `0` = not reported, `1` = connected,
  `2` = disconnected. Central treats `2` as an unavailable station even if
  its local adapter still accepts Modbus connections. A data-logger without
  an intermediary may use `0`.
- HR9 is reserved, zero.

## Analog block

- `+0`: sensor ID.
- `+1`: independent quality flags, bit 0 valid, bit 1 threshold alarm,
  bit 2 stale. An alarm is not a device error.
- `+2..+3`: IEEE-754 float, ABCD. Ignore the value when invalid or stale.
- `+4`: status `0` = Measuring, `1` = Calibrating, `2` = Error,
  `3` = custom (Maintenance or Auto); `0xffff` = unknown/unavailable.
- `+5..+7`: reserved, zero.

Status and quality are independent. In particular, an error status may be
known even when its numeric value is unavailable. An HTTP timeout does not
prove a sensor is in error: report unknown and stale instead. Unknown status
must never be converted to Measuring. For v1, quality flags cannot faithfully
represent TT10; Central shows `Unknown` instead of inventing a status.

The OSG mapping above follows `SensorStatus` from the observed
`/api/data/defines`. `03` is a vendor-specific custom state, not a claim
that Auto and Maintenance are equivalent. Confirmation of `syncing`, `alert`,
units and timestamps remains necessary for production use.
