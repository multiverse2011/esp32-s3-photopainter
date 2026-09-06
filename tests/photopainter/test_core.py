"""Focused contract tests for the pure PhotoPainter core."""

from __future__ import annotations

import unittest
from datetime import datetime, timedelta, timezone

from custom_components.photopainter.config import day_window_minutes, parse_hhmm
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


class DayWindowTests(unittest.TestCase):
    def test_hours_past_midnight_are_accepted(self) -> None:
        self.assertEqual(parse_hhmm("08:00", name="day_start"), 8 * 60)
        self.assertEqual(parse_hhmm("8:00", name="day_start"), 8 * 60)
        self.assertEqual(parse_hhmm("26:00", name="day_end"), 26 * 60)

    def test_wrapping_window_matches_the_past_midnight_notation(self) -> None:
        self.assertEqual(day_window_minutes("08:00", "02:00"), (480, 1560))
        self.assertEqual(
            day_window_minutes("08:00", "02:00"),
            day_window_minutes("08:00", "26:00"),
        )

    def test_window_without_wrap_is_left_alone(self) -> None:
        self.assertEqual(day_window_minutes("06:00", "22:00"), (360, 1320))

    def test_malformed_and_oversized_windows_are_rejected(self) -> None:
        for value in ("24:60", "8", "0800", "48:00", "aa:bb", ""):
            with self.assertRaises(ValueError):
                parse_hhmm(value, name="day_start")
        with self.assertRaises(ValueError):
            day_window_minutes("08:00", "40:00")


if __name__ == "__main__":
    unittest.main()
