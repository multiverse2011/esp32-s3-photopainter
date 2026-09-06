"""Snapshot and wire-contract helpers.

The functions in this module are deliberately deterministic and free of HA
types.  The adapter supplies service responses and state dictionaries, while
this layer enforces the feature specification's date, freshness, and input
validation rules.
"""

from __future__ import annotations

import hashlib
import math
from datetime import date, datetime, time, timedelta, timezone
from typing import Any, Iterable
from zoneinfo import ZoneInfo, ZoneInfoNotFoundError

from .const import (
    ALLOWED_NIBBLES,
    CALENDAR_OWNERS,
    FRAME_BYTES,
    HEIGHT,
    MAX_CALENDAR_ITEMS,
    MAX_FORECAST_ITEMS,
    PALETTE_ID,
    WIDTH,
)
from .models import (
    CalendarEvent,
    CalendarSection,
    ForecastSection,
    ForecastSlot,
    RoomReading,
    SensorReading,
)


def parse_timezone(value: str) -> ZoneInfo:
    """Return a configured IANA timezone, rejecting silently invalid values."""

    try:
        return ZoneInfo(value)
    except (ZoneInfoNotFoundError, ValueError) as err:
        raise ValueError(f"unknown timezone: {value}") from err


def parse_datetime(value: Any, *, require_timezone: bool = True) -> datetime | None:
    """Parse HA ISO values without inventing a timezone."""

    if isinstance(value, datetime):
        parsed = value
    elif isinstance(value, str):
        text = value.strip()
        if not text:
            return None
        if text.endswith("Z"):
            text = text[:-1] + "+00:00"
        try:
            parsed = datetime.fromisoformat(text)
        except ValueError:
            return None
    else:
        return None
    if parsed.tzinfo is None:
        if require_timezone:
            return None
        return parsed
    return parsed


def _as_utc(value: datetime) -> datetime:
    if value.tzinfo is None:
        raise ValueError("timezone required")
    return value.astimezone(timezone.utc)


def _calendar_endpoint(value: Any, *, tz: ZoneInfo) -> tuple[datetime | date, bool] | None:
    """Parse HA calendar API ``{date/dateTime: ...}`` endpoint."""

    if isinstance(value, dict):
        if "date" in value and isinstance(value["date"], str):
            try:
                return date.fromisoformat(value["date"]), True
            except ValueError:
                return None
        value = value.get("dateTime")
    elif isinstance(value, str) and len(value) == 10:
        # Calendar ``get_events`` serializes all-day endpoints as plain ISO
        # dates (the REST API uses {"date": ...} before serialization).
        try:
            return date.fromisoformat(value), True
        except ValueError:
            pass
    parsed = parse_datetime(value)
    if parsed is None:
        return None
    return parsed.astimezone(tz), False


def _calendar_event(raw: Any, *, tz: ZoneInfo, now: datetime) -> CalendarEvent | None:
    if not isinstance(raw, dict):
        return None
    start_result = _calendar_endpoint(raw.get("start"), tz=tz)
    end_result = _calendar_endpoint(raw.get("end"), tz=tz)
    if start_result is None or end_result is None:
        return None
    start_value, start_all_day = start_result
    end_value, end_all_day = end_result
    if start_all_day != end_all_day or type(start_value) is not type(end_value):
        return None
    all_day = start_all_day
    if all_day:
        local_start = datetime.combine(start_value, time.min, tzinfo=tz)
        local_end = datetime.combine(end_value, time.min, tzinfo=tz)
    else:
        local_start = start_value
        local_end = end_value
    if local_end <= local_start:
        return None
    title = raw.get("summary", raw.get("title", ""))
    if not isinstance(title, str):
        return None
    return CalendarEvent(
        title=title,
        start=local_start,
        end=local_end,
        all_day=all_day,
        ongoing=(not all_day and local_start <= now < local_end),
        source_event_id=(raw.get("uid") or raw.get("id"))
        if isinstance(raw.get("uid", raw.get("id")), str)
        else None,
    )


