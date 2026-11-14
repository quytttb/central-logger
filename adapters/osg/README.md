# OSG Adapter — EV-24 / MDATA 8690

`osg_adapter` is a separate process on the Central computer or another
authorized LAN host. It reads OSG's HTTP API and presents an edge-compatible
REST config v1 and Modbus TCP map v1 to Central. It does not run on the OSG
logger. Vendor-independent v1 servers and `EdgeSource` live in
`adapters/common/`; an adapter for a different vendor can implement that
interface without changing OSG-specific code.

**Verification status:** permitted, time-limited tests against the actual
OSG devices confirmed `/api/sensors` and `/api/data/realtime` return arrays
under `data`. `.17` exposes six IDs — 159 O2 (`%v`), 160 CO, 161 NO,
162 SO2, 164 NO2, 165 NOx (all `mg/Nm³` except O2) — and `.16` exposes seven
IDs — 164 O2 (`%V`), 165 CO, 166 NO, 167 `NO2 CONVERTER`, 168 SO2, 169 H2O
(`%`), 170 `mA` (empty unit). The adapter served those IDs to a local Modbus
client and returned them in REST config. Catalog `status` and realtime
`status` disagree on both devices (e.g. `.17` ID 164 catalog 3 vs realtime 2;
`.16` catalog all 3 vs realtime all 0), and `.17` uses lowercase `%v` while
`.16` uses uppercase `%V`, so per-device quality policies and expected units
must stay separate. The meanings of `status`, `syncing`, `alert`, `enabled`
and `time` still need manufacturer confirmation. The parser deliberately rejects unrecognized response envelopes,
invalid IDs and duplicate IDs. Do not treat this adapter as production-ready
before completing the hardware checklist below.

## Safety and permissions

- The only write request sent upstream is `POST /api/auth/login`. All data
  requests are `GET /api/sensors` and `GET /api/data/realtime`; the adapter
  never forwards Central's configuration requests to OSG.
- The adapter does **not** enable Modbus RTU or TCP on OSG. OSG's RTU output
  and its possible sensor-side RTU connection are different things.
- Default listeners are `127.0.0.1:15020` (Modbus) and `127.0.0.1:18080`
  (REST). Modbus has no authentication. Non-loopback bind requires explicit
  `--allow-remote`; use only on an authorized, trusted LAN. Do not expose
  either listener to the Internet.
- OSG has only been verified over plain HTTP. Password, Bearer token and
  readings can be observed or altered on that network. Use a read-only
  account if provided; use a protected LAN. The adapter disables system proxy
  use for OSG to avoid sending credentials to an unintended proxy.
- The OSG access token is held in memory, not written or logged. Credentials
  and the **different** adapter REST token are read from private files outside
  the repository; on Unix group/other permissions must be absent. Never put
  secrets on the command line or in the repository.

## Build

Qt 6.11 with Core, Network and SerialBus is required, in addition to the
existing Central build dependencies:

```bash
cmake -S . -B build -DCMAKE_PREFIX_PATH=/path/to/Qt/6.11/gcc_64
cmake --build build --target osg_adapter
ctest --test-dir build -R 'test_osg_adapter|test_sensor_merger|test_poll_history_store' --output-on-failure
```

Use `-DCENTRAL_LOGGER_BUILD_ADAPTERS=OFF` to build Central without adapters.
Nothing starts automatically.

## Prepare and run

In a private directory **outside the repo**, prepare a JSON file containing
only `username` and `password`. Separately generate an adapter token:

```bash
umask 077
TOKEN_FILE=$(mktemp /tmp/opencode/osg-adapter-token.XXXXXX)
openssl rand -hex 32 > "$TOKEN_FILE"
printf 'Adapter token file: %s\n' "$TOKEN_FILE"
```

Restrict both files to mode `0600`. `/tmp` contents may be removed at reboot;
use an appropriately protected persistent location when appropriate. Create
the adapter token once and reuse it in Central; generating a different token
requires updating Central's saved API token.

