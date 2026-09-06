"""Configuration normalization for the PhotoPainter HA entry."""

from __future__ import annotations

import re
from typing import Any
from zoneinfo import ZoneInfo, ZoneInfoNotFoundError

from .const import (
    DEFAULT_DAY_END,
    DEFAULT_DAY_INTERVAL,
    DEFAULT_DAY_START,
    DEFAULT_NIGHT_INTERVAL,
    ROOM_LABELS,
    ROOMS,
)

DEVICE_ID_RE = re.compile(r"^[a-z0-9][a-z0-9-]{0,63}$")


def validate_device_id(value: Any) -> str:
    if not isinstance(value, str) or not DEVICE_ID_RE.fullmatch(value):
        raise ValueError("device_id must use lower-case letters, digits, and hyphens")
    return value


HHMM_RE = re.compile(r"^(\d{1,2}):([0-5]\d)$")
MINUTES_PER_DAY = 24 * 60
MAX_HHMM_HOUR = 47


def parse_hhmm(value: Any, *, name: str) -> int:
    """Return minutes from local midnight.

    Hours may run past 24 so a window that crosses midnight can be written
    the way broadcast schedules are: ``26:00`` is 02:00 on the following day.
    """

    if not isinstance(value, str):
        raise ValueError(f"{name} must be HH:MM")
    match = HHMM_RE.match(value.strip())
    if match is None:
        raise ValueError(f"{name} must be HH:MM")
    hour = int(match.group(1))
    minute = int(match.group(2))
    if hour > MAX_HHMM_HOUR:
        raise ValueError(f"{name} must be between 00:00 and {MAX_HHMM_HOUR}:59")
    return hour * 60 + minute


def day_window_minutes(day_start: Any, day_end: Any) -> tuple[int, int]:
    """Return the day window as minutes from midnight, end always after start.

    ``08:00``-``02:00`` and ``08:00``-``26:00`` describe the same window; the
    end is pushed to the next day when it is not already past the start.
    """

    start = parse_hhmm(day_start, name="day_start")
    end = parse_hhmm(day_end, name="day_end")
    if end <= start:
        end += MINUTES_PER_DAY
    if end - start > MINUTES_PER_DAY:
        raise ValueError("the day window must not exceed 24 hours")
    return start, end


def validate_intervals(day: Any, night: Any) -> tuple[int, int]:
    try:
        day_value = int(day)
        night_value = int(night)
    except (TypeError, ValueError):
        raise ValueError("update intervals must be integers") from None
    if not 5 <= day_value <= 120:
        raise ValueError("day interval must be between 5 and 120 minutes")
    if not 30 <= night_value <= 360:
        raise ValueError("night interval must be between 30 and 360 minutes")
    return day_value, night_value


def normalize_entry_data(data: dict[str, Any]) -> dict[str, Any]:
    """Apply defaults and validate user supplied entity bindings."""

    result = dict(data)
    result["device_id"] = validate_device_id(result.get("device_id"))
    result.setdefault("timezone", "Asia/Tokyo")
    if not isinstance(result["timezone"], str):
        raise ValueError("timezone must be an IANA name")
    try:
        ZoneInfo(result["timezone"])
    except (ZoneInfoNotFoundError, ValueError):
        raise ValueError("timezone must be a valid IANA name") from None
    result.setdefault("template_id", "home_duo")
    result.setdefault("calendar_1", None)
    result.setdefault("calendar_2", None)
    result.setdefault("weather_entity", None)
    for room in ROOMS:
        result.setdefault(f"{room}_temperature", None)
        result.setdefault(f"{room}_humidity", None)
    day, night = validate_intervals(
        result.get("day_interval_minutes", DEFAULT_DAY_INTERVAL),
        result.get("night_interval_minutes", DEFAULT_NIGHT_INTERVAL),
    )
    result["day_interval_minutes"] = day
    result["night_interval_minutes"] = night
    result.setdefault("day_start", DEFAULT_DAY_START)
    result.setdefault("day_end", DEFAULT_DAY_END)
    day_window_minutes(result["day_start"], result["day_end"])
    if not isinstance(result.get("device_key_hash"), str) or not re.fullmatch(r"[0-9a-f]{64}", result["device_key_hash"]):
        raise ValueError("device key hash is missing")
    return result


def config_for_display(data: dict[str, Any]) -> dict[str, Any]:
    """Return non-secret configuration for diagnostics and entity metadata."""

    return {
        "device_id": data.get("device_id"),
        "template_id": data.get("template_id", "home_duo"),
        "timezone": data.get("timezone", "Asia/Tokyo"),
        "calendars": {
            "calendar_1": data.get("calendar_1"),
            "calendar_2": data.get("calendar_2"),
        },
        "rooms": {
            room: {
                "display_name": ROOM_LABELS[room],
                "temperature": data.get(f"{room}_temperature"),
                "humidity": data.get(f"{room}_humidity"),
            }
            for room in ROOMS
        },
        "weather_entity": data.get("weather_entity"),
        "day_interval_minutes": data.get("day_interval_minutes", DEFAULT_DAY_INTERVAL),
        "night_interval_minutes": data.get("night_interval_minutes", DEFAULT_NIGHT_INTERVAL),
        "day_start": data.get("day_start", DEFAULT_DAY_START),
        "day_end": data.get("day_end", DEFAULT_DAY_END),
    }