def normalize_calendar_events(
    raw_events: Iterable[Any] | None,
    *,
    owner_id: str,
    entity_id: str | None,
    now: datetime,
    timezone_name: str,
    collected_at: datetime | None,
    complete: bool = True,
    fetch_failed: bool = False,
) -> CalendarSection:
    """Filter, sort, and cap one owner's calendar events.

    End dates are exclusive.  A failed query is represented as ``unavailable``
    even when the response happened to contain an empty list; a successful
    empty query becomes ``No events today`` in the renderer.
    """

    tz = parse_timezone(timezone_name)
    local_now = now.astimezone(tz)
    display_date = local_now.date()
    owner = next((item for item in CALENDAR_OWNERS if item[0] == owner_id), None)
    if owner is None:
        raise ValueError(f"unknown owner: {owner_id}")
    _, display_name, icon_id = owner
    if entity_id is None:
        return CalendarSection(
            owner_id,
            display_name,
            icon_id,
            None,
            "unconfigured",
            display_date,
            collected_at,
            error_reason="unconfigured",
        )
    if fetch_failed or raw_events is None:
        return CalendarSection(
            owner_id,
            display_name,
            icon_id,
            entity_id,
            "unavailable",
            display_date,
            collected_at,
            error_reason="request_failed",
        )

    day_start = datetime.combine(display_date, time.min, tzinfo=tz)
    day_end = day_start + timedelta(days=1)
    parsed: list[CalendarEvent] = []
    invalid_count = 0
    seen: set[tuple[str, str, str]] = set()
    raw_list = list(raw_events)
    limit_hit = len(raw_list) > MAX_CALENDAR_ITEMS
    for raw in raw_list[:MAX_CALENDAR_ITEMS]:
        event = _calendar_event(raw, tz=tz, now=local_now)
        if event is None:
            invalid_count += 1
            continue
        # End dates for all-day events are exclusive, and timed events need an
        # actual overlap with today's local range.
        if event.end <= day_start or event.start >= day_end:
            continue
        if not event.all_day and event.end <= local_now:
            continue
        identity = (
            event.source_event_id or "",
            event.start.isoformat(),
            event.title,
        )
        if identity in seen:
            continue
        seen.add(identity)
        event.ongoing = not event.all_day and event.start <= local_now < event.end
        parsed.append(event)

    parsed.sort(
        key=lambda event: (
            0 if event.all_day else 1 if event.ongoing else 2,
            event.start,
            event.end,
            event.title,
        )
    )
    visible = parsed[:3]
    remaining: int | None = max(0, len(parsed) - len(visible))
    if limit_hit or not complete or invalid_count:
        remaining = None
    effective_complete = not limit_hit and complete and not invalid_count
    status = "ok"
    error_reason: str | None = "invalid_event" if invalid_count else None
    if not parsed and invalid_count:
        status = "unavailable"
    return CalendarSection(
        owner_id,
        display_name,
        icon_id,
        entity_id,
        status,
        display_date,
        collected_at,
        visible,
        remaining,
        complete=effective_complete,
        error_reason=error_reason,
        cached_events=parsed,
    )


def normalize_sensor_state(
    state: Any,
    *,
    unit: str,
    collected_at: datetime,
    source_updated_at: datetime | None = None,
) -> SensorReading:
    """Normalise a sensor state without turning unknown into zero."""

    state_text = state if isinstance(state, str) else None
    if state_text is None and isinstance(state, (int, float)) and not isinstance(state, bool):
        numeric = float(state)
    elif state_text is not None:
        try:
            numeric = float(state_text)
        except ValueError:
            numeric = math.nan
    else:
        numeric = math.nan
    if not math.isfinite(numeric):
        status = "unavailable" if str(state).lower() in {"unknown", "unavailable", "none"} else "unavailable"
        return SensorReading(None, unit, status, collected_at, source_updated_at, "non_numeric")
    # Avoid retaining -0.0 in serialized/display values.
    if numeric == 0:
        numeric = 0.0
    return SensorReading(numeric, unit, "ok", collected_at, source_updated_at)


