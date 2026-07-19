"""Zone decision algorithm (section 3.5 of the plan) - pure, no I/O.

Decides which zone each beacon is in by comparing the filtered RSSI reported
by the three ATOMs (argmax), with temporal hysteresis (a margin in dB held for
a minimum time) and a presence timeout. Unit-testable in isolation
(tests/test_zones.py).
"""
from dataclasses import dataclass


@dataclass
class Reading:
    rssi: float
    zone: str
    ts: float


class ZoneEngine:
    def __init__(self, hysteresis_db, hysteresis_hold_s, presence_timeout_s,
                 presence_rssi_min):
        self.hysteresis_db = hysteresis_db
        self.hysteresis_hold_s = hysteresis_hold_s
        self.presence_timeout_s = presence_timeout_s
        self.presence_rssi_min = presence_rssi_min
        self.readings = {}    # mac -> {atom_id: Reading}
        self.presence = {}    # mac -> {"zone", "since", "last_seen", "rssi"}
        self.pending = {}     # mac -> {"zone", "since"}

    def add_reading(self, mac, atom_id, zone, rssi, ts):
        self.readings.setdefault(mac, {})[atom_id] = Reading(rssi, zone, ts)

    def recompute(self, now, active_macs=None):
        """Re-evaluate every beacon's zone. active_macs = set of active MACs
        (whitelist); if given, beacons outside it are dropped. Returns
        {mac: zone}."""
        for mac in list(self.readings.keys()):
            if active_macs is not None and mac not in active_macs:
                self.readings.pop(mac, None)
                self.presence.pop(mac, None)
                self.pending.pop(mac, None)
                continue

            live = {a: r for a, r in self.readings[mac].items()
                    if now - r.ts <= self.presence_timeout_s}

            best = None
            if live:
                best_atom = max(live, key=lambda a: live[a].rssi)
                best = live[best_atom]

            if best is None or best.rssi < self.presence_rssi_min:
                self.presence.pop(mac, None)
                self.pending.pop(mac, None)
                continue

            cur = self.presence.get(mac)
            if cur is None:
                self.presence[mac] = {"zone": best.zone, "since": now,
                                      "last_seen": now, "rssi": best.rssi}
                self.pending.pop(mac, None)
                continue

            cur["last_seen"] = now
            cur["rssi"] = best.rssi
            if best.zone == cur["zone"]:
                self.pending.pop(mac, None)
                continue

            cur_rssi = max((live[a].rssi for a in live if live[a].zone == cur["zone"]),
                           default=-999.0)
            if best.rssi - cur_rssi >= self.hysteresis_db:
                p = self.pending.get(mac)
                if p is None or p["zone"] != best.zone:
                    self.pending[mac] = {"zone": best.zone, "since": now}
                elif now - p["since"] >= self.hysteresis_hold_s:
                    self.presence[mac] = {"zone": best.zone, "since": now,
                                          "last_seen": now, "rssi": best.rssi}
                    self.pending.pop(mac, None)
            else:
                self.pending.pop(mac, None)

        return {mac: p["zone"] for mac, p in self.presence.items()}

    def counts(self):
        c = {}
        for p in self.presence.values():
            c[p["zone"]] = c.get(p["zone"], 0) + 1
        return c

    def zone_of(self, mac):
        p = self.presence.get(mac)
        return p["zone"] if p else None

    def rssi_of(self, mac):
        p = self.presence.get(mac)
        return p["rssi"] if p else None
