"""Deterministic Layout B renderer.

Home Assistant renders the complete image so the ESP32 does not need a
Japanese-capable font or an icon font.  The renderer writes indexed pixels
directly and packs them according to the PhotoPainter ``packed4`` contract.
Packaged IBM/Noto/MDI assets can be supplied by a deployment; a deterministic
DejaVu fallback keeps diagnostics and contract tests usable when those assets
are not installed.
"""

from __future__ import annotations

import os
import math
import json
from io import BytesIO
from datetime import date, datetime, timedelta
from pathlib import Path
from typing import Any, Iterable
from zoneinfo import ZoneInfo

from PIL import Image, ImageDraw, ImageFont

from .const import (
    BLACK,
    BLUE,
    CALENDAR_OWNERS,
    FORECAST_ICONS,
    GREEN,
    HEIGHT,
    OVERLAY,
    PALETTE_ID,
    RED,
    ROOM_LABELS,
    WHITE,
    WIDTH,
    YELLOW,
)
from .core import calendar_time_label, frame_sha256
from .models import CalendarSection, DisplaySnapshot, ForecastSlot, RenderedFrame, RoomReading


_PALETTE_RGB = (
    (0, 0, 0),
    (255, 255, 255),
    (255, 242, 0),
    (220, 0, 0),
    (255, 128, 0),  # unused candidate slot, retained only in metadata
    (0, 80, 190),
    (0, 155, 70),
)
_FONT_ROOT = Path(__file__).with_name("assets")
_SYSTEM_FONT_ROOTS = (
    Path("/usr/share/fonts/truetype"),
    Path("/usr/local/share/fonts"),
)

_MDI_CODEPOINTS = {
    "alert-circle-outline": 0xF05D6,
    "moon-waning-crescent": 0xF0F65,
    "star-four-points": 0xF0AE2,
    "weather-cloudy": 0xF0590,
    "weather-fog": 0xF0591,
    "weather-hail": 0xF0592,
    "weather-lightning": 0xF0593,
    "weather-lightning-rainy": 0xF067E,
    "weather-night": 0xF0594,
    "weather-night-partly-cloudy": 0xF0F31,
    "weather-partly-cloudy": 0xF0595,
    "weather-pouring": 0xF0596,
    "weather-rainy": 0xF0597,
    "weather-snowy": 0xF0598,
    "weather-snowy-rainy": 0xF067F,
    "weather-sunny": 0xF0599,
    "weather-windy": 0xF059D,
    "weather-windy-variant": 0xF059E,
}
_FALLBACK_MASKS: dict[str, list[str]] | None = None


def _font_candidates(*names: str) -> Iterable[Path]:
    configured = os.environ.get("PHOTOPAINTER_FONT_DIR")
    roots = [Path(configured)] if configured else []
    roots.extend((_FONT_ROOT, *_SYSTEM_FONT_ROOTS))
    for root in roots:
        for name in names:
            yield root / name
            # Search one level below common system font roots without an
            # expensive recursive scan for every text draw.
            if root.exists() and root.is_dir():
                yield from root.glob(f"*/{name}")


def _font(
    size: int,
    *,
    mono: bool = False,
    japanese: bool = False,
    medium: bool = False,
) -> ImageFont.FreeTypeFont | ImageFont.ImageFont:
    if japanese:
        names = (
            "NotoSansJP-Regular.ttf",
            "NotoSansCJK-Regular.ttc",
            "NotoSansCJKjp-Regular.otf",
            "NotoSansJP-wght.ttf",
        )
    elif mono:
        names = (("IBMPlexMono-Medium.ttf",) if medium else ()) + (
            "IBMPlexMono-Regular.ttf",
            "DejaVuSansMono.ttf",
        )
    else:
        names = (("IBMPlexSans-Medium.ttf",) if medium else ()) + (
            "IBMPlexSans-Regular.ttf",
            "DejaVuSans.ttf",
        )
    for candidate in _font_candidates(*names):
        try:
            if candidate.exists():
                font = ImageFont.truetype(str(candidate), size)
                if japanese and hasattr(font, "set_variation_by_axes"):
                    # Noto Sans JP is variable; its upstream default axis is
                    # 100, while the spec requires regular weight 400.
                    font.set_variation_by_axes([400])
                return font
        except OSError:
            continue
    return ImageFont.load_default()


