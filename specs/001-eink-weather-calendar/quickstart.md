# Quickstart Guide: ESP32-S3 E-ink Weather Calendar

**Date**: 2025-12-20
**Feature**: 001-eink-weather-calendar

## Prerequisites

### Hardware

- Waveshare ESP32-S3 PhotoPainter board
- 7.3-inch 7-color E-Paper display (800x480)
- USB-C cable for programming
- (Optional) 3.7V LiPo battery

### Software

- ESP-IDF v5.x installed and configured
- OpenWeatherMap API key ([Get one here](https://openweathermap.org/api))
- Git

## Quick Setup

### 1. Clone and Enter Project

```bash
cd C:\Users\iris\Projects\esp32-s3-photopainter
git checkout 001-eink-weather-calendar
```

### 2. Set Target

```bash
idf.py set-target esp32s3
```

### 3. Configure Project

```bash
idf.py menuconfig
```

Navigate to **Weather Calendar Configuration** and set:

| Setting | Description | Example |
|---------|-------------|---------|
| WiFi SSID | Your WiFi network name | `MyHomeWiFi` |
| WiFi Password | Your WiFi password | `********` |
| OpenWeatherMap API Key | 32-character API key | `abc123...` |
| Location Latitude | Your latitude | `35.6762` |
| Location Longitude | Your longitude | `139.6503` |

### 4. Build and Flash

```bash
idf.py build
idf.py -p COM3 flash monitor
```

Replace `COM3` with your actual serial port.

## Expected Boot Sequence

```
I (0) cpu_start: Starting ESP32-S3
I (100) wifi: WiFi connecting...
I (3000) wifi: WiFi connected
I (3100) sntp: Time synchronized
I (3200) weather: Fetching weather data...
I (5000) weather: Weather data received
I (5100) epd: Initializing display...
I (5500) epd: Drawing to framebuffer...
I (6000) epd: Starting display refresh...
I (25000) epd: Refresh complete
I (25100) main: Entering deep sleep for 30 minutes
```

## Troubleshooting

### WiFi Connection Failed

```
E (30000) wifi: Connection timeout
```

**Fix**: Verify SSID and password in menuconfig. Ensure 2.4GHz network (5GHz not supported).

### API Error

```
E (5000) weather: HTTP error 401
```

**Fix**: Check API key is correct and active at [OpenWeatherMap](https://home.openweathermap.org/api_keys).

### Display Not Updating

```
E (60000) epd: BUSY timeout
```

**Fix**: Check display cable connection. Try hardware reset:

```bash
idf.py -p COM3 flash monitor  # Re-flash to trigger cold boot
```

### PSRAM Not Detected

```
E (100) heap_caps: Could not allocate SPIRAM
```

**Fix**: Run `idf.py menuconfig`, ensure:
- `Component config → ESP PSRAM → Support for external, SPI-connected RAM` is enabled
- `Component config → ESP PSRAM → Mode → Octal Mode` is selected

## Development Workflow

### Build Only

```bash
idf.py build
```

### Flash Only (without rebuild)

```bash
idf.py -p COM3 flash
```

### Monitor Only

```bash
idf.py -p COM3 monitor
```

Exit monitor: `Ctrl+]`

### Clean Build

```bash
idf.py fullclean
idf.py build
```

## Component Testing

### Test WiFi Only

In `main/main.c`, modify to test WiFi:

```c
void app_main(void) {
    wifi_manager_init();
    if (wifi_manager_connect(30000) == ESP_OK) {
        ESP_LOGI("TEST", "WiFi connected!");
        wifi_manager_sync_time(15000);
        ESP_LOGI("TEST", "Time: %lld", time(NULL));
    }
    wifi_manager_disconnect();
}
```

### Test Display Only

```c
void app_main(void) {
    epd_driver_init();
    gfx_init();

    // Fill screen with color test
    gfx_fill_rect(0, 0, 200, 480, EPD_COLOR_BLACK);
    gfx_fill_rect(200, 0, 200, 480, EPD_COLOR_RED);
    gfx_fill_rect(400, 0, 200, 480, EPD_COLOR_GREEN);
    gfx_fill_rect(600, 0, 200, 480, EPD_COLOR_BLUE);

    epd_driver_refresh();
    epd_driver_sleep();
}
```

### Test Weather API

```c
void app_main(void) {
    wifi_manager_init();
    wifi_manager_connect(30000);
    weather_service_init();

    weather_data_t weather;
    if (weather_service_fetch(&weather) == ESP_OK) {
        ESP_LOGI("TEST", "City: %s", weather.city_name);
        for (int i = 0; i < 4; i++) {
            ESP_LOGI("TEST", "Day %d: %.1f°C, %s",
                i, weather.daily[i].temp,
                weather.daily[i].description);
        }
    }

    wifi_manager_disconnect();
}
```

## File Structure After Setup

```
esp32-s3-photopainter/
├── main/
│   ├── main.c
│   ├── Kconfig.projbuild
│   └── CMakeLists.txt
├── components/
│   ├── epd_driver/
│   ├── gfx_library/
│   ├── weather_service/
│   ├── calendar_ui/
│   └── wifi_manager/
├── sdkconfig.defaults
├── CMakeLists.txt
└── partitions.csv
```

## Next Steps

1. **Full implementation**: Run `/speckit.tasks` to generate task list
2. **Review contracts**: See `contracts/component-apis.md` for API details
3. **Understand data flow**: See `data-model.md` for structures

## Resources

- [ESP-IDF Documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/)
- [OpenWeatherMap API](https://openweathermap.org/forecast5)
- [Waveshare Wiki](https://www.waveshare.com/wiki/7.3inch_e-Paper_HAT)
- [Project Spec](./spec.md)
- [Implementation Plan](./plan.md)
