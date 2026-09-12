"""Authenticated PhotoPainter device API v1."""

from __future__ import annotations

import json
import asyncio
import time
from datetime import datetime, timezone
from typing import Any

from aiohttp import web
from homeassistant.components.http import HomeAssistantView
from homeassistant.core import HomeAssistant

from .const import (
    DOMAIN,
    FRAME_BYTES,
    MANIFEST_LIMIT,
    MIN_REFRESH_SECONDS,
    PALETTE_ID,
    REPORT_LIMIT,
    SCHEMA_VERSION,
)
from .runtime import FRAME_ID_RE, PhotoPainterRuntime

_RUNTIME_KEY = "runtimes"


def runtime_for(hass: HomeAssistant, device_id: str) -> PhotoPainterRuntime | None:
    runtimes = hass.data.get(DOMAIN, {}).get(_RUNTIME_KEY, {})
    runtime = runtimes.get(device_id)
    return runtime


def _unauthorized() -> web.Response:
    return web.json_response({"error": "unauthorized"}, status=401, headers={"WWW-Authenticate": "Bearer"})


async def _authorized(request: web.Request, device_id: str, kind: str) -> PhotoPainterRuntime | web.Response:
    runtime = runtime_for(request.app["hass"], device_id)
    if runtime is None:
        return web.json_response({"error": "not_found"}, status=404)
    header = request.headers.get("Authorization", "")
    scheme, _, key = header.partition(" ")
    if scheme.lower() != "bearer" or not runtime.is_authorized(key):
        return _unauthorized()
    source = request.remote or "unknown"
    if not runtime.allow_request(source, kind):
        return web.json_response({"error": "rate_limited"}, status=429, headers={"Retry-After": "60"})
    await runtime.async_mark_seen()
    return runtime


def _retry_after_at(next_wake_at: datetime | None) -> int:
    if next_wake_at is None:
        return 1800
    seconds = int((next_wake_at - datetime.now(timezone.utc)).total_seconds())
    return max(MIN_REFRESH_SECONDS, min(7200, seconds))


class _PhotoPainterView(HomeAssistantView):
    requires_auth = False
    cors_allowed = False

    @staticmethod
    def _hass(request: web.Request) -> HomeAssistant:
        return request.app["hass"]


class ManifestView(_PhotoPainterView):
    url = "/api/photopainter/v1/devices/{device_id}/manifest"
    name = "api:photopainter:manifest"

    async def get(self, request: web.Request, device_id: str) -> web.Response:
        authorized = await _authorized(request, device_id, "manifest")
        if isinstance(authorized, web.Response):
            return authorized
        runtime = authorized
        if request.content_type not in {"application/json", "application/octet-stream"}:
            return web.json_response({"error": "unsupported_content_type"}, status=415)
        frame, next_wake_at, redisplay_required = await runtime.async_manifest_snapshot()
        if frame is None:
            return web.json_response(
                {"error": "frame_not_ready"},
                status=503,
                headers={"Retry-After": str(_retry_after_at(next_wake_at))},
            )
        frame_path = f"/api/photopainter/v1/devices/{device_id}/frames/{frame.frame_id}"
        payload = {
            "schema_version": SCHEMA_VERSION,
            "device_id": device_id,
            "server_time": datetime.now(timezone.utc).isoformat(),
            "frame": frame.manifest_fragment(frame_path),
            "schedule": {
                "next_poll_at": next_wake_at.isoformat() if next_wake_at else None,
                "retry_after_seconds": _retry_after_at(next_wake_at),
                "min_refresh_seconds": MIN_REFRESH_SECONDS,
            },
            "redisplay_required": redisplay_required,
        }
        encoded = json.dumps(payload, separators=(",", ":")).encode("utf-8")
        if len(encoded) > MANIFEST_LIMIT:
            return web.json_response({"error": "manifest_too_large"}, status=500)
        return web.Response(body=encoded, content_type="application/json")


class FrameView(_PhotoPainterView):
    url = "/api/photopainter/v1/devices/{device_id}/frames/{frame_id}"
    name = "api:photopainter:frame"

    async def get(self, request: web.Request, device_id: str, frame_id: str) -> web.Response:
        authorized = await _authorized(request, device_id, "frame")
        if isinstance(authorized, web.Response):
            return authorized
        runtime = authorized
        if not FRAME_ID_RE.fullmatch(frame_id):
            return web.json_response({"error": "invalid_frame_id"}, status=422)
        frame = runtime.get_frame(frame_id)
        if frame is None:
            return web.json_response({"error": "frame_not_found"}, status=404)
        etag = f'"{frame.frame_id}"'
        if request.headers.get("If-None-Match") == etag:
            return web.Response(status=304, headers={"ETag": etag})
        return web.Response(
            body=frame.data,
            content_type="application/octet-stream",
            headers={
                "Content-Length": str(FRAME_BYTES),
                "ETag": etag,
                "Cache-Control": "private, max-age=86400, immutable",
                "Content-Encoding": "identity",
            },
        )


class ReportView(_PhotoPainterView):
    url = "/api/photopainter/v1/devices/{device_id}/reports"
    name = "api:photopainter:report"

    async def post(self, request: web.Request, device_id: str) -> web.Response:
        authorized = await _authorized(request, device_id, "report")
        if isinstance(authorized, web.Response):
            return authorized
        runtime = authorized
        encoding = request.headers.get("Content-Encoding")
        if encoding and encoding.lower() not in {"identity"}:
            return web.json_response({"error": "compressed_reports_not_supported"}, status=415)
        content_length = request.headers.get("Content-Length")
        if content_length and (not content_length.isdigit() or int(content_length) > REPORT_LIMIT):
            return web.json_response({"error": "report_too_large"}, status=413)
        chunks: list[bytes] = []
        total = 0
        deadline = time.monotonic() + 30
        while True:
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                return web.json_response({"error": "report_timeout"}, status=408)
            try:
                chunk = await asyncio.wait_for(request.content.read(1024), timeout=min(5, remaining))
            except asyncio.TimeoutError:
                return web.json_response({"error": "report_timeout"}, status=408)
            if not chunk:
                break
            total += len(chunk)
            if total > REPORT_LIMIT:
                return web.json_response({"error": "report_too_large"}, status=413)
            chunks.append(chunk)
        body = b"".join(chunks)
        try:
            payload = json.loads(body)
        except (json.JSONDecodeError, UnicodeDecodeError):
            return web.json_response({"error": "invalid_json"}, status=400)
        result = await runtime.async_handle_report(payload)
        if not result.accepted:
            return web.json_response({"error": result.error or "rejected"}, status=result.status)
        return web.json_response({"accepted": True, "report_id": result.report_id})


def register_views(hass: HomeAssistant) -> None:
    if hass.data.setdefault(DOMAIN, {}).get("views_registered"):
        return
    hass.http.register_view(ManifestView)
    hass.http.register_view(FrameView)
    hass.http.register_view(ReportView)
    hass.data[DOMAIN]["views_registered"] = True
