"""AXION API (phases 4 and 6 of the plan).

- POST /api/readings         ATOM ingestion; returns zone_count + whitelist_ver
- GET  /api/beacons          list registry
- POST /api/beacons          new beacon (bumps whitelist_ver)
- PUT  /api/beacons/{id}     edit/enable/disable (bumps whitelist_ver)
- DELETE /api/beacons/{id}   remove (bumps whitelist_ver)
- GET  /api/beacons/active   whitelist for firmware: {ver, macs}
- GET  /api/state            full state (zones, atoms)
- WS   /ws                   pushes state on every change
- /                          dashboard;  /beacons  registry
"""
import asyncio
import json
import os
import time

from fastapi import FastAPI, HTTPException, WebSocket, WebSocketDisconnect
from fastapi.responses import FileResponse
from fastapi.staticfiles import StaticFiles

from . import config, db
from .models import BeaconIn, BeaconPatch, ReadingsIn, ReadingsOut
from .zones import ZoneEngine

STATIC_DIR = os.path.join(os.path.dirname(__file__), "..", "static")

engine = ZoneEngine(config.HYSTERESIS_DB, config.HYSTERESIS_HOLD_S,
                    config.PRESENCE_TIMEOUT_S, config.PRESENCE_RSSI_MIN)

app = FastAPI(title="AXION BLE")

_ws_clients: set[WebSocket] = set()
_last_state_json: str = ""


def build_state():
    now = time.time()
    beacons = db.beacon_by_mac()
    atoms_rows = db.get_atoms()

    zones = {z: {"count": 0, "operators": []} for z in config.ZONES}
    for mac, zone in engine.recompute(now, active_macs=set(db.active_macs())).items():
        if zone not in zones:
            continue
        b = beacons.get(mac.upper()) or beacons.get(mac)
        rssi = engine.rssi_of(mac)
        zones[zone]["count"] += 1
        zones[zone]["operators"].append({
            "operator": b["operator"] if b else mac,
            "badge": b["badge"] if b else "",
            "mac": mac,
            "rssi": round(rssi, 1) if rssi is not None else None,
        })

    atoms = {}
    for a in atoms_rows:
        online = a["last_seen"] is not None and (now - a["last_seen"]) <= config.ATOM_OFFLINE_S
        atoms[a["id"]] = {
            "zone": a["zone"],
            "online": online,
            "last_seen": a["last_seen"],
            "count": zones.get(a["zone"], {}).get("count", 0),
        }
    return {"ts": now, "zones": zones, "atoms": atoms}


async def broadcast_state(force=False):
    global _last_state_json
    state = build_state()
    js = json.dumps(state, sort_keys=True)
    if not force and js == _last_state_json:
        return
    _last_state_json = js
    dead = []
    for ws in _ws_clients:
        try:
            await ws.send_text(js)
        except Exception:
            dead.append(ws)
    for ws in dead:
        _ws_clients.discard(ws)


async def _recompute_loop():
    while True:
        await asyncio.sleep(config.RECOMPUTE_PERIOD_S)
        try:
            await broadcast_state()
        except Exception:
            pass


@app.on_event("startup")
async def _startup():
    db.init_db()
    asyncio.create_task(_recompute_loop())


# ---------- ingestion ----------

@app.post("/api/readings", response_model=ReadingsOut)
async def post_readings(r: ReadingsIn):
    if r.atom_id not in config.ATOMS:
        raise HTTPException(404, f"unknown atom: {r.atom_id}")
    now = time.time()
    db.set_atom_seen(r.atom_id, now)
    active = set(db.active_macs())
    for b in r.beacons:
        mac = b.mac.upper()
        engine.add_reading(mac, r.atom_id, r.zone, b.rssi, now)
        if mac in active:
            db.persist_reading(r.atom_id, mac, b.rssi, b.n, now)
    engine.recompute(now, active_macs=active)
    zone_count = engine.counts().get(config.ATOMS[r.atom_id], 0)
    await broadcast_state()
    return ReadingsOut(zone_count=zone_count, whitelist_ver=db.get_whitelist_ver())


# ---------- beacon registry ----------

@app.get("/api/beacons")
async def get_beacons():
    return db.list_beacons()


@app.post("/api/beacons")
async def create_beacon(b: BeaconIn):
    try:
        bid = db.add_beacon(b.operator, b.badge, b.mac)
    except Exception as e:
        raise HTTPException(400, f"badge or MAC already registered ({e})")
    ver = db.bump_whitelist_ver()
    await broadcast_state()
    return {"id": bid, "whitelist_ver": ver}


@app.put("/api/beacons/{bid}")
async def edit_beacon(bid: int, patch: BeaconPatch):
    db.update_beacon(bid, patch.operator, patch.badge, patch.mac,
                     None if patch.active is None else int(patch.active))
    ver = db.bump_whitelist_ver()
    await broadcast_state()
    return {"ok": True, "whitelist_ver": ver}


@app.delete("/api/beacons/{bid}")
async def remove_beacon(bid: int):
    db.delete_beacon(bid)
    ver = db.bump_whitelist_ver()
    await broadcast_state()
    return {"ok": True, "whitelist_ver": ver}


@app.get("/api/beacons/active")
async def beacons_active():
    return {"ver": db.get_whitelist_ver(), "macs": db.active_macs()}


# ---------- state ----------

@app.get("/api/state")
async def get_state():
    return build_state()


@app.websocket("/ws")
async def ws(ws: WebSocket):
    await ws.accept()
    _ws_clients.add(ws)
    await ws.send_text(json.dumps(build_state(), sort_keys=True))
    try:
        while True:
            await ws.receive_text()
    except WebSocketDisconnect:
        pass
    finally:
        _ws_clients.discard(ws)


# ---------- pages ----------

@app.get("/")
async def index():
    return FileResponse(os.path.join(STATIC_DIR, "index.html"))


@app.get("/beacons")
async def beacons_page():
    return FileResponse(os.path.join(STATIC_DIR, "beacons.html"))


if os.path.isdir(STATIC_DIR):
    app.mount("/static", StaticFiles(directory=STATIC_DIR), name="static")
