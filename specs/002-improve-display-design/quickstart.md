# Quickstart: E-ink Display Design Improvement

**Feature**: 002-improve-display-design
**Date**: 2025-12-20

## Overview

このガイドでは、E-ink天気カレンダーの新UIデザイン実装を開始するための手順を説明します。

## Prerequisites

### Development Environment

- ESP-IDF v5.x installed
- Python 3.8+
- VS Code with ESP-IDF extension (recommended)

### Hardware

- ESP32-S3 (Waveshare PhotoPainter board)
- Waveshare 7.3" 7-color E-Paper display (800x480)
- USB-C cable

### API Keys

1. **OpenWeatherMap API Key** (既存)
   - [OpenWeatherMap](https://openweathermap.org/api) でアカウント作成
   - Free planで1000回/日のAPI呼び出し可能

2. **Todoist API Token** (新規)
   - [Todoist Developer](https://developer.todoist.com/) でAPIトークン取得
   - Settings → Integrations → Developer からトークンを確認

## Quick Setup

### 1. Clone and Configure

```bash
# Clone repository (if not already done)
git clone <repository-url>
cd esp32-s3-photopainter

# Checkout feature branch
git checkout 002-improve-display-design
```

### 2. Configure API Keys

```bash
# Open menuconfig
idf.py menuconfig
```

Navigate to:
- `Component config` → `Weather Service` → Set OpenWeatherMap API key
- `Component config` → `Task Service` → Set Todoist API token
- `Component config` → `Train Service` → Set target line name (e.g., "中央線")

### 3. Build and Flash

```bash
# Build
idf.py build

# Flash
idf.py -p /dev/ttyUSB0 flash

# Monitor (optional)
idf.py -p /dev/ttyUSB0 monitor
```

## Project Structure

```
components/
├── calendar_ui/          # UI rendering (modify)
├── task_service/         # NEW: Todoist integration
├── train_service/        # NEW: JR East scraping
├── weather_service/      # Modify for hourly forecast
├── gfx_library/          # Existing
├── epd_driver/           # Existing
└── wifi_manager/         # Existing

main/
└── main.c                # Main application
```

## Key Files to Modify

| File | Changes |
|------|---------|
| `components/weather_service/include/weather_types.h` | daily[4] → hourly[5] |
| `components/weather_service/weather_http.c` | 3-hour forecast API |
| `components/calendar_ui/calendar_ui.c` | New 2-column layout |
| `main/main.c` | Integrate new services |

## New Components to Create

### task_service

```c
// components/task_service/include/task_service.h
esp_err_t task_service_init(void);
esp_err_t task_service_fetch(task_list_t *tasks);
void task_service_deinit(void);
```

### train_service

```c
// components/train_service/include/train_service.h
esp_err_t train_service_init(void);
esp_err_t train_service_fetch(train_status_t *status);
void train_service_deinit(void);
```

## Layout Reference

```
+------------------+----------------------------------------+
|                  |  Weather                               |
|   10             |  +------+------+------+------+------+  |
|   (huge)         |  |HH:00 |HH:00 |HH:00 |HH:00 |HH:00 |  |
|                  |  |☀️    |⛅    |🌧️    |🌧️    |☀️    |  |
|   December       |  |5°C   |8°C   |10°C  |3°C   |2°C   |  |
|                  |  |95%   |70%   |30%   |20%   |15%   |  |
|   Tasks          |  +------+------+------+------+------+  |
|   • Task 1       |----------------------------------------|
|   • Task 2       |  Train                                 |
|                  |  平常運転 / 遅延あり                    |
|                  |----------------------------------------|
|                  |                    Updated YYYY/MM/DD  |
+------------------+----------------------------------------+
   200px                          600px
```

## Testing

### Manual Testing Checklist

- [ ] WiFi connection successful
- [ ] Weather data fetched (5 time slots)
- [ ] Tasks displayed from Todoist
- [ ] Train status displayed
- [ ] Layout renders correctly
- [ ] Cache works after deep sleep

### Debug Tips

```bash
# Enable verbose logging
idf.py menuconfig
# Component config → Log output → Default log verbosity → Debug
```

## Troubleshooting

| Issue | Solution |
|-------|----------|
| Weather not loading | Check OpenWeatherMap API key in menuconfig |
| Tasks empty | Verify Todoist token and "today" tasks exist |
| Train always UNKNOWN | Check line name matches exactly (e.g., "中央線") |
| Display garbled | Verify SPI connections, check E-Paper driver |

## Next Steps

1. Review [data-model.md](./data-model.md) for data structures
2. Review [contracts/](./contracts/) for API contracts
3. Generate tasks with `/speckit.tasks`
4. Implement features following generated tasks