def _mdi_font(size: int) -> ImageFont.FreeTypeFont | ImageFont.ImageFont | None:
    for candidate in _font_candidates("materialdesignicons-webfont.ttf"):
        try:
            if candidate.exists():
                return ImageFont.truetype(str(candidate), size)
        except OSError:
            continue
    return None


def _fallback_masks() -> dict[str, list[str]]:
    global _FALLBACK_MASKS
    if _FALLBACK_MASKS is None:
        path = _FONT_ROOT / "fallback-mdi-masks.json"
        try:
            _FALLBACK_MASKS = json.loads(path.read_text(encoding="utf-8"))
        except (OSError, ValueError):
            _FALLBACK_MASKS = {}
    return _FALLBACK_MASKS


def _draw_mdi_mask(draw: ImageDraw.ImageDraw, name: str, size: int, x: int, y: int, fill: int) -> bool:
    mask = _fallback_masks().get(f"{name}@{size}")
    if not isinstance(mask, list) or len(mask) != size:
        return False
    for row, bits in enumerate(mask):
        for column, bit in enumerate(bits):
            if bit == "1":
                draw.point((x + column, y + row), fill=fill)
    return True


def _mdi_glyph(name: str) -> str:
    return chr(_MDI_CODEPOINTS.get(name, 0xF05D6))


def _palette_image() -> Image.Image:
    image = Image.new("P", (WIDTH, HEIGHT), WHITE)
    palette: list[int] = []
    for red, green, blue in _PALETTE_RGB:
        palette.extend((red, green, blue))
    palette.extend([255] * (256 * 3 - len(palette)))
    image.putpalette(palette)
    return image


def _fit_text(draw: ImageDraw.ImageDraw, text: str, font: ImageFont.ImageFont, width: int) -> str:
    """Truncate a title to one line at a Unicode code-point boundary."""

    if width <= 0:
        return ""
    if draw.textbbox((0, 0), text, font=font)[2] <= width:
        return text
    ellipsis = "…"
    result = ""
    for character in text:
        candidate = result + character + ellipsis
        if draw.textbbox((0, 0), candidate, font=font)[2] > width:
            break
        result += character
    return result + ellipsis if result else ellipsis


def _font_for_character(character: str, size: int, *, mono: bool = False) -> ImageFont.ImageFont:
    return _font(size, mono=mono, japanese=ord(character) > 0x024F)


# A codepoint no font maps, used to learn what this font's "missing glyph" looks
# like.  Plane 16 private use, so even icon fonts leave it alone.
_NOTDEF_SAMPLE = "􏿽"
_GLYPH_CACHE: dict[tuple[int, bool, str], bool] = {}


_PROBE_BOX = 96


def _render_probe(font: ImageFont.FreeTypeFont | ImageFont.ImageFont, character: str) -> bytes:
    probe = Image.new("L", (_PROBE_BOX, _PROBE_BOX), 0)
    ImageDraw.Draw(probe).text((4, 4), character, font=font, fill=255)
    return probe.tobytes()


def _has_glyph(character: str, size: int, *, mono: bool = False) -> bool:
    """Report whether the font picked for this character can actually draw it.

    Pillow does not expose a font's cmap, and a missing glyph is drawn as the
    font's own `.notdef` box rather than raising, so the character is compared
    against a codepoint that is certainly missing.
    """

    key = (size, mono, character)
    cached = _GLYPH_CACHE.get(key)
    if cached is not None:
        return cached
    font = _font_for_character(character, size, mono=mono)
    result = _render_probe(font, character) != _render_probe(font, _NOTDEF_SAMPLE)
    _GLYPH_CACHE[key] = result
    return result


