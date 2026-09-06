"""Constants shared by the PhotoPainter Home Assistant integration.

The wire palette is deliberately kept in one place.  The values are the
candidate Waveshare/Spectra 6 nibble mapping from the feature specification;
the hardware profile is opt-in in the firmware until the panel colour chart
has been measured.
"""

from __future__ import annotations

DOMAIN = "photopainter"
NAME = "PhotoPainter"
VERSION = "0.1.0"

SCHEMA_VERSION = 1
WIDTH = 800
HEIGHT = 480
FRAME_BYTES = WIDTH * HEIGHT // 2
PALETTE_ID = "spectra6-ws73-v1"
PALETTE_HARDWARE_VERIFIED = False
ALLOWED_NIBBLES = frozenset((0x0, 0x1, 0x2, 0x3, 0x5, 0x6))

# Logical colours used by the renderer and by the packed4 wire contract.
BLACK = 0
WHITE = 1
YELLOW = 2
RED = 3
BLUE = 5
GREEN = 6

OVERLAY = {"x": 344, "y": 24, "width": 432, "height": 40}
MANIFEST_LIMIT = 8 * 1024
REPORT_LIMIT = 4 * 1024
MAX_FORECAST_ITEMS = 200
MAX_CALENDAR_ITEMS = 200
MIN_REFRESH_SECONDS = 300
DEFAULT_DAY_INTERVAL = 30
DEFAULT_NIGHT_INTERVAL = 120
DEFAULT_DAY_START = "06:00"
DEFAULT_DAY_END = "22:00"

CALENDAR_OWNERS = (
    ("calendar_1", "Star", "mdi:star-four-points"),
    ("calendar_2", "Moon", "mdi:moon-waning-crescent"),
)

ROOMS = ("living", "study", "bedroom", "outdoor")
ROOM_LABELS = {
    "living": "Living",
    "study": "Study",
    "bedroom": "Bedroom",
    "outdoor": "Outdoor",
}

FORECAST_ICONS = {
    "sunny": ("weather-sunny", RED),
    "clear-night": ("weather-night", BLUE),
    "partlycloudy": ("weather-partly-cloudy", BLACK),
    "cloudy": ("weather-cloudy", BLACK),
    "rainy": ("weather-rainy", BLUE),
    "pouring": ("weather-pouring", BLUE),
    "snowy": ("weather-snowy", BLUE),
    "snowy-rainy": ("weather-snowy-rainy", BLUE),
    "lightning": ("weather-lightning", RED),
    "lightning-rainy": ("weather-lightning-rainy", RED),
    "fog": ("weather-fog", BLACK),
    "windy": ("weather-windy", BLACK),
    "windy-variant": ("weather-windy-variant", BLACK),
    "hail": ("weather-hail", BLUE),
    "exceptional": ("alert-circle-outline", RED),
}

STORAGE_VERSION = 1
STORAGE_KEY = f"{DOMAIN}.state"

