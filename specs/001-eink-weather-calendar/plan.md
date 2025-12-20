# Implementation Plan: ESP32-S3 E-ink Weather Calendar

**Branch**: `001-eink-weather-calendar` | **Date**: 2025-12-20 | **Spec**: [spec.md](./spec.md)
**Input**: Feature specification from `/specs/001-eink-weather-calendar/spec.md`

## Summary

4日間天気予報付きE-inkカレンダーをESP32-S3上に実装します。Waveshare 7.3インチ 7色E-Paperディスプレイ（800x480）を使用し、WiFi経由でOpenWeatherMap APIから天気データを取得、ディープスリープによる省電力動作で長期バッテリー駆動を実現します。ESP-IDFコンポーネント方式でモジュール化された設計を採用します。

## Technical Context

**Language/Version**: C (ESP-IDF v5.x, FreeRTOS)
**Primary Dependencies**: ESP-IDF (WiFi, HTTPS, SNTP), cJSON, SPI Driver
**Storage**: NVS (Non-Volatile Storage) for configuration and weather cache, RTC Memory for fast wake
**Testing**: ESP-IDF Unity framework, manual hardware testing
**Target Platform**: ESP32-S3 (Waveshare PhotoPainter board)
**Project Type**: Embedded single firmware
**Performance Goals**: Boot-to-display in 30 seconds, 15-20 second display refresh
**Constraints**: PSRAM 384KB for framebuffer, 8MB Flash, deep sleep <10μA, API calls ≤8/day
**Scale/Scope**: Single device, 4-day forecast, 7-color display (800x480)

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

Constitution file is currently a template with no project-specific principles defined. Proceeding with standard embedded development best practices:

| Gate | Status | Notes |
|------|--------|-------|
| Modular design | ✅ PASS | ESP-IDF component architecture used |
| Testability | ✅ PASS | Each component independently testable |
| Error handling | ✅ PASS | Fallback mechanisms defined |
| Documentation | ✅ PASS | Header files with API documentation |

No violations to justify.

## Project Structure

### Documentation (this feature)

```text
specs/001-eink-weather-calendar/
├── plan.md              # This file
├── research.md          # Phase 0 output
├── data-model.md        # Phase 1 output
├── quickstart.md        # Phase 1 output
├── contracts/           # Phase 1 output (API contracts)
└── tasks.md             # Phase 2 output (/speckit.tasks)
```

### Source Code (repository root)

```text
main/
├── main.c                    # Application entry point, state machine
├── Kconfig.projbuild         # menuconfig settings
└── CMakeLists.txt

components/
├── epd_driver/               # E-Paper display driver
│   ├── epd_driver.c          # SPI communication, GPIO control
│   ├── epd_spi.c             # SPI initialization, DMA transfer
│   ├── include/
│   │   └── epd_driver.h
│   └── CMakeLists.txt
│
├── gfx_library/              # Graphics library
│   ├── gfx_paint.c           # Drawing functions, buffer management
│   ├── gfx_primitives.c      # Shapes (line, rect, circle)
│   ├── fonts/
│   │   ├── font_16.c
│   │   ├── font_24.c
│   │   └── font_32.c
│   ├── include/
│   │   └── gfx_paint.h
│   └── CMakeLists.txt
│
├── weather_service/          # Weather data service
│   ├── weather_http.c        # HTTPS client
│   ├── weather_parser.c      # JSON parsing
│   ├── include/
│   │   ├── weather_service.h
│   │   └── weather_types.h
│   └── CMakeLists.txt
│
├── calendar_ui/              # Calendar UI rendering
│   ├── calendar_ui.c         # Layout rendering
│   ├── weather_icons.c       # Weather icon drawing
│   ├── include/
│   │   └── calendar_ui.h
│   └── CMakeLists.txt
│
└── wifi_manager/             # WiFi and time sync
    ├── wifi_manager.c        # WiFi connection, SNTP
    ├── include/
    │   └── wifi_manager.h
    └── CMakeLists.txt

sdkconfig.defaults            # Default configuration
partitions.csv                # Partition table (NVS, app)
```

**Structure Decision**: ESP-IDF component-based architecture selected. Each hardware/feature domain is isolated into a separate component with clear public APIs defined in header files. This enables independent development, testing, and reuse.

