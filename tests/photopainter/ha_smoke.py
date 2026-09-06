"""Target-HA smoke test.

Run from the repository root with the pinned target image:

  docker run --rm --entrypoint python \
    -v "$PWD:/project" -w /project \
    ghcr.io/home-assistant/home-assistant:2026.9.1 \
    tests/photopainter/ha_smoke.py

The workstation's Python does not contain Home Assistant, so this script is
intentionally separate from the stdlib unittest suite.
"""

from __future__ import annotations

import asyncio
import hashlib
import os
import shutil
from datetime import datetime, timedelta, timezone
from types import MappingProxyType

from aiohttp.test_utils import TestClient, TestServer
from homeassistant import bootstrap, loader
from homeassistant.config_entries import ConfigEntry
from homeassistant.core import HomeAssistant


async def main() -> None:
    runtime_dir = "/tmp/photopainter-ha-smoke"
    shutil.rmtree(runtime_dir, ignore_errors=True)
    os.makedirs(runtime_dir, exist_ok=True)
    os.symlink("/project/custom_components", f"{runtime_dir}/custom_components")
    hass = HomeAssistant(runtime_dir)
    hass.config.skip_pip = True
    loader.async_setup(hass)
    try:
        await bootstrap.async_from_config_dict(
            {
                "homeassistant": {
                    "name": "PhotoPainter smoke",
                    "latitude": 0,
                    "longitude": 0,
                    "elevation": 0,
                    "unit_system": "metric",
                    "time_zone": "Asia/Tokyo",
                },
                "http": {"server_host": "127.0.0.1"},
            },
            hass,
        )
        key = "smoke-device-key"
        entry = ConfigEntry(
            version=1,
            minor_version=0,
            domain="photopainter",
            title="Smoke display",
            data={
                "device_id": "smoke-display",
                "device_key_hash": hashlib.sha256(key.encode()).hexdigest(),
                "timezone": "Asia/Tokyo",
            },
            options={},
            source="user",
            unique_id="smoke-display",
            discovery_keys=MappingProxyType({}),
            subentries_data=[],
        )
        hass.config_entries._entries[entry.entry_id] = entry
        assert await hass.config_entries.async_setup(entry.entry_id)
        await hass.async_block_till_done()
        runtime = hass.data["photopainter"]["runtimes"]["smoke-display"]
        assert runtime.pending_frame is not None
        assert len(hass.states.async_all()) >= 11

        async with TestClient(TestServer(hass.http.app)) as client:
            prefix = "/api/photopainter/v1/devices/smoke-display"
            response = await client.get(f"{prefix}/manifest")
            assert response.status == 401
            headers = {"Authorization": f"Bearer {key}"}
            response = await client.get(f"{prefix}/manifest", headers=headers)
            assert response.status == 200
            manifest = await response.json()
            response = await client.get(manifest["frame"]["path"], headers=headers)
            frame = await response.read()
            assert response.status == 200 and len(frame) == 192000
            assert hashlib.sha256(frame).hexdigest() == manifest["frame"]["id"]
            now = datetime.now(timezone.utc)
            report = {
                "schema_version": 1,
                "report_id": "smoke-1",
                "boot_id": "smoke-boot",
                "firmware_version": "smoke",
                "received_frame_id": manifest["frame"]["id"],
                "displayed_frame_id": manifest["frame"]["id"],
                "display_result": "success",
                "display_completed_at": now.isoformat(),
                "local_overlay": "none",
                "battery_percent": None,
                "wifi_rssi_dbm": -60,
                "next_wake_at": (now + timedelta(minutes=30)).isoformat(),
                "error_code": None,
            }
            response = await client.post(f"{prefix}/reports", json=report, headers=headers)
            assert response.status == 200
            response = await client.post(f"{prefix}/reports", json=report, headers=headers)
            assert response.status == 200
            displayed_at = runtime.last_displayed
            displayed_overlay = runtime.local_overlay
            next_wake = runtime.next_wake_at
            report["report_id"] = "smoke-stale-ack"
            report["display_completed_at"] = (now - timedelta(minutes=1)).isoformat()
            report["local_overlay"] = "offline"
            report["next_wake_at"] = (now + timedelta(hours=2)).isoformat()
            report["error_code"] = "stale_should_not_overwrite"
            response = await client.post(f"{prefix}/reports", json=report, headers=headers)
            assert response.status == 200
            assert runtime.last_displayed == displayed_at
            assert runtime.local_overlay == displayed_overlay
            assert runtime.next_wake_at == next_wake
            assert runtime.last_error is None
            report["report_id"] = "smoke-unknown-frame"
            report["displayed_frame_id"] = "0" * 64
            response = await client.post(f"{prefix}/reports", json=report, headers=headers)
            assert response.status == 409
            report["report_id"] = "smoke-bad-schema"
            report["displayed_frame_id"] = manifest["frame"]["id"]
            report["schema_version"] = 99
            response = await client.post(f"{prefix}/reports", json=report, headers=headers)
            assert response.status == 422
        print("PhotoPainter HA smoke: PASS")
    finally:
        await hass.async_stop(force=True)


if __name__ == "__main__":
    asyncio.run(main())
