"""API request/response contracts (section 3.3 of the plan)."""
from typing import List, Optional

from pydantic import BaseModel, Field


class BeaconReading(BaseModel):
    mac: str
    rssi: float
    n: int = 1


class ReadingsIn(BaseModel):
    atom_id: str
    zone: str
    seq: int = 0
    uptime_ms: int = 0
    beacons: List[BeaconReading] = Field(default_factory=list)


class ReadingsOut(BaseModel):
    zone_count: int
    whitelist_ver: int


class BeaconIn(BaseModel):
    operator: str
    badge: str
    mac: str


class BeaconPatch(BaseModel):
    operator: Optional[str] = None
    badge: Optional[str] = None
    mac: Optional[str] = None
    active: Optional[bool] = None
