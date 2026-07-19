#!/usr/bin/env python3
"""AXION API mock - plan tests 3.1/3.3.

Receives POST /api/readings, prints each payload and answers
{"zone_count": <number of beacons in the payload>, "whitelist_ver": 1}.

Detects on its own (prints WARNING):
- delivery gap: more than 6 s without a POST from an atom already seen;
- lost POST: a jump in the "seq" field (the firmware increments it per send).

Usage: python tools/mock_api.py [port]   (default 8000)
Standard library only. Ctrl+C prints a summary and exits.
"""
import json
import sys
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

GAP_LIMIT_S = 6.0

state = {}   # atom_id -> {"last": epoch, "seq": int, "count": int, "warns": int}


class Handler(BaseHTTPRequestHandler):
    def do_POST(self):
        if self.path != "/api/readings":
            self.send_error(404)
            return
        length = int(self.headers.get("Content-Length", 0))
        raw = self.rfile.read(length)
        try:
            data = json.loads(raw)
        except json.JSONDecodeError:
            print(f"[{time.strftime('%H:%M:%S')}] invalid payload: {raw[:200]!r}")
            self.send_error(400)
            return

        now = time.time()
        atom = data.get("atom_id", "?")
        seq = data.get("seq")
        beacons = data.get("beacons", [])

        st = state.setdefault(atom, {"last": None, "seq": None, "count": 0, "warns": 0})
        if st["last"] is not None:
            gap = now - st["last"]
            if gap > GAP_LIMIT_S:
                st["warns"] += 1
                print(f"[{time.strftime('%H:%M:%S')}] WARNING: {atom} went {gap:.1f}s without a POST (limit {GAP_LIMIT_S:.0f}s)")
        if st["seq"] is not None and isinstance(seq, int) and seq > st["seq"] + 1:
            st["warns"] += 1
            print(f"[{time.strftime('%H:%M:%S')}] WARNING: {atom} skipped seq {st['seq']} -> {seq} ({seq - st['seq'] - 1} lost POST(s))")
        st["last"] = now
        st["seq"] = seq if isinstance(seq, int) else st["seq"]
        st["count"] += 1

        detail = " ".join(f"{b.get('mac')}={b.get('rssi')}(n={b.get('n')})" for b in beacons)
        print(f"[{time.strftime('%H:%M:%S')}] {atom} zone={data.get('zone')} seq={seq} "
              f"beacons={len(beacons)} {detail}")

        resp = json.dumps({"zone_count": len(beacons), "whitelist_ver": 1}).encode()
        self.send_response(200)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(resp)))
        self.end_headers()
        self.wfile.write(resp)

    def log_message(self, *args):
        pass


if __name__ == "__main__":
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 8000
    print(f"AXION mock API at http://0.0.0.0:{port}/api/readings - Ctrl+C to stop")
    print(f"Point CONFIG_AXION_API_URL to http://<this-machine-ip>:{port}")
    try:
        ThreadingHTTPServer(("0.0.0.0", port), Handler).serve_forever()
    except KeyboardInterrupt:
        print("\n--- SUMMARY ---")
        for atom, st in sorted(state.items()):
            print(f"{atom}: {st['count']} POSTs received, {st['warns']} warning(s)")
        print("Test 3.1 PASSES if: 10 min running and 0 warning(s) per atom.")
