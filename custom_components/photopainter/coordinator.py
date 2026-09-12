"""HA adapter that collects entities and renders one immutable frame."""

from __future__ import annotations

import asyncio
import dataclasses
import logging
from datetime import date, datetime, time, timedelta, timezone
from functools import partial
from typing import Any

from homeassistant.const import EVENT_HOMEASSISTANT_STARTED
from homeassistant.core import CoreState, Event, HomeAssistant, callback
from homeassistant.helpers.event import async_track_point_in_time, async_track_state_change_event
from homeassistant.helpers.update_coordinator import DataUpdateCoordinator, UpdateFailed

from .config import day_window_minutes
from .const import (
    CALENDAR_OWNERS,
    DEFAULT_DAY_END,
    DEFAULT_DAY_INTERVAL,
    DEFAULT_DAY_START,
    DEFAULT_NIGHT_INTERVAL,
    DOMAIN,
    ROOM_LABELS,
    ROOMS,
)
from .core import normalize_calendar_events, normalize_forecast, normalize_sensor_state, parse_timezone
from .models import CalendarSection, DisplaySnapshot, ForecastSection, RoomReading, SensorReading
from .renderer import render_snapshot

_LOGGER = logging.getLogger(__name__)

# States that mean "this entity has nothing to report yet".
_MISSING_STATES = {"unavailable", "unknown"}


def next_poll_at(
    now: datetime,
    *,
    timezone_name: str,
    day_interval_minutes: int = DEFAULT_DAY_INTERVAL,
    night_interval_minutes: int = DEFAULT_NIGHT_INTERVAL,
    day_start: str = DEFAULT_DAY_START,
    day_end: str = DEFAULT_DAY_END,
) -> datetime:
    """Return the next planned wake time, respecting day/night boundaries."""

    tz = parse_timezone(timezone_name)
    local = now.astimezone(tz)
    start_minutes, end_minutes = day_window_minutes(day_start, day_end)
    day_interval = max(5, int(day_interval_minutes))
    night_interval = max(30, int(night_interval_minutes))
    candidates: list[datetime] = []
    # Wake times form a local-time series anchored at the configured day and
    # night boundaries.  This keeps 13:07 on a 13:30 cadence and includes the
    # 00:00 wake instead of drifting to 01:59.
    for offset in range(-2, 4):
        day = local.date() + timedelta(days=offset)
        midnight = datetime.combine(day, time(0, 0), tzinfo=tz)
        day_start_dt = midnight + timedelta(minutes=start_minutes)
        day_end_dt = midnight + timedelta(minutes=end_minutes)
        next_day_start = day_start_dt + timedelta(days=1)
        cursor = day_start_dt
        while cursor < day_end_dt:
            candidates.append(cursor)
            cursor += timedelta(minutes=day_interval)
        cursor = day_end_dt
        while cursor < next_day_start:
            candidates.append(cursor)
            cursor += timedelta(minutes=night_interval)
    next_local = min(
        (item for item in candidates if item > local),
        default=local + timedelta(minutes=night_interval),
    )
    return next_local.astimezone(timezone.utc)


def _state_value(state: Any) -> tuple[Any, datetime | None, str | None]:
    if state is None:
        return "unavailable", None, None
    value = getattr(state, "state", state)
    attributes = getattr(state, "attributes", {}) or {}
    last_updated = getattr(state, "last_updated", None)
    if isinstance(last_updated, datetime) and last_updated.tzinfo is None:
        last_updated = last_updated.replace(tzinfo=timezone.utc)
    return value, last_updated, attributes.get("unit_of_measurement")


def _cached_room_reading(reading: SensorReading | None, *, now: datetime, default_unit: str) -> SensorReading:
    if reading is None or reading.value is None or reading.collected_at is None:
        return SensorReading(None, default_unit, "unavailable", now, error_reason="request_failed")
    if now - reading.collected_at >= timedelta(hours=6):
        return SensorReading(None, reading.unit, "unavailable", reading.collected_at, reading.source_updated_at, "cache_expired")
    return dataclasses.replace(reading, status="stale")


def _cached_calendar(section: CalendarSection | None, *, now: datetime, timezone_name: str) -> CalendarSection | None:
    if section is None or section.display_date != now.astimezone(parse_timezone(timezone_name)).date():
        return None
    return dataclasses.replace(section, status="stale", error_reason="request_failed")