def supported_text(text: str, size: int) -> str:
    """Drop characters the fonts cannot draw, so no `.notdef` box reaches the panel.

    Emoji are the common case: calendar titles carry them and the bundled Noto
    Sans JP has no glyphs for them.  Whitespace orphaned by a dropped character
    goes with it, so "🎂 ゆうり" renders as "ゆうり" rather than " ゆうり".
    """

    if all(_has_glyph(character, size) for character in text):
        return text
    kept = "".join(character for character in text if _has_glyph(character, size))
    return kept.strip()


def _mixed_width(draw: ImageDraw.ImageDraw, text: str, size: int, *, mono: bool = False) -> int:
    return sum(draw.textbbox((0, 0), char, font=_font_for_character(char, size, mono=mono))[2] for char in text)


def _fit_mixed_text(draw: ImageDraw.ImageDraw, text: str, size: int, width: int) -> str:
    if _mixed_width(draw, text, size) <= width:
        return text
    result = ""
    for character in text:
        if _mixed_width(draw, result + character + "…", size) > width:
            break
        result += character
    return result + "…" if result else "…"


def _draw_mixed_text(draw: ImageDraw.ImageDraw, position: tuple[float, float], text: str, size: int, fill: int) -> None:
    x, y = position
    for character in text:
        font = _font_for_character(character, size)
        draw.text((x, y), character, font=font, fill=fill)
        x += draw.textbbox((0, 0), character, font=font)[2]


def _draw_owner_icon(draw: ImageDraw.ImageDraw, owner_id: str, x: int, y: int) -> None:
    """Draw the pinned MDI 7.4.47 glyph as a black/white mask."""

    name = "star-four-points" if owner_id == "calendar_1" else "moon-waning-crescent"
    font = _mdi_font(24)
    if font is not None:
        draw.text((x, y - 1), _mdi_glyph(name), font=font, fill=BLACK)
    elif not _draw_mdi_mask(draw, name, 24, x, y, BLACK):
        raise RuntimeError(f"missing MDI asset and fallback mask: {name}")


def _draw_weather_icon(draw: ImageDraw.ImageDraw, slot: ForecastSlot, cx: int, cy: int) -> None:
    icon = FORECAST_ICONS.get(slot.condition or "")
    color = icon[1] if icon else BLACK
    condition = slot.condition or ""
    icon_key = {
        "partlycloudy-night": "weather-night-partly-cloudy",
        "partlycloudy": "weather-partly-cloudy",
    }.get(condition, FORECAST_ICONS.get(condition, (None, color))[0])
    if icon_key in _MDI_CODEPOINTS:
        font = _mdi_font(32)
        if font is not None:
            glyph = _mdi_glyph(icon_key)
            bbox = draw.textbbox((0, 0), glyph, font=font)
            draw.text((cx - (bbox[2] - bbox[0]) / 2, cy - 18), glyph, font=font, fill=color)
        elif not _draw_mdi_mask(draw, icon_key, 32, cx - 16, cy - 16, color):
            raise RuntimeError(f"missing MDI asset and fallback mask: {icon_key}")
    else:
        font = _font(28, mono=True)
        marker = "—" if slot.condition is None else "?"
        bbox = draw.textbbox((0, 0), marker, font=font)
        draw.text((cx - (bbox[2] - bbox[0]) / 2, cy - (bbox[3] - bbox[1]) / 2 - 3), marker, font=font, fill=BLACK)


def _oldest_collected(snapshot: DisplaySnapshot) -> datetime | None:
    values: list[datetime] = []
    for section in snapshot.calendars:
        if section.collected_at:
            values.append(section.collected_at)
    for room in snapshot.rooms:
        for reading in (room.temperature, room.humidity):
            if reading.collected_at:
                values.append(reading.collected_at)
    if snapshot.forecast.collected_at:
        values.append(snapshot.forecast.collected_at)
    return min(values) if values else None


