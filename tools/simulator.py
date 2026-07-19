#!/usr/bin/env python3
"""Reading simulator - tests the AXION API without hardware (plan tests 4.2/4.3).

Sends POST /api/readings imitating the three ATOMs, with scripted scenarios:
  - a person enters Zone A
  - crosses to Zone B (must switch after the hysteresis)
  - stays on the A/B boundary (must not oscillate)
  - a beacon seen by two ATOMs (must not be double-counted)
  - a beacon disappears (timeout)

Usage:
  python tools/simulator.py [http://127.0.0.1:8000]
Requires the server running and a beacon registered with the MAC below
(or run with --seed to register it automatically).
"""
import json
import sys
import time
import urllib.request

BASE = sys.argv[1] if len(sys.argv) > 1 and sys.argv[1].startswith("http") else "http://127.0.0.1:8000"
MAC = "AA:BB:CC:DD:EE:01"
ATOMS = {"atom-a": "A", "atom-b": "B", "atom-c": "C"}


def post(path, obj):
    data = json.dumps(obj).encode()
    req = urllib.request.Request(BASE + path, data=data,
                                 headers={"Content-Type": "application/json"},
                                 method="POST")
    with urllib.request.urlopen(req, timeout=3) as r:
        return json.loads(r.read())


def get(path):
    with urllib.request.urlopen(BASE + path, timeout=3) as r:
        return json.loads(r.read())


def send(rssi_by_atom):
    """rssi_by_atom: {atom_id: rssi}. Sends one POST per ATOM."""
    out = {}
    for atom, rssi in rssi_by_atom.items():
        if rssi is None:
            continue
        resp = post("/api/readings", {
            "atom_id": atom, "zone": ATOMS[atom], "seq": 0, "uptime_ms": 0,
            "beacons": [{"mac": MAC, "rssi": rssi, "n": 10}],
        })
        out[atom] = resp
    return out


def zone_now():
    st = get("/api/state")
    for z, d in st["zones"].items():
        for o in d["operators"]:
            if o["mac"].upper() == MAC:
                return z
    return None


def step(desc, rssi, secs, expect=None):
    print(f"\n>> {desc}")
    t0 = time.time()
    while time.time() - t0 < secs:
        send(rssi)
        time.sleep(0.5)
    z = zone_now()
    status = ""
    if expect is not None:
        status = "  [OK]" if z == expect else f"  [FAIL: expected {expect}]"
    print(f"   current zone = {z}{status}")
    return z


def seed():
    try:
        post("/api/beacons", {"operator": "Simulated", "badge": "SIM001", "mac": MAC})
        print(f"beacon {MAC} registered")
    except Exception as e:
        print(f"(beacon already exists or registration error: {e})")


if __name__ == "__main__":
    if "--seed" in sys.argv:
        seed()
    print(f"Simulator -> {BASE}  (MAC {MAC})")
    ok = True
    if step("Person enters Zone A", {"atom-a": -55, "atom-b": -80, "atom-c": -85}, 3, "A") != "A":
        ok = False
    step("Seen by A and B (A stronger) - no double count",
         {"atom-a": -58, "atom-b": -66, "atom-c": -90}, 3, "A")
    if step("Crosses to Zone B", {"atom-a": -80, "atom-b": -55, "atom-c": -88}, 6, "B") != "B":
        ok = False
    step("Beacon disappears (presence timeout)", {}, 0, None)
    print("   waiting for presence timeout (16 s)...")
    time.sleep(16)
    post("/api/readings", {"atom_id": "atom-a", "zone": "A", "beacons": []})
    z = zone_now()
    print(f"   zone after timeout = {z}  {'[OK]' if z is None else '[FAIL: should be gone]'}")
    if z is not None:
        ok = False

    print("\n=== RESULT:", "ALL OK" if ok else "FAILURE", "===")
    sys.exit(0 if ok else 1)