def forecast_boundaries(now: datetime, timezone_name: str, count: int = 4) -> list[datetime]:
    """Return local 3-hour boundaries at or after ``now``."""

    tz = parse_timezone(timezone_name)
    local = now.astimezone(tz)
    seconds_since_midnight = (
        local.hour * 3600 + local.minute * 60 + local.second + local.microsecond / 1_000_000
    )
    # Ceiling to the next 00/03/06/... boundary.  A value exactly on a
    # boundary stays there; 13:00 therefore starts at 15:00, not 12:00.
    candidate_seconds = math.ceil(seconds_since_midnight / (3 * 3600)) * (3 * 3600)
    candidate_date = local.date()
    if candidate_seconds >= 24 * 3600:
        candidate_date += timedelta(days=1)
        candidate_seconds %= 24 * 3600
    candidate_hour, remainder = divmod(int(candidate_seconds), 3600)
    candidate_minute, candidate_second = divmod(remainder, 60)
    first = datetime.combine(candidate_date, time(candidate_hour, candidate_minute, candidate_second), tzinfo=tz)
    return [first + timedelta(hours=3 * offset) for offset in range(count)]


def _celsius(value: Any, unit: str | None) -> float | None:
    if isinstance(value, bool):
        return None
    if not isinstance(value, (int, float)):
        try:
            value = float(value)
        except (TypeError, ValueError):
            return None
    result = float(value)
    if not math.isfinite(result):
        return None
    normalized = (unit or "").strip().lower().replace("°", "")
    if normalized in {"c", "celsius"}:
        return result
    if normalized in {"f", "fahrenheit"}:
        return (result - 32.0) * 5.0 / 9.0
    return None


def _forecast_datetime(item: dict[str, Any]) -> datetime | None:
    value = item.get("datetime", item.get("valid_at"))
    return parse_datetime(value, require_timezone=True)


def normalize_forecast(
    response: Any,
    *,
    entity_id: str | None,
    now: datetime,
    timezone_name: str,
    collected_at: datetime,
    source_temperature_unit: str | None,
    fetch_failed: bool = False,
    cached_slots: list[ForecastSlot] | None = None,
    cache_collected_at: datetime | None = None,
) -> ForecastSection:
    """Select exact hourly values for the four upcoming 3-hour boundaries."""

    tz = parse_timezone(timezone_name)
    boundaries = forecast_boundaries(now, timezone_name)
    if entity_id is None:
        return ForecastSection(None, "unconfigured", "unconfigured", collected_at, None, source_temperature_unit, _empty_slots(boundaries, tz, local_date=now.astimezone(tz).date()))

    if fetch_failed:
        if cached_slots and cache_collected_at and now - cache_collected_at < timedelta(hours=6):
            cached_by_time = {
                slot.valid_at.astimezone(timezone.utc): slot for slot in cached_slots
            }
            stale_slots = [
                _rebase_cached_slot(cached_by_time.get(_as_utc(boundary)), boundary, now.astimezone(tz).date())
                for boundary in boundaries
            ]
            return ForecastSection(
                entity_id,
                "stale",
                "request_failed",
                cache_collected_at,
                None,
                source_temperature_unit,
                stale_slots,
            )
        return ForecastSection(entity_id, "unavailable", "request_failed", collected_at, None, source_temperature_unit, _empty_slots(boundaries, tz, local_date=now.astimezone(tz).date()))

    items: list[dict[str, Any]] = []
    source_issued_at: datetime | None = None
    if isinstance(response, dict):
        # weather.get_forecasts response is keyed by entity_id.
        response = response.get(entity_id, response)
        if isinstance(response, dict):
            source_issued_at = parse_datetime(response.get("issued_at"))
            response = response.get("forecast", [])
    if isinstance(response, list):
        items = [item for item in response[:MAX_FORECAST_ITEMS] if isinstance(item, dict)]
    by_time: dict[datetime, list[dict[str, Any]]] = {}
    for item in items:
        valid_at = _forecast_datetime(item)
        if valid_at is not None:
            by_time.setdefault(_as_utc(valid_at), []).append(item)
    slots: list[ForecastSlot] = []
    for boundary in boundaries:
        target = _as_utc(boundary)
        matches = by_time.get(target, [])
        item: dict[str, Any] | None = matches[0] if len(matches) == 1 else None
        if len(matches) > 1:
            # Different entries for the same instant must not be chosen
            # arbitrarily.  Keep a field only when every duplicate agrees.
            item = {
                "condition": _same_value(matches, "condition"),
                "temperature": _same_value(matches, "temperature"),
            }
        condition = item.get("condition") if item else None
        if not isinstance(condition, str) or not condition.strip():
            condition = None
        else:
            condition = condition.strip().lower()
        raw_temperature = item.get("temperature") if item else None
        temp_c = _celsius(raw_temperature, source_temperature_unit) if item else None
        status = "ok" if condition is not None or temp_c is not None else "unavailable"
        slots.append(
            ForecastSlot(
                valid_at=boundary.astimezone(timezone.utc),
                local_date=boundary.date(),
                local_time=boundary.strftime("%H:%M"),
                day_offset=(boundary.date() - now.astimezone(tz).date()).days,
                condition=condition,
                icon_key=condition,
                temperature_c=temp_c,
                raw_temperature=(
                    float(raw_temperature)
                    if isinstance(raw_temperature, (int, float))
                    and not isinstance(raw_temperature, bool)
                    and math.isfinite(float(raw_temperature))
                    else None
                ),
                status=status,
            )
        )
    overall = "ok" if any(slot.status == "ok" for slot in slots) else "unavailable"
    reason = None if overall == "ok" else "no_valid_slots"
    return ForecastSection(entity_id, overall, reason, collected_at, source_issued_at, source_temperature_unit, slots)