def _draw_header(draw: ImageDraw.ImageDraw, snapshot: DisplaySnapshot, next_poll_at: datetime | None) -> None:
    date_font = _font(20)
    mono_font = _font(36, mono=True, medium=True)
    small_font = _font(16)
    tz = ZoneInfo(snapshot.display_timezone)
    local = snapshot.generated_at.astimezone(tz)
    collected = _oldest_collected(snapshot) or snapshot.generated_at
    collected_local = collected.astimezone(tz)
    date_text = local.strftime("%Y/%m/%d")
    draw.text((24, 22), date_text, font=mono_font, fill=BLACK)
    # The weekday sits on the date's own baseline rather than its own top edge.
    date_box = draw.textbbox((24, 22), date_text, font=mono_font)
    draw.text(
        (date_box[2] + 12, date_box[3]),
        f"({local.strftime('%a')})",
        font=date_font,
        fill=RED if local.weekday() == 6 else BLACK,
        anchor="ls",
    )
    draw.line((24, 80, 776, 80), fill=BLACK, width=2)
    next_local = next_poll_at.astimezone(tz) if next_poll_at else None
    if snapshot.status == "offline":
        first = "Offline"
        second = f"Last {collected_local.strftime('%b %d %H:%M')}"
    elif snapshot.status == "time_unknown":
        first = "Time not synced"
        second = ""
    elif snapshot.status == "stale":
        first = f"Stale · {collected_local.strftime('%b %d %H:%M')}"
        second = f"Next {next_local.strftime('%H:%M') if next_local else '—'}"
    else:
        first = f"Updated {collected_local.strftime('%b %d %H:%M')}"
        second = f"Next {next_local.strftime('%H:%M') if next_local else '—'}"
    draw.text((776, 24), _fit_text(draw, first, small_font, 432), font=small_font, fill=BLACK, anchor="ra")
    if second:
        draw.text((776, 45), _fit_text(draw, second, small_font, 432), font=small_font, fill=BLACK, anchor="ra")


def _section_label(draw: ImageDraw.ImageDraw, text: str, x: int, y: int, right: str | None = None) -> None:
    font = _font(16, medium=True)
    draw.text((x, y), text, font=font, fill=BLACK)
    if right:
        width = draw.textbbox((0, 0), right, font=font)[2]
        draw.text((x + 382 - width, y), right, font=font, fill=BLACK)


def _calendar_line(draw: ImageDraw.ImageDraw, event: Any, display_date: date, x: int, y: int, width: int) -> None:
    label = calendar_time_label(event, display_date)
    # Two-digit months push the dated label past the column, so it steps down
    # a size or two rather than running into the title.
    for size in (14, 13, 12):
        time_font = _font(size, mono=True)
        if draw.textbbox((0, 0), label, font=time_font)[2] <= 76:
            break
    draw.text((x, y + 4), label, font=time_font, fill=BLACK)
    title_x = x + 80
    title = _fit_mixed_text(draw, supported_text(event.title, 24), 24, width - 80)
    _draw_mixed_text(draw, (title_x, y - 3), title, 24, BLACK)


def _draw_calendar(draw: ImageDraw.ImageDraw, section: CalendarSection, x: int, y: int) -> None:
    _draw_owner_icon(draw, section.owner_id, x, y)
    title_font = _font(16)
    heading = ""
    if section.remaining_count is not None and section.remaining_count > 0:
        heading = f"+{section.remaining_count} more"
    elif not section.complete:
        heading = "More events"
    if heading:
        draw.text((x + 32, y + 3), heading, font=title_font, fill=BLACK)
    if section.status == "unconfigured":
        message = "Calendar not linked"
        draw.text((x, y + 39), message, font=_font(16), fill=BLACK)
        return
    if section.status == "unavailable":
        draw.text((x, y + 39), "Unavailable", font=_font(16), fill=BLACK)
        return
    if section.status == "stale":
        draw.text((x + 220, y + 3), "Stale", font=_font(16), fill=BLACK)
    if not section.events:
        draw.text((x, y + 39), "No events today or tomorrow", font=_font(16), fill=BLACK)
        return
    row_y = y + 38
    for event in section.events[:3]:
        _calendar_line(draw, event, section.display_date, x, row_y, 316)
        row_y += 39


def _format_temperature(value: float | None) -> str:
    if value is None:
        return "—"
    if value == 0:
        value = 0.0
    return f"{value:.1f}°"