class PhotoPainterCoordinator(DataUpdateCoordinator[DisplaySnapshot]):
    """Collect configured HA entities and keep the current frame in sync."""

    def __init__(self, hass: HomeAssistant, entry: Any, runtime: Any) -> None:
        self.entry = entry
        self.runtime = runtime
        self.config = dict(entry.data)
        self._previous_calendars: dict[str, CalendarSection] = {}
        self._calendar_raw: dict[str, list[dict[str, Any]]] = {}
        self._calendar_collected: dict[str, datetime] = {}
        self._calendar_truncated: dict[str, bool] = {}
        self._previous_rooms: dict[str, RoomReading] = {}
        self._previous_forecast: ForecastSection | None = None
        self._unsub_state = None
        self._unsub_schedule = None
        self._unsub_started = None
        self._dirty = True
        self._scheduled_target: datetime | None = None
        self._awaiting_entities: set[str] = set()
        super().__init__(
            hass,
            _LOGGER,
            config_entry=entry,
            name=f"{DOMAIN}_{self.config.get('device_id', entry.entry_id)}",
            update_interval=None,
            update_method=self._async_update_data,
            always_update=True,
        )
        runtime.coordinator = self

    async def async_start(self) -> None:
        entity_ids = self._bound_entity_ids()
        if entity_ids:
            self._unsub_state = async_track_state_change_event(
                self.hass,
                entity_ids,
                self._async_state_changed,
            )
        if self.hass.state is not CoreState.running:
            # Entities are still coming up, so the frame rendered now is full of
            # placeholders. Render again once HA is up.
            self._unsub_started = self.hass.bus.async_listen_once(
                EVENT_HOMEASSISTANT_STARTED, self._async_hass_started
            )
        await self.async_config_entry_first_refresh()

    async def _async_hass_started(self, _event: Event) -> None:
        self._unsub_started = None
        self._dirty = True
        await self.async_request_refresh()

    async def async_stop(self) -> None:
        if self._unsub_started:
            self._unsub_started()
            self._unsub_started = None
        if self._unsub_state:
            self._unsub_state()
            self._unsub_state = None
        if self._unsub_schedule:
            self._unsub_schedule()
            self._unsub_schedule = None

    async def async_reconfigure(self, data: dict[str, Any]) -> None:
        old_config = self.config
        self.config = dict(data)
        for owner_id, _name, _icon in CALENDAR_OWNERS:
            if old_config.get(owner_id) != self.config.get(owner_id):
                self._calendar_raw.pop(owner_id, None)
                self._calendar_collected.pop(owner_id, None)
                self._calendar_truncated.pop(owner_id, None)
                self._previous_calendars.pop(owner_id, None)
        if old_config.get("weather_entity") != self.config.get("weather_entity"):
            self._previous_forecast = None
        for room in ROOMS:
            if any(old_config.get(f"{room}_{kind}") != self.config.get(f"{room}_{kind}") for kind in ("temperature", "humidity")):
                self._previous_rooms.pop(room, None)
        if old_config.get("timezone") != self.config.get("timezone"):
            self._calendar_raw.clear()
            self._calendar_collected.clear()
            self._calendar_truncated.clear()
            self._previous_calendars.clear()
            self._previous_forecast = None
        if self._unsub_state:
            self._unsub_state()
            self._unsub_state = None
        entity_ids = self._bound_entity_ids()
        if entity_ids:
            self._unsub_state = async_track_state_change_event(self.hass, entity_ids, self._async_state_changed)
        self.runtime.device_key_hash = self.config.get("device_key_hash", self.runtime.device_key_hash)
        await self.async_request_refresh()

    def _bound_entity_ids(self) -> list[str]:
        values = [self.config.get("calendar_1"), self.config.get("calendar_2"), self.config.get("weather_entity")]
        values.extend(self.config.get(f"{room}_temperature") for room in ROOMS)
        values.extend(self.config.get(f"{room}_humidity") for room in ROOMS)
        return [value for value in values if isinstance(value, str) and value]

    async def _async_state_changed(self, event: Event[Any]) -> None:
        # State events only mark the image dirty.  The scheduled wake performs
        # the collection/render so a burst of HA events cannot redraw a panel.
        self._dirty = True
        # The exception is an entity the current frame is drawing a placeholder
        # for: after a restart the battery sensors report one by one, and the
        # panel should not wait for the next wake to show a house it can
        # already read. Each entity triggers this at most once per render, and
        # the coordinator's debouncer collapses a burst of them.
        entity_id = event.data.get("entity_id")
        if entity_id not in self._awaiting_entities:
            return
        new_state = event.data.get("new_state")
        if new_state is None or new_state.state in _MISSING_STATES:
            return
        self._awaiting_entities.discard(entity_id)
        await self.async_request_refresh()

    @callback
    def _scheduled_refresh(self, _when: datetime) -> None:
        self._dirty = True
        self._scheduled_target = _when
        self.hass.async_create_task(self.async_request_refresh())

    async def _async_calendar(self, entity_id: str | None, now: datetime) -> list[dict[str, Any]] | None:
        if not entity_id:
            return []
        tz = parse_timezone(self.config["timezone"])
        local = now.astimezone(tz)
        start = datetime.combine(local.date(), time.min, tzinfo=tz)
        # Two days, so tomorrow can backfill the slots today leaves empty.
        end = start + timedelta(days=2)
        response = await asyncio.wait_for(
            self.hass.services.async_call(
                "calendar",
                "get_events",
                {
                    "entity_id": entity_id,
                    "start_date_time": start.isoformat(),
                    "end_date_time": end.isoformat(),
                },
                blocking=True,
                return_response=True,
            ),
            timeout=10,
        )
        if isinstance(response, dict):
            item = response.get(entity_id, response)
            if isinstance(item, dict) and isinstance(item.get("events"), list):
                return item["events"]
        return None

    async def _async_forecast(self, entity_id: str | None) -> tuple[Any, str | None]:
        if not entity_id:
            return None, None
        state = self.hass.states.get(entity_id)
        attributes = getattr(state, "attributes", {}) if state is not None else {}
        unit = attributes.get("temperature_unit") if isinstance(attributes, dict) else None
        response = await asyncio.wait_for(
            self.hass.services.async_call(
                "weather",
                "get_forecasts",
                {"entity_id": entity_id, "type": "hourly"},
                blocking=True,
                return_response=True,
            ),
            timeout=10,
        )
        return response, unit

    async def _async_update_data(self) -> DisplaySnapshot:
        now = datetime.now(timezone.utc)
        timezone_name = self.config.get("timezone", "Asia/Tokyo")
        errors: list[str] = []
        calendars: list[CalendarSection] = []
        async def collect_calendar(owner_id: str, entity_id: str | None) -> list[dict[str, Any]] | None:
            if not entity_id:
                return []
            try:
                return await self._async_calendar(entity_id, now)
            except Exception:
                # Service exceptions are intentionally not interpolated into
                # logs; calendar providers may include private event data.
                errors.append(f"{owner_id}:request_failed")
                return None

        calendar_results = await asyncio.gather(
            *(collect_calendar(owner_id, self.config.get(owner_id)) for owner_id, _name, _icon in CALENDAR_OWNERS)
        )
        for (owner_id, _display_name, _icon), raw_events in zip(CALENDAR_OWNERS, calendar_results):
            entity_id = self.config.get(owner_id)
            if raw_events is not None:
                if entity_id:
                    self._calendar_raw[owner_id] = raw_events[:200]
                    self._calendar_collected[owner_id] = now
                    self._calendar_truncated[owner_id] = len(raw_events) > 200
                section = normalize_calendar_events(
                    raw_events,
                    owner_id=owner_id,
                    entity_id=entity_id,
                    now=now,
                    timezone_name=timezone_name,
                    collected_at=now,
                    fetch_failed=False,
                )
            else:
                cached_raw = self._calendar_raw.get(owner_id)
                cache_time = self._calendar_collected.get(owner_id)
                same_local_day = bool(
                    cache_time
                    and cache_time.astimezone(parse_timezone(timezone_name)).date()
                    == now.astimezone(parse_timezone(timezone_name)).date()
                )
                if cached_raw is not None and cache_time and same_local_day and now - cache_time < timedelta(hours=6):
                    section = normalize_calendar_events(
                        cached_raw,
                        owner_id=owner_id,
                        entity_id=entity_id,
                        now=now,
                        timezone_name=timezone_name,
                        collected_at=cache_time,
                        complete=not self._calendar_truncated.get(owner_id, False),
                    )
                    section = dataclasses.replace(section, status="stale", error_reason="request_failed")
                else:
                    section = normalize_calendar_events(
                        None,
                        owner_id=owner_id,
                        entity_id=entity_id,
                        now=now,
                        timezone_name=timezone_name,
                        collected_at=cache_time,
                        fetch_failed=True,
                    )
            calendars.append(section)
            self._previous_calendars[owner_id] = section

        rooms: list[RoomReading] = []
        for room in ROOMS:
            readings: list[SensorReading] = []
            for kind, default_unit in (("temperature", "°C"), ("humidity", "%")):
                entity_id = self.config.get(f"{room}_{kind}")
                if not entity_id:
                    readings.append(SensorReading(None, default_unit, "unconfigured", now, error_reason="unconfigured"))
                    continue
                state = self.hass.states.get(entity_id) if entity_id else None
                value, source_updated, unit = _state_value(state)
                reading = normalize_sensor_state(
                    value,
                    unit=unit or default_unit,
                    collected_at=now,
                    source_updated_at=source_updated,
                    temperature=kind == "temperature",
                )
                if state is None:
                    reading = SensorReading(None, unit or default_unit, "unavailable", now, error_reason="missing_entity")
                    errors.append(f"{room}_{kind}:unavailable")
                elif str(value).lower() in {"unknown", "unavailable"}:
                    # Explicit source unavailability must not be made to look
                    # current by retaining a previous numeric value.
                    reading = SensorReading(None, unit or default_unit, "unavailable", now, source_updated, "source_unavailable")
                    errors.append(f"{room}_{kind}:unavailable")
                readings.append(reading)
            room_reading = RoomReading(room, ROOM_LABELS[room], readings[0], readings[1])
            rooms.append(room_reading)
            self._previous_rooms[room] = room_reading

        weather_entity = self.config.get("weather_entity")
        try:
            response, temperature_unit = await self._async_forecast(weather_entity)
            forecast = normalize_forecast(
                response,
                entity_id=weather_entity,
                now=now,
                timezone_name=timezone_name,
                collected_at=now,
                source_temperature_unit=temperature_unit,
                fetch_failed=response is None and weather_entity is not None,
                cached_slots=self._previous_forecast.slots if self._previous_forecast else None,
                cache_collected_at=self._previous_forecast.collected_at if self._previous_forecast else None,
            )
        except Exception:
            _LOGGER.debug("forecast collection failed")
            errors.append("forecast:request_failed")
            forecast = normalize_forecast(
                None,
                entity_id=weather_entity,
                now=now,
                timezone_name=timezone_name,
                collected_at=now,
                source_temperature_unit=self._previous_forecast.source_temperature_unit if self._previous_forecast else None,
                fetch_failed=True,
                cached_slots=self._previous_forecast.slots if self._previous_forecast else None,
                cache_collected_at=self._previous_forecast.collected_at if self._previous_forecast else None,
            )
        self._previous_forecast = forecast

        status = "stale" if any(section.status == "stale" for section in calendars) or any(
            reading.status == "stale" for room in rooms for reading in (room.temperature, room.humidity)
        ) or forecast.status == "stale" else "ok"
        if errors and not any(section.status == "ok" for section in calendars) and not any(
            reading.status == "ok" for room in rooms for reading in (room.temperature, room.humidity)
        ) and forecast.status in {"unavailable", "unconfigured"}:
            status = "offline"
        snapshot = DisplaySnapshot(
            generated_at=now,
            display_timezone=timezone_name,
            display_date=now.astimezone(parse_timezone(timezone_name)).date(),
            calendars=calendars,
            rooms=rooms,
            forecast=forecast,
            status=status,
            error_reason=";".join(errors) if errors else None,
            collected_at=now,
        )
        next_wake = next_poll_at(
            now,
            timezone_name=timezone_name,
            day_interval_minutes=self.config.get("day_interval_minutes", DEFAULT_DAY_INTERVAL),
            night_interval_minutes=self.config.get("night_interval_minutes", DEFAULT_NIGHT_INTERVAL),
            day_start=self.config.get("day_start", DEFAULT_DAY_START),
            day_end=self.config.get("day_end", DEFAULT_DAY_END),
        )
        self.runtime.server_next_wake_at = next_wake
        frame = await self.hass.async_add_executor_job(
            partial(render_snapshot, snapshot, next_poll_at=next_wake)
        )
        # A frame restored from storage describes a working house; one rendered
        # while HA is still starting does not. Keep the stored frame until the
        # entities are actually up, so a restart minutes before the device wakes
        # cannot hand it a screen full of placeholders.
        if self.hass.state is CoreState.running or self.runtime.pending_frame is None:
            await self.runtime.async_set_pending_frame(frame)
        self._dirty = False
        if self._unsub_schedule:
            self._unsub_schedule()
        # Generate one minute before a planned device wake, then schedule the
        # actual wake once that prewake refresh has completed.
        prewake = next_wake - timedelta(minutes=1)
        if self._scheduled_target is not None and self._scheduled_target == prewake:
            schedule_at = next_wake
        else:
            schedule_at = max(now + timedelta(seconds=1), prewake)
        self._scheduled_target = schedule_at
        self._unsub_schedule = async_track_point_in_time(self.hass, self._scheduled_refresh, schedule_at)
        self._awaiting_entities = {
            entity_id
            for entity_id in self._bound_entity_ids()
            if (state := self.hass.states.get(entity_id)) is None or state.state in _MISSING_STATES
        }
        return snapshot
