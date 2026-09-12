"""Focused contract tests for the pure PhotoPainter core."""

from __future__ import annotations

import unittest
from datetime import datetime, timedelta, timezone

from custom_components.photopainter.config import day_window_minutes, parse_hhmm
from custom_components.photopainter.core import (
    calendar_time_label,
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

    def _plans(self, raw, *, now):
        return normalize_calendar_events(
            raw,
            owner_id="calendar_1",
            entity_id="calendar.star",
            now=now,
            timezone_name="Asia/Tokyo",
            collected_at=now,
        )

    def test_tomorrow_fills_the_slots_today_leaves_empty(self) -> None:
        now = datetime(2026, 9, 6, 22, 10, tzinfo=JST)
        section = self._plans(
            [
                {"summary": "tonight", "start": "2026-09-06T23:00:00+09:00", "end": "2026-09-06T23:30:00+09:00"},
                {"summary": "this morning", "start": "2026-09-06T10:00:00+09:00", "end": "2026-09-06T12:00:00+09:00"},
                {"summary": "tomorrow noon", "start": "2026-09-07T12:00:00+09:00", "end": "2026-09-07T13:00:00+09:00"},
                {"summary": "tomorrow all day", "start": "2026-09-07", "end": "2026-09-08"},
            ],
            now=now,
        )
        self.assertEqual([event.title for event in section.events], ["tonight", "tomorrow all day", "tomorrow noon"])

    def test_a_full_today_leaves_no_room_for_tomorrow(self) -> None:
        now = datetime(2026, 9, 6, 8, 0, tzinfo=JST)
        section = self._plans(
            [
                {"summary": "today 1", "start": "2026-09-06T09:00:00+09:00", "end": "2026-09-06T10:00:00+09:00"},
                {"summary": "today 2", "start": "2026-09-06T11:00:00+09:00", "end": "2026-09-06T12:00:00+09:00"},
                {"summary": "today 3", "start": "2026-09-06T13:00:00+09:00", "end": "2026-09-06T14:00:00+09:00"},
                {"summary": "today 4", "start": "2026-09-06T15:00:00+09:00", "end": "2026-09-06T16:00:00+09:00"},
                {"summary": "tomorrow", "start": "2026-09-07T09:00:00+09:00", "end": "2026-09-07T10:00:00+09:00"},
            ],
            now=now,
        )
        self.assertEqual([event.title for event in section.events], ["today 1", "today 2", "today 3"])
        self.assertEqual(section.remaining_count, 1)

    def test_overflowing_tomorrow_is_not_counted_as_more(self) -> None:
        now = datetime(2026, 9, 6, 22, 10, tzinfo=JST)
        section = self._plans(
            [
                {"summary": f"tomorrow {hour}", "start": f"2026-09-07T{hour:02d}:00:00+09:00", "end": f"2026-09-07T{hour + 1:02d}:00:00+09:00"}
                for hour in (9, 11, 13, 15)
            ],
            now=now,
        )
        self.assertEqual([event.title for event in section.events], ["tomorrow 9", "tomorrow 11", "tomorrow 13"])
        self.assertEqual(section.remaining_count, 0)

    def test_the_day_after_tomorrow_never_appears(self) -> None:
        now = datetime(2026, 9, 6, 22, 10, tzinfo=JST)
        section = self._plans(
            [{"summary": "too far", "start": "2026-09-08T09:00:00+09:00", "end": "2026-09-08T10:00:00+09:00"}],
            now=now,
        )
        self.assertEqual(section.events, [])
        self.assertEqual(section.status, "ok")

    def test_an_event_spanning_both_days_is_listed_once(self) -> None:
        now = datetime(2026, 9, 6, 22, 10, tzinfo=JST)
        section = self._plans(
            [{"summary": "long trip", "start": "2026-09-05", "end": "2026-09-09"}],
            now=now,
        )
        self.assertEqual([event.title for event in section.events], ["long trip"])

    def test_time_label_carries_the_date_of_the_day_it_is_shown_on(self) -> None:
        now = datetime(2026, 9, 6, 22, 10, tzinfo=JST)
        section = self._plans(
            [
                {"summary": "long trip", "start": "2026-09-05", "end": "2026-09-09"},
                {"summary": "tonight", "start": "2026-09-06T22:00:00+09:00", "end": "2026-09-06T23:30:00+09:00"},
                {"summary": "tomorrow", "start": "2026-09-07T09:00:00+09:00", "end": "2026-09-07T10:00:00+09:00"},
            ],
            now=now,
        )
        labels = [calendar_time_label(event, section.display_date) for event in section.events]
        self.assertEqual(labels, ["9/6 all", "9/6 Now", "9/7 09:00"])

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
