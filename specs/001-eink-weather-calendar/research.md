# Research: ESP32-S3 E-ink Weather Calendar

**Date**: 2025-12-20
**Feature**: 001-eink-weather-calendar

## Research Topics

This document consolidates research findings for key technical decisions in the ESP32-S3 E-ink Weather Calendar project.

---

## 1. E-Paper Display Driver

### Decision: Custom SPI driver with ESP-IDF native APIs

### Rationale

The Waveshare 7.3-inch 7-color E-Paper display uses a specific initialization sequence and command set. While generic libraries exist, a custom driver provides:

- Full control over timing-critical operations
- Optimal memory management for PSRAM framebuffer
- Direct integration with ESP-IDF SPI and GPIO APIs
- Compatibility with the specific display model (ACeP 7-color technology)

### Alternatives Considered

| Alternative | Rejected Because |
|-------------|------------------|
| GxEPD2 library | Arduino-focused, not optimized for ESP-IDF, heavy abstraction |
| LovyanGFX | Complex setup for this specific display, overkill for single-purpose device |
| Waveshare sample code | Copy-paste approach, need to understand and optimize |

### Key Implementation Notes

- Display uses ACeP (Advanced Color ePaper) technology with 7-color palette
- Initialization sequence: Power Setting → Booster → Panel Setting → Resolution → VCOM
- Full refresh takes 15-20 seconds (no partial refresh support)
- BUSY pin must be monitored with timeout (max 20 seconds)
- SPI frequency: Start at 1MHz, increase to 10MHz after verification

---

## 2. Weather API Selection

### Decision: OpenWeatherMap 5-day/3-hour Forecast API

### Rationale

OpenWeatherMap provides a robust, well-documented API with a generous free tier (1000 calls/day). The 5-day/3-hour forecast endpoint provides sufficient data for our 4-day display with flexibility in data selection.

### Alternatives Considered

| Alternative | Rejected Because |
|-------------|------------------|
| Open-Meteo | Good but less established, fewer icon codes |
| Weather.gov | US-only, not suitable for Japan default |
| AccuWeather | Limited free tier (50 calls/day) |
| Tomorrow.io | Complex pricing, overkill for simple forecast |

### Key Implementation Notes

- Endpoint: `/data/2.5/forecast?lat={lat}&lon={lon}&appid={key}&units=metric&cnt=32`
- Returns 3-hour intervals; extract 12:00 (noon) data for each day
- Response size: ~15-20KB JSON, fits in PSRAM buffer
- Cache response in NVS for offline fallback
- Limit API calls to 8/day (every 3 hours during active period)

### Icon Code Mapping

| API Code | Weather | Display Icon |
|----------|---------|--------------|
| 01d/01n | Clear | Yellow sun |
| 02d/02n | Few clouds | Sun + cloud |
| 03d/03n | Scattered clouds | Cloud |
| 04d/04n | Broken clouds | Dark cloud |
| 09d/09n | Shower rain | Cloud + rain |
| 10d/10n | Rain | Cloud + rain |
| 11d/11n | Thunderstorm | Cloud + lightning |
| 13d/13n | Snow | Cloud + snow |
| 50d/50n | Mist | Wavy lines |

---

## 3. Memory Management Strategy

### Decision: PSRAM for large buffers, internal RAM for time-critical data

### Rationale

ESP32-S3 has limited internal RAM (~320KB usable) but supports external PSRAM (8MB on PhotoPainter). Framebuffer (384KB) must use PSRAM, while DMA buffers and frequently accessed data should use internal RAM for speed.

### Memory Layout

| Region | Data | Size | Rationale |
|--------|------|------|-----------|
| PSRAM | Framebuffer | 384KB | Too large for internal RAM |
| PSRAM | JSON response | 32KB | Large, infrequent access |
| Internal RAM | DMA buffer | 4KB | SPI DMA requires internal RAM |
| Internal RAM | Weather struct | 2KB | Frequent access during rendering |
| RTC Memory | Weather cache | 2KB | Survives deep sleep |
| NVS | Config + full cache | 16KB | Persistent storage |

### Key Implementation Notes

- Use `heap_caps_malloc(size, MALLOC_CAP_SPIRAM)` for PSRAM allocation
- DMA buffers MUST use `MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL`
- RTC memory: Use `RTC_DATA_ATTR` for variables surviving deep sleep
- Monitor heap fragmentation with `heap_caps_get_largest_free_block()`

---

## 4. Power Management

### Decision: Timer-based deep sleep with RTC memory caching

### Rationale

E-ink displays retain image without power, making deep sleep ideal. Timer wake provides predictable update intervals without external wake sources.

### Power States

| State | Current | Duration | Notes |
|-------|---------|----------|-------|
| Active (WiFi + display) | ~200mA | 20-30s | Full operation |
| Active (display only) | ~100mA | 15-20s | Refresh cycle |
| Deep sleep | ~10μA | 30min-2hr | Timer wake enabled |

### Update Schedule