def _format_humidity(value: float | None) -> str:
    if value is None:
        return "—"
    return f"{round(value):.0f}%"


def _draw_room(draw: ImageDraw.ImageDraw, room: RoomReading, x: int, y: int, width: int = 183) -> None:
    draw.text((x, y), room.display_name, font=_font(20), fill=BLACK)
    temperature = room.temperature.value
    humidity = room.humidity.value
    humidity_color = BLUE if humidity is not None and humidity >= 70 else BLACK
    baseline = y + 59
    draw.text((x, baseline), _format_temperature(temperature), font=_font(34, mono=True), fill=BLACK, anchor="ls")
    draw.text((x + 110, baseline), _format_humidity(humidity), font=_font(18, mono=True), fill=humidity_color, anchor="ls")


def _format_forecast_temp(slot: ForecastSlot) -> str:
    if slot.temperature_c is None:
        return "—"
    rounded = int(slot.temperature_c + 0.5) if slot.temperature_c >= 0 else int(slot.temperature_c - 0.5)
    return f"{rounded if rounded else 0}°"


def _draw_forecast(draw: ImageDraw.ImageDraw, slots: list[ForecastSlot], x: int, y: int) -> None:
    slot_width = 88
    time_font = _font(16, mono=True)
    temp_font = _font(24, mono=True)
    for index in range(4):
        slot = slots[index] if index < len(slots) else None
        cx = x + slot_width * index + slot_width // 2
        if slot is None:
            continue
        label = slot.label
        bbox = draw.textbbox((0, 0), label, font=time_font)
        draw.text((cx - (bbox[2] - bbox[0]) / 2, y), label, font=time_font, fill=BLACK)
        _draw_weather_icon(draw, slot, cx, y + 42)
        temp = _format_forecast_temp(slot)
        bbox = draw.textbbox((0, 0), temp, font=temp_font)
        draw.text((cx - (bbox[2] - bbox[0]) / 2, y + 72), temp, font=temp_font, fill=BLACK)


def _fresh_until(snapshot: DisplaySnapshot) -> datetime | None:
    values: list[datetime] = []
    for section in snapshot.calendars:
        if section.collected_at:
            values.append(section.collected_at + timedelta(hours=1))
    for room in snapshot.rooms:
        if room.temperature.collected_at:
            values.append(room.temperature.collected_at + timedelta(hours=1))
        if room.humidity.collected_at:
            values.append(room.humidity.collected_at + timedelta(hours=1))
    if snapshot.forecast.collected_at:
        values.append(snapshot.forecast.collected_at + timedelta(hours=6))
    return min(values) if values else None