Example, substituting paths and a verified host:

```bash
./build/adapters/osg/osg_adapter \
  --osg-url http://LOGGER_HOST \
  --credentials-file /private/path/osg-credentials.json \
  --adapter-token-file /private/path/osg-adapter-token \
  --station-code OSG-TEST \
  --station-name 'OSG test'
```

The process starts upstream polling only when explicitly launched. Stop it
with Ctrl+C. `--poll-ms`, `--timeout-ms` and `--stale-ms` are configurable;
defaults are 10 s, 5 s and 30 s. Requests are sequential, with a deadline and
bounded exponential backoff on errors. Reauthentication is attempted once
per cycle after a 401. Catalog refreshes at most once per minute during normal
operation. No periodic background service is installed.

In Central, Add Logger with host `127.0.0.1`, Modbus port `15020`, Unit ID `1`,
REST port `18080`, and the value from the adapter-token file in API Token.
Wait for the adapter to load the OSG catalog, then use **Connect & Load
Config**. Before the first successful catalog load, config returns 503 rather
than an empty, misleading catalog. Use the station name and poll interval
returned by Connect if a read-only save is desired.

`POST /api/v1/config` is rejected with a clear read-only response; a stale
revision gets 409. Central currently saves its local row before an asynchronous
POST and may show a non-blocking warning if the form changes `station_name` or
`poll_interval`. No part of that POST is sent to OSG. Revisions change when the
virtual config/catalog changes; reload the form before saving.
`GET /api/v1/readings` is authenticated cached diagnostics only, and
`GET /api/v1/reports/latest` returns 404.

### Two OSG loggers on one Central computer

Run one foreground adapter process per OSG device, in separate terminals, with
different station codes, listener ports and independently generated REST
tokens. The same protected OSG credential file can be used if both devices
really share the account. For example, after independently verifying both
addresses and preparing the two token files:

```bash
./build/adapters/osg/osg_adapter \
  --osg-url http://192.168.1.17 \
  --credentials-file /private/path/osg-credentials.json \
  --adapter-token-file /private/path/osg-17-token \
  --station-code OSG-17 --station-name 'OSG 17' \
  --modbus-port 15020 --rest-port 18080
```

```bash
./build/adapters/osg/osg_adapter \
  --osg-url http://192.168.1.16 \
  --credentials-file /private/path/osg-credentials.json \
  --adapter-token-file /private/path/osg-16-token \
  --station-code OSG-16 --station-name 'OSG 16' \
  --modbus-port 15021 --rest-port 18081 \
  --analog-ids 164,165,166,167,168,169,170
```

Add each in Central using host `127.0.0.1`, the respective Modbus and REST ports,
and the respective token. ID `170` on `.16` has an empty unit in the observed
API and remains invalid until its unit and quality are verified. These two
loggers reuse IDs for different channels (for example, `164` is O2 on `.16`
but NO2 on `.17`); Central's Summary separates columns when ID, name or unit
differs. Keep per-device quality policies and units separate.

## Data quality and known limitations

- Sensor IDs come from the OSG catalog, not from an assumed OSG Modbus map.
  They must be integral, unique and fit uint16. Only the six observed gas
  channel IDs are enabled as analog by default; additional/unmapped IDs are
  ignored and counted in debug. On another installation, explicitly supply
  `--analog-ids` only after verifying each channel is analog. Metadata and units
  are taken from the catalog; a missing or conflicting realtime unit causes
  invalid quality.
- Empty strings are not numbers. `"0"` parses as numeric zero; it is marked
  valid **only** when all verified quality conditions also pass.
- By default **no OSG status, syncing or alert value is trusted**: all values
  are reported invalid, shown as `—` in Central and excluded from average
  exports. This is intentional, not a claim that OSG sensors are broken.
  The debug endpoint exposes selected cached source fields, including raw value,
  status, time, enabled, alert and syncing, for investigation.
  The v1 alarm bit is clear when no alarm can be verified; it is **not** proof
  that no source alarm exists.