## Complexity Tracking

No Constitution violations requiring justification.

## Implementation Phases

### Phase 1: Foundation Setup

**Goal**: Project configuration and WiFi connectivity

- Configure ESP-IDF project (PSRAM, WiFi, HTTPS, cJSON)
- Create component directory structure
- Implement WiFi manager with SNTP time sync
- Create Kconfig for runtime configuration

**Deliverables**: Working WiFi connection with time sync

### Phase 2: Display Driver

**Goal**: E-Paper display communication

- Implement SPI driver for EPD
- GPIO configuration (DC, CS, RST, BUSY)
- Display initialization sequence
- PSRAM framebuffer allocation
- Display refresh and sleep commands

**Deliverables**: Ability to write framebuffer to display

### Phase 3: Graphics Library

**Goal**: Drawing primitives and text rendering

- Framebuffer initialization and clear
- Pixel, line, rectangle, circle drawing
- 7-color palette support
- Bitmap font rendering (16/24/32px)
- Text alignment utilities

**Deliverables**: Complete drawing API

### Phase 4: Weather Service

**Goal**: Weather data acquisition and caching

- HTTPS client for OpenWeatherMap API
- JSON parsing with cJSON
- Daily forecast extraction (noon data)
- NVS cache for offline fallback
- Retry logic with exponential backoff

**Deliverables**: Reliable weather data retrieval

### Phase 5: Calendar UI

**Goal**: Complete user interface

- Screen layout (header + 4 forecast columns)
- Weather icon rendering
- Temperature, humidity, wind display
- Date/time formatting
- Error state display

**Deliverables**: Full calendar display

### Phase 6: Power Management

**Goal**: Long battery life

- Deep sleep configuration
- RTC memory for fast wake
- Time-based update intervals (30min day / 2hr night)
- API call throttling (≤8/day)
- Display sleep command

**Deliverables**: Optimized power consumption

### Phase 7: Error Handling & Testing

**Goal**: Robust operation

- WiFi connection retry
- API failure fallback
- Display BUSY timeout
- Memory safety checks
- Integration testing

**Deliverables**: Production-ready firmware

## Hardware Configuration

### Pin Assignment (Waveshare ESP32-S3 PhotoPainter)

| Signal | GPIO | Direction | Description |
|--------|------|-----------|-------------|
| DC     | 8    | Output    | Data/Command control |
| CS     | 9    | Output    | Chip Select |
| SCK    | 10   | Output    | SPI Clock |
| MOSI   | 11   | Output    | SPI Data Out |
| RST    | 12   | Output    | Hardware Reset |
| BUSY   | 13   | Input     | Busy Status |

### SPI Configuration

- Host: SPI3_HOST
- Clock: 10MHz (start at 1MHz for debugging)
- DMA: Enabled
- Mode: Mode 0 (CPOL=0, CPHA=0)

### Memory Allocation

| Region | Usage | Size |
|--------|-------|------|
| PSRAM | Framebuffer | 384KB (800×480) |
| PSRAM | JSON buffer | 16-32KB |
| Internal RAM | DMA buffer | 4KB |
| Internal RAM | Weather data struct | ~2KB |
| RTC Memory | Weather cache | ~2KB |

## 7-Color Palette

| Index | Color | Usage |
|-------|-------|-------|
| 0 | Black | Text, borders |
| 1 | White | Background |
| 2 | Green | Good conditions |
| 3 | Blue | Rain, cold temps |
| 4 | Red | Warnings, high temps |
| 5 | Yellow | Sun icon |
| 6 | Orange | Warm temps |

## External API Integration

### OpenWeatherMap Forecast API

- **Endpoint**: `https://api.openweathermap.org/data/2.5/forecast`
- **Parameters**: `lat`, `lon`, `appid`, `units=metric`, `cnt=32`
- **Rate Limit**: 1000 calls/day (free tier)
- **Usage Target**: ≤8 calls/day (3-hour intervals)
- **Response**: 3-hour interval forecasts, extract noon data for daily

### NTP Time Sync

- **Servers**: `pool.ntp.org`, `time.nist.gov`
- **Sync Frequency**: Once per 24 hours or on boot
- **Timezone**: Asia/Tokyo (JST, UTC+9) default