def _pack4(image: Image.Image) -> bytes:
    pixels = image.load()
    output = bytearray(WIDTH * HEIGHT // 2)
    offset = 0
    for y in range(HEIGHT):
        for x in range(0, WIDTH, 2):
            output[offset] = ((int(pixels[x, y]) & 0x0F) << 4) | (int(pixels[x + 1, y]) & 0x0F)
            offset += 1
    return bytes(output)


def render_snapshot(snapshot: DisplaySnapshot, *, next_poll_at: datetime | None = None) -> RenderedFrame:
    """Render a snapshot into an immutable, hash-addressed frame."""

    image = _palette_image()
    draw = ImageDraw.Draw(image)
    _draw_header(draw, snapshot, next_poll_at)

    _section_label(draw, "PLANS", 24, 96)
    draw.line((370, 96, 370, 456), fill=BLACK, width=2)
    calendars = snapshot.calendars
    for index, (owner_id, _name, _icon) in enumerate(CALENDAR_OWNERS):
        section = next((item for item in calendars if item.owner_id == owner_id), None)
        if section is not None:
            _draw_calendar(draw, section, 24, 128 if index == 0 else 304)

    _section_label(draw, "CLIMATE", 394, 96, "°C / % RH")
    by_room = {room.room_id: room for room in snapshot.rooms}
    positions = {
        "living": (394, 128),
        "study": (590, 128),
        "bedroom": (394, 224),
        "outdoor": (590, 224),
    }
    for room_id, (x, y) in positions.items():
        room = by_room.get(room_id)
        if room:
            _draw_room(draw, room, x, y)
        else:
            draw.text((x, y), ROOM_LABELS.get(room_id, room_id.title()), font=_font(20), fill=BLACK)
            draw.text((x, y + 27), "—", font=_font(34, mono=True), fill=BLACK)
            draw.text((x, y + 68), "—", font=_font(18, mono=True), fill=BLACK)

    if snapshot.forecast.status == "unconfigured":
        forecast_right = "Weather not linked"
    elif snapshot.forecast.status == "unavailable":
        forecast_right = "Forecast unavailable"
    elif snapshot.forecast.status == "stale":
        forecast_right = "Stale · °C"
    else:
        forecast_right = "3-HOURLY · °C"
    _section_label(draw, "FORECAST", 394, 324, forecast_right)
    _draw_forecast(draw, snapshot.forecast.slots, 394, 356)

    packed = _pack4(image)
    return RenderedFrame(
        frame_id=frame_sha256(packed),
        data=packed,
        generated_at=snapshot.generated_at,
        fresh_until=_fresh_until(snapshot),
        palette_id=PALETTE_ID,
        width=WIDTH,
        height=HEIGHT,
        status_overlay=dict(OVERLAY),
        snapshot=snapshot,
    )


def packed4_to_png(data: bytes) -> bytes:
    """Expand a validated packed4 frame for HA's authenticated Image entity."""

    from .core import validate_packed4

    validate_packed4(data)
    image = Image.new("P", (WIDTH, HEIGHT))
    palette: list[int] = []
    for color in _PALETTE_RGB:
        palette.extend(color)
    palette.extend([255] * (256 * 3 - len(palette)))
    image.putpalette(palette)
    pixels = image.load()
    offset = 0
    for y in range(HEIGHT):
        for x in range(0, WIDTH, 2):
            value = data[offset]
            pixels[x, y] = value >> 4
            pixels[x + 1, y] = value & 0x0F
            offset += 1
    stream = BytesIO()
    image.save(stream, format="PNG", optimize=False)
    return stream.getvalue()


def packed4_with_overlay_to_png(
    data: bytes,
    *,
    overlay: str,
    source_time: datetime | None,
    timezone_name: str,
) -> bytes:
    """Render the firmware's reserved status overlay for displayed preview."""

    from .core import validate_packed4

    validate_packed4(data)
    image = Image.new("P", (WIDTH, HEIGHT))
    palette: list[int] = []
    for color in _PALETTE_RGB:
        palette.extend(color)
    palette.extend([255] * (256 * 3 - len(palette)))
    image.putpalette(palette)
    pixels = image.load()
    offset = 0
    for y in range(HEIGHT):
        for x in range(0, WIDTH, 2):
            value = data[offset]
            pixels[x, y] = value >> 4
            pixels[x + 1, y] = value & 0x0F
            offset += 1
    draw = ImageDraw.Draw(image)
    draw.rectangle((OVERLAY["x"], OVERLAY["y"], OVERLAY["x"] + OVERLAY["width"] - 1, OVERLAY["y"] + OVERLAY["height"] - 1), fill=WHITE)
    font = _font(16)
    if overlay == "offline":
        first = "Offline"
        second = f"Last {source_time.astimezone(ZoneInfo(timezone_name)).strftime('%b %d %H:%M') if source_time else '—'}"
    elif overlay == "time_unknown":
        first, second = "Time not synced", ""
    else:
        first, second = "", ""
    if first:
        draw.text((OVERLAY["x"], OVERLAY["y"]), _fit_text(draw, first, font, OVERLAY["width"]), font=font, fill=BLACK)
    if second:
        draw.text((OVERLAY["x"], OVERLAY["y"] + 20), _fit_text(draw, second, font, OVERLAY["width"]), font=font, fill=BLACK)
    stream = BytesIO()
    image.save(stream, format="PNG", optimize=False)
    return stream.getvalue()
