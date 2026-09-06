"""Pure data models for PhotoPainter.

These models intentionally do not import Home Assistant.  This lets the
normalisation, rendering, and protocol tests run on a workstation while the
HA adapter remains a thin asynchronous layer.
"""

from __future__ import annotations

from dataclasses import dataclass, field
from datetime import date, datetime
from typing import Any


@dataclass(slots=True)
class CalendarEvent:
    title: str
    start: datetime
    end: datetime
    all_day: bool = False
    ongoing: bool = False
    source_event_id: str | None = None

    def as_dict(self) -> dict[str, Any]:
        return {
            "title": self.title,
            "start": self.start.isoformat(),
            "end": self.end.isoformat(),
            "all_day": self.all_day,
            "ongoing": self.ongoing,
            "source_event_id": self.source_event_id,
        }


@dataclass(slots=True)
class CalendarSection:
    owner_id: str
    display_name: str
    icon_id: str
    entity_id: str | None
    status: str
    display_date: date
    collected_at: datetime | None = None
    events: list[CalendarEvent] = field(default_factory=list)
    remaining_count: int | None = 0
    complete: bool = True
    error_reason: str | None = None
    # Full normalized events are retained for the next snapshot/cache pass;
    # ``events`` remains the three-row display projection.
    cached_events: list[CalendarEvent] = field(default_factory=list, repr=False)

    def as_dict(self) -> dict[str, Any]:
        return {
            "owner_id": self.owner_id,
            "display_name": self.display_name,
            "icon_id": self.icon_id,
            "entity_id": self.entity_id,
            "status": self.status,
            "display_date": self.display_date.isoformat(),
            "collected_at": self.collected_at.isoformat() if self.collected_at else None,
            "events": [event.as_dict() for event in self.events],
            "remaining_count": self.remaining_count,
            "complete": self.complete,
            "error_reason": self.error_reason,
        }


@dataclass(slots=True)
class SensorReading:
    value: float | None
    unit: str
    status: str
    collected_at: datetime | None = None
    source_updated_at: datetime | None = None
    error_reason: str | None = None

    def as_dict(self) -> dict[str, Any]:
        return {
            "value": self.value,
            "unit": self.unit,
            "status": self.status,
            "collected_at": self.collected_at.isoformat() if self.collected_at else None,
            "source_updated_at": self.source_updated_at.isoformat()
            if self.source_updated_at
            else None,
            "error_reason": self.error_reason,
        }


@dataclass(slots=True)
class RoomReading:
    room_id: str
    display_name: str
    temperature: SensorReading
    humidity: SensorReading

    def as_dict(self) -> dict[str, Any]:
        return {
            "room_id": self.room_id,
            "display_name": self.display_name,
            "temperature": self.temperature.as_dict(),
            "humidity": self.humidity.as_dict(),
        }


@dataclass(slots=True)
class ForecastSlot:
    valid_at: datetime
    local_date: date
    local_time: str
    day_offset: int
    condition: str | None
    icon_key: str | None
    temperature_c: float | None
    raw_temperature: float | None
    status: str

    @property
    def label(self) -> str:
        return f"{self.local_time}{' +1d' if self.day_offset else ''}"

    def as_dict(self) -> dict[str, Any]:
        return {
            "valid_at": self.valid_at.isoformat(),
            "local_date": self.local_date.isoformat(),
            "local_time": self.local_time,
            "day_offset": self.day_offset,
            "condition": self.condition,
            "icon_key": self.icon_key,
            "temperature_c": self.temperature_c,
            "raw_temperature": self.raw_temperature,
            "status": self.status,
        }


@dataclass(slots=True)
class ForecastSection:
    entity_id: str | None
    status: str
    error_reason: str | None
    collected_at: datetime | None
    source_issued_at: datetime | None
    source_temperature_unit: str | None
    slots: list[ForecastSlot] = field(default_factory=list)

    def as_dict(self) -> dict[str, Any]:
        return {
            "entity_id": self.entity_id,
            "status": self.status,
            "error_reason": self.error_reason,
            "collected_at": self.collected_at.isoformat() if self.collected_at else None,
            "source_issued_at": self.source_issued_at.isoformat()
            if self.source_issued_at
            else None,
            "source_temperature_unit": self.source_temperature_unit,
            "slots": [slot.as_dict() for slot in self.slots],
        }


@dataclass(slots=True)
class DisplaySnapshot:
    generated_at: datetime
    display_timezone: str
    display_date: date
    calendars: list[CalendarSection]
    rooms: list[RoomReading]
    forecast: ForecastSection
    status: str = "ok"
    error_reason: str | None = None
    collected_at: datetime | None = None

    def as_dict(self) -> dict[str, Any]:
        return {
            "generated_at": self.generated_at.isoformat(),
            "display_timezone": self.display_timezone,
            "display_date": self.display_date.isoformat(),
            "status": self.status,
            "error_reason": self.error_reason,
            "collected_at": self.collected_at.isoformat() if self.collected_at else None,
            "calendars": [calendar.as_dict() for calendar in self.calendars],
            "rooms": [room.as_dict() for room in self.rooms],
            "forecast": self.forecast.as_dict(),
        }


@dataclass(slots=True)
class RenderedFrame:
    frame_id: str
    data: bytes
    generated_at: datetime
    fresh_until: datetime | None
    palette_id: str
    width: int
    height: int
    status_overlay: dict[str, int]
    snapshot: DisplaySnapshot

    def manifest_fragment(self, path: str) -> dict[str, Any]:
        return {
            "id": self.frame_id,
            "path": path,
            "width": self.width,
            "height": self.height,
            "format": "packed4",
            "palette_id": self.palette_id,
            "byte_length": len(self.data),
            "sha256": self.frame_id,
            "generated_at": self.generated_at.isoformat(),
            "fresh_until": self.fresh_until.isoformat() if self.fresh_until else None,
            "status_overlay": self.status_overlay,
        }
