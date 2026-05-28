# Provisioning QR v1 (`central-logger-provision/v1`)

Pairing **Central App** with a **data-logger** on the factory LAN without exposing the REST Bearer token via `GET /config`.

## Payload (UTF-8 JSON, one line in QR)

```json
{
  "schema": "central-logger-provision/v1",
  "api_token": "<rest_api_token from edge>",
  "host": "192.168.1.50",
  "api_port": 8080,
  "modbus_port": 5020,
  "modbus_unit_id": 1,
  "station_code": "TRAM-BD-001",
  "station_name": "Trạm mẫu"
}
```

| Field | Required | Central form field |
|-------|----------|-------------------|
| `schema` | Yes, exact string above | Validated |
| `api_token` | Yes | API Token |
| `host` | Recommended | Host IP |
| `api_port` | No (default 8080) | API Port |
| `modbus_port` | No (default 5020) | Modbus Port (Central column) |
| `modbus_unit_id` | No (default 1) | Unit ID (Central column) |
| `station_code` | No | Device: Station code |
| `station_name` | No | Device: Station name; Add: Name hint |

## Edge (data-logger)

Generate QR in **Settings → HTTP REST Server** (separate repo). Same JSON schema.

## Central

**Add / Edit Logger → Scan QR…** → select PNG/JPG → fields filled.

- **Linux:** `sudo apt install libzbar0` + Python deps `pyzbar`, `Pillow`.
- **Windows deploy:** run `scripts/stage_zbar_windows.ps1` (auto-downloads pinned DLLs) before `pyside6-deploy`; bundle ends up under `native/windows/` next to the `.exe` (see [`resources/native/windows/README.md`](../resources/native/windows/README.md)).

## Security

- QR contains a secret; use only on LAN during installation.
- Do not log `api_token` in application logs.
