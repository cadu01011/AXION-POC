"""Zone algorithm tests (plan tests 4.1 and 4.2).

Runs with pytest OR directly: `python server/tests/test_zones.py` (uses
unittest, no pip install needed).
"""
import os
import sys
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "app"))
from zones import ZoneEngine  # noqa: E402


def make_engine():
    return ZoneEngine(hysteresis_db=5.0, hysteresis_hold_s=3.0,
                      presence_timeout_s=15.0, presence_rssi_min=-88.0)


class TestZone(unittest.TestCase):

    def test_argmax_strongest_zone(self):
        e = make_engine()
        e.add_reading("m1", "atom-a", "A", -60, 0)
        e.add_reading("m1", "atom-b", "B", -75, 0)
        e.add_reading("m1", "atom-c", "C", -80, 0)
        e.recompute(0)
        self.assertEqual(e.zone_of("m1"), "A")

    def test_fast_entry(self):
        e = make_engine()
        e.add_reading("m1", "atom-b", "B", -70, 0)
        e.recompute(0)
        self.assertEqual(e.zone_of("m1"), "B")

    def test_no_double_count(self):
        e = make_engine()
        e.add_reading("m1", "atom-a", "A", -65, 0)
        e.add_reading("m1", "atom-b", "B", -66, 0)
        e.recompute(0)
        self.assertEqual(sum(e.counts().values()), 1)

    def test_hysteresis_prevents_flap(self):
        e = make_engine()
        e.add_reading("m1", "atom-a", "A", -70, 0)
        e.recompute(0)
        for t in range(1, 11):
            e.add_reading("m1", "atom-a", "A", -70, t)
            e.add_reading("m1", "atom-b", "B", -67, t)
            e.recompute(t)
        self.assertEqual(e.zone_of("m1"), "A")

    def test_hysteresis_switches_with_margin_and_time(self):
        e = make_engine()
        e.add_reading("m1", "atom-a", "A", -70, 0)
        e.recompute(0)
        for t in range(1, 6):
            e.add_reading("m1", "atom-a", "A", -75, t)
            e.add_reading("m1", "atom-b", "B", -60, t)
            e.recompute(t)
        self.assertEqual(e.zone_of("m1"), "B")

    def test_switch_does_not_happen_before_hold(self):
        e = make_engine()
        e.add_reading("m1", "atom-a", "A", -70, 0)
        e.recompute(0)
        e.add_reading("m1", "atom-a", "A", -75, 1)
        e.add_reading("m1", "atom-b", "B", -60, 1)
        e.recompute(1)
        self.assertEqual(e.zone_of("m1"), "A")

    def test_timeout_removes(self):
        e = make_engine()
        e.add_reading("m1", "atom-a", "A", -60, 0)
        e.recompute(0)
        self.assertEqual(e.zone_of("m1"), "A")
        e.recompute(20)
        self.assertIsNone(e.zone_of("m1"))

    def test_presence_floor(self):
        e = make_engine()
        e.add_reading("m1", "atom-a", "A", -95, 0)
        e.add_reading("m1", "atom-b", "B", -98, 0)
        e.recompute(0)
        self.assertIsNone(e.zone_of("m1"))

    def test_multi_beacon_distinct_zones(self):
        e = make_engine()
        e.add_reading("m1", "atom-a", "A", -55, 0)
        e.add_reading("m1", "atom-b", "B", -80, 0)
        e.add_reading("m2", "atom-b", "B", -58, 0)
        e.add_reading("m2", "atom-a", "A", -78, 0)
        e.add_reading("m3", "atom-c", "C", -60, 0)
        e.recompute(0)
        self.assertEqual(e.zone_of("m1"), "A")
        self.assertEqual(e.zone_of("m2"), "B")
        self.assertEqual(e.zone_of("m3"), "C")
        self.assertEqual(e.counts(), {"A": 1, "B": 1, "C": 1})

    def test_disabled_beacon_drops(self):
        e = make_engine()
        e.add_reading("m1", "atom-a", "A", -60, 0)
        e.add_reading("m2", "atom-a", "A", -61, 0)
        e.recompute(0, active_macs={"m1", "m2"})
        self.assertEqual(sum(e.counts().values()), 2)
        e.recompute(0, active_macs={"m1"})
        self.assertEqual(sum(e.counts().values()), 1)
        self.assertIsNone(e.zone_of("m2"))


if __name__ == "__main__":
    unittest.main(verbosity=2)