| Time Period | Interval | API Calls | Rationale |
|-------------|----------|-----------|-----------|
| 06:00-22:00 (day) | 30 min | 32/day | User likely viewing |
| 22:00-06:00 (night) | 2 hours | 4/day | Low activity period |
| API fetch | 3 hours | 8/day | Match 3-hour forecast data |

### Battery Life Estimate

```
Active energy per cycle: 200mA × 30s = 1.67mAh
Cycles per day: ~40 (day) + 4 (night) = 44
Daily consumption: 44 × 1.67mAh = 73mAh
Deep sleep: 10μA × 24h = 0.24mAh
Total daily: ~74mAh
3000mAh battery: ~40 days (conservative estimate)
```

Note: IMPLEMENTATION_PLAN.md estimates 650 days, which assumes much shorter active time. Actual battery life depends on WiFi connection time.

---

## 5. Error Handling Strategy

### Decision: Graceful degradation with cached data fallback

### Rationale

Network connectivity is inherently unreliable. The device should continue to display useful information even when WiFi or API is unavailable.

### Error Scenarios

| Error | Detection | Recovery |
|-------|-----------|----------|
| WiFi connection failed | Event timeout (30s) | Retry 3x, then use cache |
| API request failed | HTTP error / timeout | Retry 3x exponential backoff, use cache |
| JSON parse error | cJSON returns NULL | Use cache, log error |
| Display BUSY timeout | >20s on BUSY pin | Hardware reset, retry once |
| PSRAM allocation failed | NULL return | Reduce buffer size, retry |
| NVS read error | ESP_ERR return | Use defaults |

### Cache Freshness Display

When using cached data, display should indicate:
- "Last updated: [timestamp]" in header
- Different icon treatment (e.g., grayed out) if data >6 hours old
- Error icon if data >24 hours old

---

## 6. Font and Icon Strategy

### Decision: Bitmap fonts (16/24/32px) + geometric shape icons

### Rationale

Vector fonts require complex rendering libraries. Bitmap fonts are simple, fast, and predictable for fixed-size displays. Geometric icons use existing drawing primitives.

### Font Sizes

| Size | Usage | Characters |
|------|-------|------------|
| 16px | Secondary info (wind, humidity) | ASCII 0x20-0x7E |
| 24px | Date, temperature | ASCII 0x20-0x7E + ° |
| 32px | Current time, day names | ASCII 0x20-0x7E |

### Icon Design

All icons are 120×120 pixels, drawn with graphics primitives:

| Icon | Components |
|------|------------|
| Sun | Yellow filled circle + orange rays (lines) |
| Cloud | White filled ellipses + black outline |
| Rain | Cloud + blue vertical lines |
| Snow | Cloud + white circles (snowflakes) |
| Thunder | Cloud + yellow zigzag polyline |
| Fog | Horizontal wavy lines |

### Alternatives Considered

| Alternative | Rejected Because |
|-------------|------------------|
| TrueType fonts | Complex parsing, high memory usage |
| Pre-rendered images | Flash storage, inflexible |
| Unicode support | Not needed for MVP, adds complexity |

---

## 7. Build and Configuration

### Decision: Kconfig for runtime settings, sdkconfig.defaults for build

### Rationale

ESP-IDF's Kconfig system provides a user-friendly menuconfig interface for non-technical users while maintaining type-safe configuration.

### Configuration Items

| Item | Type | Storage | Default |
|------|------|---------|---------|
| WiFi SSID | String | Kconfig | "myssid" |
| WiFi Password | String | Kconfig | "mypassword" |
| API Key | String | Kconfig | "" |
| Latitude | String | Kconfig | "35.6762" (Tokyo) |
| Longitude | String | Kconfig | "139.6503" (Tokyo) |
| Timezone | String | Kconfig | "JST-9" |
| Update interval (day) | Integer | Hardcoded | 30 min |
| Update interval (night) | Integer | Hardcoded | 120 min |

### Build Configuration (sdkconfig.defaults)

```ini
# Critical settings for PhotoPainter board
CONFIG_SPIRAM=y
CONFIG_SPIRAM_MODE_OCT=y
CONFIG_SPIRAM_SPEED_80M=y
CONFIG_ESP_MAIN_TASK_STACK_SIZE=8192
CONFIG_PARTITION_TABLE_CUSTOM=y
```

---

## Conclusion

All major technical decisions have been resolved. No NEEDS CLARIFICATION markers remain. The project is ready to proceed to Phase 1 design and implementation.

### Key Risks Identified

1. **Display refresh time**: 15-20 seconds is significant; ensure user expectations are set
2. **WiFi connection time**: May dominate power consumption; optimize with fast connect
3. **JSON parsing memory**: Monitor PSRAM usage during API response parsing
4. **HTTPS certificate**: May need to embed root CA for OpenWeatherMap

### References

- [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/)
- [OpenWeatherMap API Docs](https://openweathermap.org/forecast5)
- [Waveshare 7.3inch e-Paper Wiki](https://www.waveshare.com/wiki/7.3inch_e-Paper_HAT)
- Sample code: `C:\Users\iris\Projects\xiaozhi-esp32-sample`
