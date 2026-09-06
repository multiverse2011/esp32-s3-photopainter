"""Focused contract tests for the pure PhotoPainter core."""

from __future__ import annotations

import unittest
from datetime import datetime, timedelta, timezone

from custom_components.photopainter.core import (
    forecast_boundaries,
    normalize_calendar_events,
    normalize_forecast,
    validate_packed4,
)


JST = timezone(timedelta(hours=9))


class CoreContractTests(unittest.TestCase):
    def test_forecast_boundary_ceils_to_next_three_hour_mark(self) -> None:
        now = datetime(2026, 9, 6, 13, 0, tzinfo=JST)
        result = forecast_boundaries(now, "Asia/Tokyo")
        self.assertEqual(result[0].strftime("%Y-%m-%d %H:%M"), "2026-09-06 15:00")
        exact = forecast_boundaries(datetime(2026, 9, 6, 15, 0, tzinfo=JST), "Asia/Tokyo")
        self.assertEqual(exact[0].strftime("%H:%M"), "15:00")

    def test_all_day_plain_iso_endpoints_are_included(self) -> None:
        now = datetime(2026, 9, 6, 13, 0, tzinfo=JST)
        section = normalize_calendar_events(
            [{"summary": "Holiday", "start": "2026-09-06", "end": "2026-09-07"}],
            owner_id="calendar_1",
            entity_id="calendar.star",
            now=now,
            timezone_name="Asia/Tokyo",
            collected_at=now,
        )
        self.assertEqual(section.status, "ok")
        self.assertTrue(section.events[0].all_day)

    def test_malformed_event_does_not_look_like_successful_empty(self) -> None:
        now = datetime(2026, 9, 6, 13, 0, tzinfo=JST)
        section = normalize_calendar_events(
            [{"summary": "missing endpoints"}],
            owner_id="calendar_1",
            entity_id="calendar.star",
            now=now,
            timezone_name="Asia/Tokyo",
            collected_at=now,
        )
        self.assertEqual(section.status, "unavailable")
        self.assertFalse(section.complete)
        self.assertEqual(section.error_reason, "invalid_event")

    def test_unconfigured_forecast_keeps_four_slots_and_next_day_label(self) -> None:
        now = datetime(2026, 9, 6, 23, 59, tzinfo=JST)
        section = normalize_forecast(
            None,
            entity_id=None,
            now=now,
            timezone_name="Asia/Tokyo",
            collected_at=now,
            source_temperature_unit="°C",
        )
        self.assertEqual(len(section.slots), 4)
        self.assertEqual(section.slots[0].day_offset, 1)

    def test_stale_forecast_rebases_to_current_targets_with_placeholders(self) -> None:
        previous = datetime(2026, 9, 6, 18, 0, tzinfo=JST)
        current = datetime(2026, 9, 6, 23, 59, tzinfo=JST)
        old = normalize_forecast(
            [{"datetime": "2026-09-06T15:00:00+09:00", "condition": "sunny", "temperature": 21}],
            entity_id="weather.home",
            now=previous,
            timezone_name="Asia/Tokyo",
            collected_at=previous,
            source_temperature_unit="°C",
        )
        stale = normalize_forecast(
            None,
            entity_id="weather.home",
            now=current,
            timezone_name="Asia/Tokyo",
            collected_at=current,
            source_temperature_unit="°C",
            fetch_failed=True,
            cached_slots=old.slots,
            cache_collected_at=previous,
        )
        self.assertEqual(stale.status, "stale")
        self.assertEqual(len(stale.slots), 4)
        self.assertEqual([slot.day_offset for slot in stale.slots], [1, 1, 1, 1])
        self.assertTrue(all(slot.temperature_c is None for slot in stale.slots))

    def test_packed_frame_rejects_reserved_nibble(self) -> None:
        valid = bytes([0x11]) * 192000
        validate_packed4(valid)
        with self.assertRaises(ValueError):
            validate_packed4(bytes([0x41]) * 192000)

    def test_non_finite_or_boolean_forecast_temperature_is_missing(self) -> None:
        now = datetime(2026, 9, 6, 13, 0, tzinfo=JST)
        section = normalize_forecast(
            [
                {"datetime": "2026-09-06T15:00:00+09:00", "condition": "sunny", "temperature": True},
                {"datetime": "2026-09-06T18:00:00+09:00", "condition": "sunny", "temperature": float("nan")},
            ],
            entity_id="weather.home",
            now=now,
            timezone_name="Asia/Tokyo",
            collected_at=now,
            source_temperature_unit="°C",
        )
        self.assertIsNone(section.slots[0].temperature_c)
        self.assertIsNone(section.slots[0].raw_temperature)
        self.assertIsNone(section.slots[1].temperature_c)
        self.assertIsNone(section.slots[1].raw_temperature)


if __name__ == "__main__":
    unittest.main()
