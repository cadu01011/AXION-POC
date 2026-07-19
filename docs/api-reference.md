# API reference

Base URL: `http://<server>:8000`. All bodies are JSON.

## POST /api/readings

Ingestion endpoint. Called by each ATOM every ~2 s.

Request:

```json
{
  "atom_id": "atom-a",
  "zone": "A",
  "seq": 1234,
  "uptime_ms": 123456,
  "beacons": [
    { "mac": "C3:FA:7B:12:5E:01", "rssi": -67.2, "n": 9 }
  ]
}
```

- `rssi` — filtered value over the window.
- `n` — number of advertisements kept in the window.
- Beacons with no reading in the window are omitted.

Response `200`:

```json
{ "zone_count": 2, "whitelist_ver": 7 }
```

- `zone_count` — operators currently in this ATOM's zone.
- `whitelist_ver` — bumps whenever the beacon registry changes; the firmware
  refetches the whitelist when it sees a higher value.

Errors: `404` unknown `atom_id`, `422` malformed body. Any non-200 counts as a
failure toward the OFFLINE threshold on the ATOM.

## GET /api/beacons

Returns the full registry.

```json
[ { "id": 1, "operator": "John", "badge": "CRC001", "mac": "48:87:2D:9D:8F:95", "active": 1 } ]
```

## POST /api/beacons

Register a beacon. Bumps `whitelist_ver`.

Request: `{ "operator": "John", "badge": "CRC005", "mac": "48:87:2D:9D:8F:95" }`
Response: `{ "id": 5, "whitelist_ver": 8 }`
Errors: `400` if the badge or MAC is already registered.

## PUT /api/beacons/{id}

Edit a beacon; any subset of fields. Bumps `whitelist_ver`.

Request: `{ "active": false }` (or `operator` / `badge` / `mac`)
Response: `{ "ok": true, "whitelist_ver": 9 }`

## DELETE /api/beacons/{id}

Remove a beacon. Bumps `whitelist_ver`. Response: `{ "ok": true, "whitelist_ver": 10 }`

## GET /api/beacons/active

Whitelist for the firmware.

```json
{ "ver": 7, "macs": ["48:87:2D:9D:8F:95", "48:87:2D:9D:8F:89"] }
```

## GET /api/state

Full state (also the WebSocket payload).

```json
{
  "ts": 1731000000.0,
  "zones": {
    "A": { "count": 2, "operators": [
      { "operator": "John", "badge": "CRC001", "mac": "48:87:2D:9D:8F:95", "rssi": -61.0 }
    ] },
    "B": { "count": 0, "operators": [] },
    "C": { "count": 0, "operators": [] }
  },
  "atoms": {
    "atom-a": { "zone": "A", "online": true, "last_seen": 1731000000.0, "count": 2 },
    "atom-b": { "zone": "B", "online": true, "last_seen": 1731000000.0, "count": 0 },
    "atom-c": { "zone": "C", "online": false, "last_seen": null, "count": 0 }
  }
}
```

## WS /ws

WebSocket. On connect the server sends the current `/api/state` payload, then a
fresh payload on every state change. Client messages are ignored (keep-alive).
