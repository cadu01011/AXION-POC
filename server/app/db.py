"""SQLite access (section 3.4 of the plan). One connection per operation
(POC), WAL enabled. Stores beacons, atoms and the reading history (the
dataset for the phase 7 calibration). Presence/zone lives in memory in the
ZoneEngine.
"""
import sqlite3
import time

from . import config

SCHEMA = """
CREATE TABLE IF NOT EXISTS beacons (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    operator TEXT NOT NULL,
    badge TEXT UNIQUE NOT NULL,
    mac TEXT UNIQUE NOT NULL,
    active INTEGER NOT NULL DEFAULT 1
);
CREATE TABLE IF NOT EXISTS atoms (
    id TEXT PRIMARY KEY,
    zone TEXT UNIQUE NOT NULL,
    last_seen REAL
);
CREATE TABLE IF NOT EXISTS readings (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    server_ts REAL NOT NULL,
    atom_id TEXT NOT NULL,
    mac TEXT NOT NULL,
    rssi REAL NOT NULL,
    n INTEGER NOT NULL
);
CREATE INDEX IF NOT EXISTS idx_readings_ts ON readings(server_ts);
CREATE TABLE IF NOT EXISTS meta (
    key TEXT PRIMARY KEY,
    value TEXT NOT NULL
);
"""


def _conn():
    c = sqlite3.connect(config.DB_PATH, timeout=5)
    c.row_factory = sqlite3.Row
    c.execute("PRAGMA journal_mode=WAL")
    return c


def init_db():
    with _conn() as c:
        c.executescript(SCHEMA)
        for atom_id, zone in config.ATOMS.items():
            c.execute("INSERT OR IGNORE INTO atoms(id, zone, last_seen) VALUES (?,?,NULL)",
                      (atom_id, zone))
        c.execute("INSERT OR IGNORE INTO meta(key, value) VALUES ('whitelist_ver','1')")
        cur = c.execute("SELECT COUNT(*) AS n FROM beacons")
        if cur.fetchone()["n"] == 0:
            demo = [
                ("Joao", "CRC001", "C3:FA:7B:12:5E:01", 1),
                ("Maria", "CRC002", "9A:12:88:04:AA:02", 1),
                ("Jose", "CRC003", "1B:55:20:F0:31:03", 1),
            ]
            c.executemany(
                "INSERT INTO beacons(operator, badge, mac, active) VALUES (?,?,?,?)", demo)


# ---- meta / whitelist_ver ----

def get_whitelist_ver():
    with _conn() as c:
        return int(c.execute("SELECT value FROM meta WHERE key='whitelist_ver'")
                   .fetchone()["value"])


def bump_whitelist_ver():
    with _conn() as c:
        v = int(c.execute("SELECT value FROM meta WHERE key='whitelist_ver'")
                .fetchone()["value"]) + 1
        c.execute("UPDATE meta SET value=? WHERE key='whitelist_ver'", (str(v),))
        return v


# ---- beacons ----

def list_beacons():
    with _conn() as c:
        return [dict(r) for r in c.execute(
            "SELECT id, operator, badge, mac, active FROM beacons ORDER BY id")]


def add_beacon(operator, badge, mac):
    with _conn() as c:
        cur = c.execute(
            "INSERT INTO beacons(operator, badge, mac, active) VALUES (?,?,?,1)",
            (operator, badge, mac.upper()))
        return cur.lastrowid


def update_beacon(bid, operator=None, badge=None, mac=None, active=None):
    fields, vals = [], []
    for k, v in (("operator", operator), ("badge", badge), ("mac", mac),
                 ("active", active)):
        if v is not None:
            fields.append(f"{k}=?")
            vals.append(v.upper() if k == "mac" else v)
    if not fields:
        return
    vals.append(bid)
    with _conn() as c:
        c.execute(f"UPDATE beacons SET {','.join(fields)} WHERE id=?", vals)


def delete_beacon(bid):
    with _conn() as c:
        c.execute("DELETE FROM beacons WHERE id=?", (bid,))


def active_macs():
    with _conn() as c:
        return [r["mac"] for r in c.execute(
            "SELECT mac FROM beacons WHERE active=1")]


def beacon_by_mac():
    with _conn() as c:
        return {r["mac"]: dict(r) for r in c.execute(
            "SELECT id, operator, badge, mac, active FROM beacons")}


# ---- atoms ----

def set_atom_seen(atom_id, ts=None):
    with _conn() as c:
        c.execute("UPDATE atoms SET last_seen=? WHERE id=?",
                  (ts if ts is not None else time.time(), atom_id))


def get_atoms():
    with _conn() as c:
        return [dict(r) for r in c.execute(
            "SELECT id, zone, last_seen FROM atoms ORDER BY zone")]


# ---- readings ----

def persist_reading(atom_id, mac, rssi, n, ts=None):
    with _conn() as c:
        c.execute(
            "INSERT INTO readings(server_ts, atom_id, mac, rssi, n) VALUES (?,?,?,?,?)",
            (ts if ts is not None else time.time(), atom_id, mac.upper(), rssi, n))