def _same_value(items: list[dict[str, Any]], key: str) -> Any:
    values = [item.get(key) for item in items]
    first = values[0] if values else None
    return first if all(value == first for value in values) else None


def _empty_slots(boundaries: list[datetime], tz: ZoneInfo, *, local_date: date) -> list[ForecastSlot]:
    return [
        ForecastSlot(
            valid_at=boundary.astimezone(timezone.utc),
            local_date=boundary.date(),
            local_time=boundary.strftime("%H:%M"),
            day_offset=(boundary.date() - local_date).days,
            condition=None,
            icon_key=None,
            temperature_c=None,
            raw_temperature=None,
            status="unavailable",
        )
        for boundary in boundaries
    ]


def _rebase_cached_slot(
    cached: ForecastSlot | None,
    boundary: datetime,
    local_date: date,
) -> ForecastSlot:
    """Use cached values only for the same valid instant.

    The slot label is always rebuilt from the current frame's target; cached
    values are never moved to a new forecast time.
    """

    if cached is None:
        return ForecastSlot(
            valid_at=boundary.astimezone(timezone.utc),
            local_date=boundary.date(),
            local_time=boundary.strftime("%H:%M"),
            day_offset=(boundary.date() - local_date).days,
            condition=None,
            icon_key=None,
            temperature_c=None,
            raw_temperature=None,
            status="unavailable",
        )
    return ForecastSlot(
        valid_at=boundary.astimezone(timezone.utc),
        local_date=boundary.date(),
        local_time=boundary.strftime("%H:%M"),
        day_offset=(boundary.date() - local_date).days,
        condition=cached.condition,
        icon_key=cached.icon_key,
        temperature_c=cached.temperature_c,
        raw_temperature=cached.raw_temperature,
        status=cached.status,
    )


def frame_sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def validate_packed4(
    data: bytes,
    *,
    expected_sha256: str | None = None,
    width: int = WIDTH,
    height: int = HEIGHT,
    palette_id: str = PALETTE_ID,
) -> None:
    """Validate every invariant before a frame can reach the panel."""

    if width != WIDTH or height != HEIGHT or len(data) != FRAME_BYTES:
        raise ValueError("invalid frame dimensions or length")
    if palette_id != PALETTE_ID:
        raise ValueError("unsupported palette")
    if expected_sha256 is not None and frame_sha256(data) != expected_sha256:
        raise ValueError("frame sha256 mismatch")
    for byte in data:
        if (byte >> 4) not in ALLOWED_NIBBLES or (byte & 0x0F) not in ALLOWED_NIBBLES:
            raise ValueError("unsupported palette nibble")
