"""Persistent device/frame state shared by the HA API and entities."""

from __future__ import annotations

import asyncio
import base64
import hashlib
import hmac
import re
import time
from dataclasses import dataclass
from datetime import datetime, timedelta, timezone
from typing import Any

from homeassistant.core import HomeAssistant
from homeassistant.helpers.storage import Store

from .const import DOMAIN, PALETTE_ID, SCHEMA_VERSION, STORAGE_KEY, STORAGE_VERSION
from .core import validate_packed4
from .models import RenderedFrame

FRAME_ID_RE = re.compile(r"^[0-9a-f]{64}$")
REPORT_ID_RE = re.compile(r"^[A-Za-z0-9._:-]{1,128}$")


@dataclass(slots=True)
class DeviceReportResult:
    accepted: bool
    report_id: str
    status: int = 200
    error: str | None = None


def _parse_time(value: Any) -> datetime | None:
    if not isinstance(value, str):
        return None
    try:
        parsed = datetime.fromisoformat(value.replace("Z", "+00:00"))
    except ValueError:
        return None
    if parsed.tzinfo is None:
        return None
    return parsed.astimezone(timezone.utc)


def _frame_json(frame: RenderedFrame | None) -> dict[str, Any] | None:
    if frame is None:
        return None
    return {
        "frame_id": frame.frame_id,
        "data": base64.b64encode(frame.data).decode("ascii"),
        "generated_at": frame.generated_at.isoformat(),
        "fresh_until": frame.fresh_until.isoformat() if frame.fresh_until else None,
        "palette_id": frame.palette_id,
        "width": frame.width,
        "height": frame.height,
    }


def _frame_from_json(value: Any) -> RenderedFrame | None:
    if not isinstance(value, dict):
        return None
    try:
        data = base64.b64decode(value["data"], validate=True)
        frame_id = value["frame_id"]
        generated_at = _parse_time(value["generated_at"])
        fresh_until = _parse_time(value.get("fresh_until"))
        if not isinstance(frame_id, str) or not FRAME_ID_RE.fullmatch(frame_id) or generated_at is None:
            return None
        validate_packed4(data, expected_sha256=frame_id, width=int(value["width"]), height=int(value["height"]), palette_id=value["palette_id"])
    except (KeyError, TypeError, ValueError, OverflowError):
        return None
    return RenderedFrame(frame_id, data, generated_at, fresh_until, value["palette_id"], int(value["width"]), int(value["height"]), {"x": 344, "y": 24, "width": 432, "height": 40}, None)


