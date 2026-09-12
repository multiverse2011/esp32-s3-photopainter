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
        coordinator = hass.data["photopainter"]["coordinators"][entry.entry_id]
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
            snapshot_frame, snapshot_next_wake, snapshot_redisplay = await runtime.async_manifest_snapshot()
            assert snapshot_frame is not None
            assert snapshot_next_wake is not None
            assert manifest["frame"]["id"] == snapshot_frame.frame_id
            assert manifest["schedule"]["next_poll_at"] == snapshot_next_wake.isoformat()
            assert manifest["redisplay_required"] == snapshot_redisplay
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

        # A one-shot timer must be replaced even when collection/rendering
        # raises. Restore the implementation after exercising the failure so
        # the smoke test can shut down cleanly.
        original_update = coordinator._async_update_data_impl

        async def failing_update() -> object:
            raise RuntimeError("smoke failure")

        coordinator._async_update_data_impl = failing_update
        try:
            try:
                await coordinator._async_update_data()
            except RuntimeError:
                pass
            else:
                raise AssertionError("coordinator failure was swallowed")
            assert coordinator._scheduled_target is not None
            assert coordinator._scheduled_target > datetime.now(timezone.utc)
        finally:
            coordinator._async_update_data_impl = original_update

        def state_snapshot() -> tuple[object, ...]:
            return (
                runtime.pending_frame.frame_id if runtime.pending_frame else None,
                runtime.displayed_frame.frame_id if runtime.displayed_frame else None,
                tuple(sorted(runtime._frames)),
                tuple(runtime._frame_order),
                tuple(runtime._report_ids),
                runtime.last_seen,
                runtime.last_displayed,
                runtime.battery_percent,
                runtime.wifi_signal_dbm,
                runtime.next_wake_at,
                runtime.last_error,
                runtime.local_overlay,
                runtime.overlay_source_time,
                runtime.firmware_version,
                runtime.redisplay_required,
            )

        # Exercise malformed new reports through the runtime so the HTTP
        # report bucket remains below its ten-per-minute limit.  Every field
        # that can be null is still required on the wire; the overlay source
        # time is the one optional exception.
        valid_report = {
            "schema_version": 1,
            "report_id": "validation-good",
            "boot_id": "smoke-boot",
            "firmware_version": "smoke",
            "received_frame_id": None,
            "displayed_frame_id": None,
            "display_result": "failed",
            "display_completed_at": None,
            "local_overlay": "none",
            "battery_percent": None,
            "wifi_rssi_dbm": None,
            "next_wake_at": None,
            "error_code": None,
            "unknown_field": {"ignored": True},
        }
        result = await runtime.async_handle_report(valid_report)
        assert result.accepted and result.status == 200
        accepted_state = state_snapshot()

        # A known ID is idempotent before validating the body, including when
        # a device retries with a malformed copy of an already accepted
        # report.
        duplicate_bad = dict(valid_report, schema_version=True, battery_percent="bad")
        result = await runtime.async_handle_report(duplicate_bad)
        assert result.accepted and result.status == 200
        assert state_snapshot() == accepted_state

        nullable_fields = (
            "received_frame_id",
            "displayed_frame_id",
            "display_completed_at",
            "battery_percent",
            "wifi_rssi_dbm",
            "next_wake_at",
            "error_code",
        )
        invalid_cases: list[tuple[str, dict[str, object]]] = [
            ("bool-schema", {"schema_version": True}),
            ("float-schema", {"schema_version": 1.0}),
            ("invalid-battery", {"battery_percent": "not-a-number"}),
            ("bool-battery", {"battery_percent": True}),
            ("low-battery", {"battery_percent": -1}),
            ("high-battery", {"battery_percent": 101}),
            ("invalid-rssi", {"wifi_rssi_dbm": "not-a-number"}),
            ("bool-rssi", {"wifi_rssi_dbm": True}),
            ("float-rssi", {"wifi_rssi_dbm": -60.0}),
            ("low-rssi", {"wifi_rssi_dbm": -151}),
            ("high-rssi", {"wifi_rssi_dbm": 1}),
            ("invalid-error", {"error_code": 7}),
            ("invalid-completion", {"display_completed_at": "not-a-time"}),
            ("invalid-next-wake", {"next_wake_at": "not-a-time"}),
            ("invalid-overlay-time", {"overlay_source_time": 7}),
            # UTC normalization can overflow even after fromisoformat parses
            # these strings; they must still be rejected as invalid times.
            ("completion-overflow", {"display_completed_at": "0001-01-01T00:00:00+01:00"}),
            ("next-wake-overflow", {"next_wake_at": "9999-12-31T23:59:59-01:00"}),
        ]
        for field in nullable_fields:
            invalid_cases.append((f"missing-{field}", {"missing": field}))

        for case, change in invalid_cases:
            candidate = dict(valid_report, report_id=f"validation-{case}")
            if "missing" in change:
                candidate.pop(change["missing"], None)
            else:
                candidate.update(change)
            before = state_snapshot()
            result = await runtime.async_handle_report(candidate)
            assert not result.accepted and result.status == 422, (case, result)
            assert candidate["report_id"] not in runtime._report_ids
            assert state_snapshot() == before, case

        # A rejected ID remains available for the corrected resend, and an
        # optional overlay_source_time may be sent explicitly as JSON null.
        retry_id = "validation-retry"
        invalid_retry = dict(valid_report, report_id=retry_id, battery_percent="bad")
        before = state_snapshot()
        result = await runtime.async_handle_report(invalid_retry)
        assert not result.accepted and result.status == 422
        assert retry_id not in runtime._report_ids
        assert state_snapshot() == before
        corrected_retry = dict(invalid_retry, battery_percent=None, overlay_source_time=None)
        result = await runtime.async_handle_report(corrected_retry)
        assert result.accepted and result.status == 200
        assert retry_id in runtime._report_ids
        print("PhotoPainter HA smoke: PASS")
    finally:
        await hass.async_stop(force=True)


if __name__ == "__main__":
    asyncio.run(main())