- After the manufacturer/operator has verified exact meanings for this firmware,
  an optional external `--quality-policy` JSON file must contain four arrays:
  `valid_statuses`, `acceptable_syncing`, `no_alarm_values`, and
  `alarm_values`, plus an `expected_units` object mapping sensor ID strings to
  verified units. Values must match the JSON types returned by OSG exactly;
  alarm and no-alarm lists must not overlap. Empty lists trust nothing. Do not
  fill these arrays by guessing. A numeric value also requires `enabled: true`,
  a catalog unit matching its explicitly approved `expected_units` entry, and a
  parseable source timestamp. If a unit changes, its reading stays invalid until
  an operator verifies the change, updates the policy, and reloads/saves the
  catalog in Central. Before accepting data after a restart, verify Central's
  stored units: v1 has no acknowledgment proving the catalog was saved.
  The following empty policy is safe but trusts no readings:

  ```json
  {
    "valid_statuses": [],
    "acceptable_syncing": [],
    "no_alarm_values": [],
    "alarm_values": [],
    "expected_units": {}
  }
  ```
- Expired timestamps, source disconnects and timeouts set the stale bit;
  unverified or empty data clear valid. The adapter treats future-dated
  source timestamps beyond 30 s as stale (clock skew guard) and rejects
  timestamps outside the range `(0, 10^15)` ms. Central's detail and summary values
  hide invalid/stale data. The RTU-connected flag remains false: successful
  HTTP does not prove physical RTU connectivity. Central currently marks
  sensors `ERR` while polling with RTU disconnected, even if other quality
  criteria pass.
- Map v1 has **one** uint32 seconds timestamp, whereas OSG has milliseconds per
  sensor. The adapter uses the oldest timestamp among valid, non-stale samples,
  rounded down to seconds; zero means no such timestamp was available. This
  is a conservative snapshot marker, **not** each sensor's original time.
  Cached debug data retains the individual raw timestamps. Central's displayed
  live time and `recorded_at` are based on Central's poll time; the shared
  header timestamp is stored in `logger_timestamp`.
- There is no confirmed DI/DO mapping (`Ndi=Ndo=0`) or report mapping. A single
  Central Modbus client is accepted at a time. Its header read pins a snapshot
  for the following chunks; an incomplete read expires and must restart.
- On upstream loss the adapter can still be reachable, so Central's logger
  **online** badge describes the adapter connection, not OSG health. Check
  the cached debug endpoint and sensor quality. If Central loses the adapter,
  cached table values are hidden and marked STALE; they return after a successful
  poll. Old chart points can remain visible and lines may bridge gaps; they are
  not new readings.

### History and averages

Central stores a point on value/quality change and at a 15-minute heartbeat,
including invalid or stale rows with flags; an invalid Modbus float is a
transport placeholder, not a zero measurement. The history table hides it,
and table CSV leaves its value blank. Average CSV filters `valid=1 AND stale=0`,
but uses SQL `AVG` over the **stored rows** and `recorded_at`, not a
time-weighted average of every original sample. Store-on-change therefore
overweights frequently changing values and misses intervals without valid data.
Do not claim regulatory/reporting suitability until timestamps, coverage,
quality semantics and the averaging method are independently validated.

## Hardware checklist — explicit operator permission required

1. Verify Wi-Fi/LAN, device identity, firmware version and approved read-only
   account. Remember that the same IP can identify another device on another
   network.
2. With permission, make one login and inspect only the two GET responses.
   Confirm full schema, stable IDs, units, `enabled`, empty versus zero,
   timestamps, and behavior for expired tokens.
3. Ask OSG for the exact meanings of **every** observed `status`, `syncing`
   and `alert` value, and for source timestamps/clock synchronization.
4. With permission, run the adapter briefly. Verify Central Connect/Save,
   sensor catalog, raw debug and Modbus registers, then stop it. Confirm no
   configuration/control writes reach OSG.
5. Verify stale, disconnect and recovery without disrupting the plant; compare
   against the source and assess history/averaging before any operational use.