class PhotoPainterRuntime:
    """Own frame generations and the durable report/ACK state."""

    def __init__(self, hass: HomeAssistant, entry: Any) -> None:
        self.hass = hass
        self.entry = entry
        self.device_id = entry.data["device_id"]
        self.device_key_hash = entry.data["device_key_hash"]
        self.store = Store(hass, STORAGE_VERSION, f"{STORAGE_KEY}.{self.device_id}", private=True, atomic_writes=True)
        self.pending_frame: RenderedFrame | None = None
        self.displayed_frame: RenderedFrame | None = None
        self._frames: dict[str, RenderedFrame] = {}
        self._frame_order: list[str] = []
        self._report_ids: list[str] = []
        self._lock = asyncio.Lock()
        self.coordinator: Any = None
        self.last_seen: datetime | None = None
        self.last_displayed: datetime | None = None
        self.battery_percent: float | None = None
        self.wifi_signal_dbm: int | None = None
        self.next_wake_at: datetime | None = None
        self.server_next_wake_at: datetime | None = None
        self.last_error: str | None = None
        self.local_overlay: str = "none"
        self.overlay_source_time: datetime | None = None
        self.firmware_version: str | None = None
        self.created_at: datetime = getattr(entry, "created_at", None) or datetime.now(timezone.utc)
        self.redisplay_required = False
        self._loaded = False

    async def async_load(self) -> None:
        saved = await self.store.async_load()
        if isinstance(saved, dict):
            self.pending_frame = _frame_from_json(saved.get("pending_frame"))
            self.displayed_frame = _frame_from_json(saved.get("displayed_frame"))
            for frame in (self.pending_frame, self.displayed_frame):
                if frame:
                    self._frames[frame.frame_id] = frame
            recent = saved.get("recent_frames", [])
            for item in recent if isinstance(recent, list) else []:
                frame = _frame_from_json(item)
                if frame:
                    self._frames[frame.frame_id] = frame
                    self._frame_order.append(frame.frame_id)
            self.last_seen = _parse_time(saved.get("last_seen"))
            self.last_displayed = _parse_time(saved.get("last_displayed"))
            self.next_wake_at = _parse_time(saved.get("next_wake_at"))
            self.server_next_wake_at = _parse_time(saved.get("server_next_wake_at"))
            self.overlay_source_time = _parse_time(saved.get("overlay_source_time"))
            self.battery_percent = saved.get("battery_percent") if isinstance(saved.get("battery_percent"), (int, float)) else None
            self.wifi_signal_dbm = saved.get("wifi_signal_dbm") if isinstance(saved.get("wifi_signal_dbm"), int) else None
            self.last_error = saved.get("last_error") if isinstance(saved.get("last_error"), str) else None
            self.local_overlay = saved.get("local_overlay") if saved.get("local_overlay") in {"none", "offline", "time_unknown"} else "none"
            self.firmware_version = saved.get("firmware_version") if isinstance(saved.get("firmware_version"), str) else None
            self.created_at = _parse_time(saved.get("created_at")) or self.created_at
            self.redisplay_required = bool(saved.get("redisplay_required", False))
            self._report_ids = [item for item in saved.get("report_ids", []) if isinstance(item, str)][-64:]
        self._loaded = True

    async def _async_save(self) -> None:
        await self.store.async_save(
            {
                "pending_frame": _frame_json(self.pending_frame),
                "displayed_frame": _frame_json(self.displayed_frame),
                "last_seen": self.last_seen.isoformat() if self.last_seen else None,
                "last_displayed": self.last_displayed.isoformat() if self.last_displayed else None,
                "next_wake_at": self.next_wake_at.isoformat() if self.next_wake_at else None,
                "server_next_wake_at": self.server_next_wake_at.isoformat() if self.server_next_wake_at else None,
                "overlay_source_time": self.overlay_source_time.isoformat() if self.overlay_source_time else None,
                "battery_percent": self.battery_percent,
                "wifi_signal_dbm": self.wifi_signal_dbm,
                "last_error": self.last_error,
                "local_overlay": self.local_overlay,
                "redisplay_required": self.redisplay_required,
                "report_ids": self._report_ids[-64:],
                "recent_frames": [
                    _frame_json(self._frames[frame_id])
                    for frame_id in self._frame_order[-64:]
                    if frame_id in self._frames
                    and frame_id not in {self.pending_frame.frame_id if self.pending_frame else None, self.displayed_frame.frame_id if self.displayed_frame else None}
                ],
                "firmware_version": self.firmware_version,
                "created_at": self.created_at.isoformat(),
            }
        )

    async def async_set_pending_frame(self, frame: RenderedFrame) -> None:
        validate_packed4(frame.data, expected_sha256=frame.frame_id, width=frame.width, height=frame.height, palette_id=frame.palette_id)
        async with self._lock:
            self.pending_frame = frame
            self._frames[frame.frame_id] = frame
            if frame.frame_id in self._frame_order:
                self._frame_order.remove(frame.frame_id)
            self._frame_order.append(frame.frame_id)
            keep = {self.pending_frame.frame_id}
            if self.displayed_frame:
                keep.add(self.displayed_frame.frame_id)
            for old_id in self._frame_order[:-64]:
                if old_id not in keep:
                    self._frames.pop(old_id, None)
            self._frame_order = [item for item in self._frame_order if item in self._frames]
            # A regular render is pending until the endpoint receives a
            # success report.  A forced regenerate also keeps this true when
            # the image bytes happen to be unchanged.
            self.redisplay_required = self.displayed_frame is None or self.displayed_frame.frame_id != frame.frame_id or self.redisplay_required
            await self._async_save()
        self.async_signal_update()

    def get_frame(self, frame_id: str) -> RenderedFrame | None:
        return self._frames.get(frame_id)

    def current_manifest_frame(self) -> RenderedFrame | None:
        return self.pending_frame or self.displayed_frame

    def is_authorized(self, key: str | None) -> bool:
        if not isinstance(key, str) or not key:
            return False
        candidate = hashlib.sha256(key.encode("utf-8")).hexdigest()
        return hmac.compare_digest(candidate, self.device_key_hash)

    def allow_request(self, source: str, kind: str) -> bool:
        """Bound per-device/source request volume before parsing bodies."""

        now = time.monotonic()
        bucket = getattr(self, "_request_buckets", {})
        self._request_buckets = bucket
        key = f"{source}:{kind}"
        recent = [stamp for stamp in bucket.get(key, []) if now - stamp < 60]
        limit = 10 if kind == "report" else 60
        if len(recent) >= limit:
            bucket[key] = recent
            return False
        recent.append(now)
        bucket[key] = recent
        return True

    def async_signal_update(self) -> None:
        if self.coordinator is not None:
            self.coordinator.async_update_listeners()

    async def async_request_regenerate(self) -> None:
        self.redisplay_required = True
        await self._async_save()
        if self.coordinator is not None:
            await self.coordinator.async_request_refresh()
        self.async_signal_update()

    def connection_delayed(self, now: datetime | None = None) -> bool:
        current = now or datetime.now(timezone.utc)
        if self.last_seen is None:
            baseline = self.next_wake_at or self.created_at
            return current > baseline + timedelta(minutes=10)
        if self.next_wake_at is None:
            return False
        return current > self.next_wake_at + timedelta(minutes=10)

    async def async_handle_report(self, payload: Any) -> DeviceReportResult:
        if not isinstance(payload, dict):
            return DeviceReportResult(False, "", 400, "invalid_json")
        report_id = payload.get("report_id")
        if not isinstance(report_id, str) or not REPORT_ID_RE.fullmatch(report_id):
            return DeviceReportResult(False, "", 422, "invalid_report_id")
        async with self._lock:
            if report_id in self._report_ids:
                return DeviceReportResult(True, report_id)
            if payload.get("schema_version") != SCHEMA_VERSION:
                return DeviceReportResult(False, report_id, 422, "unsupported_schema")
            if not isinstance(payload.get("boot_id"), str) or not 1 <= len(payload["boot_id"]) <= 128:
                return DeviceReportResult(False, report_id, 422, "invalid_boot_id")
            if not isinstance(payload.get("firmware_version"), str) or not 1 <= len(payload["firmware_version"]) <= 64:
                return DeviceReportResult(False, report_id, 422, "invalid_firmware_version")
            received_id = payload.get("received_frame_id")
            displayed_id = payload.get("displayed_frame_id")
            for value in (received_id, displayed_id):
                if value is not None and (not isinstance(value, str) or not FRAME_ID_RE.fullmatch(value)):
                    return DeviceReportResult(False, report_id, 422, "invalid_frame_id")
            if received_id and received_id not in self._frames:
                return DeviceReportResult(False, report_id, 409, "unknown_received_frame")
            if displayed_id and displayed_id not in self._frames:
                return DeviceReportResult(False, report_id, 409, "unknown_displayed_frame")
            result = payload.get("display_result")
            if not isinstance(result, str):
                return DeviceReportResult(False, report_id, 422, "invalid_display_result")
            if result not in {"success", "skipped", "failed"}:
                return DeviceReportResult(False, report_id, 422, "invalid_display_result")
            local_overlay = payload.get("local_overlay")
            if not isinstance(local_overlay, str) or local_overlay not in {"none", "offline", "time_unknown"}:
                return DeviceReportResult(False, report_id, 422, "invalid_local_overlay")
            overlay_source_raw = payload.get("overlay_source_time")
            if overlay_source_raw is not None and _parse_time(overlay_source_raw) is None:
                return DeviceReportResult(False, report_id, 422, "invalid_overlay_source_time")
            next_wake_raw = payload.get("next_wake_at")
            if next_wake_raw is not None and _parse_time(next_wake_raw) is None:
                return DeviceReportResult(False, report_id, 422, "invalid_next_wake_at")
            if result == "skipped" and not displayed_id:
                return DeviceReportResult(False, report_id, 422, "missing_skipped_frame")
            completion_time = _parse_time(payload.get("display_completed_at"))
            stale_success = False
            if result == "success":
                if not displayed_id:
                    return DeviceReportResult(False, report_id, 409, "unknown_displayed_frame")
                if completion_time is None:
                    return DeviceReportResult(False, report_id, 422, "invalid_display_completed_at")
                candidate = self._frames[displayed_id]
                if self.displayed_frame is not None and candidate.generated_at < self.displayed_frame.generated_at:
                    stale_success = True
                elif (
                    self.displayed_frame is not None
                    and candidate.generated_at == self.displayed_frame.generated_at
                    and self.last_displayed is not None
                    and completion_time <= self.last_displayed
                ):
                    stale_success = True
                if not stale_success:
                    self.displayed_frame = candidate
                    self.last_displayed = completion_time
                    self.local_overlay = local_overlay
                    self.overlay_source_time = _parse_time(overlay_source_raw)
                if not stale_success and self.pending_frame and self.displayed_frame.frame_id == self.pending_frame.frame_id:
                    self.redisplay_required = False
            self.last_seen = datetime.now(timezone.utc)
            if not stale_success:
                battery = payload.get("battery_percent")
                self.battery_percent = float(battery) if isinstance(battery, (int, float)) and not isinstance(battery, bool) and 0 <= battery <= 100 else None
                rssi = payload.get("wifi_rssi_dbm")
                self.wifi_signal_dbm = int(rssi) if isinstance(rssi, int) and -150 <= rssi <= 0 else None
                self.next_wake_at = _parse_time(next_wake_raw)
                self.last_error = payload.get("error_code") if isinstance(payload.get("error_code"), str) else None
                self.firmware_version = payload["firmware_version"]
            self._report_ids.append(report_id)
            self._report_ids = self._report_ids[-64:]
            await self._async_save()
        self.async_signal_update()
        return DeviceReportResult(True, report_id)

    async def async_mark_seen(self) -> None:
        """Record authenticated manifest/frame traffic without changing ACK state."""

        async with self._lock:
            self.last_seen = datetime.now(timezone.utc)
            await self._async_save()
        self.async_signal_update()
