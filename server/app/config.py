"""Server configuration constants.

Single source of truth for the server-side tunables. Mirrors section 3.1 of
PLANO_IMPLEMENTACAO.md; change a value here first, then in the plan.
"""
import os

# Zone algorithm
HYSTERESIS_DB = 5.0          # a new zone must beat the current one by at least this
HYSTERESIS_HOLD_S = 3.0      # ... continuously for at least this long
PRESENCE_TIMEOUT_S = 15.0    # a beacon with no reading drops out of the count
PRESENCE_RSSI_MIN = -88.0    # weaker than this on every ATOM = absent
ATOM_OFFLINE_S = 10.0        # an ATOM with no POST is shown OFFLINE

# Fixed POC topology
ZONES = ["A", "B", "C"]
ATOMS = {"atom-a": "A", "atom-b": "B", "atom-c": "C"}

# Infrastructure
DB_PATH = os.environ.get("AXION_DB", os.path.join(os.path.dirname(__file__), "..", "axion.db"))
RECOMPUTE_PERIOD_S = 1.0     # timeout / offline sweep timer
