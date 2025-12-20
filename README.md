# ESP32-S3 E-ink Weather Calendar

A battery-powered smart display using ESP32-S3 and a Waveshare 7.3-inch ACeP 6-color E-Paper display. Shows weather forecast, task list, and train status with current date/time, optimized for low power consumption.

## Features

- **4-Day Weather Forecast**: Displays weather icons, temperatures, humidity, and wind information
- **Hourly Weather**: Shows 5-hour forecast with temperature and weather conditions
- **Task List**: Displays up to 5 tasks from external API
- **Train Status**: Shows train line delay status (JR East)
- **Current Date/Time**: Header shows current date and time with SNTP synchronization
- **Power Efficient**: Deep sleep mode with configurable wake intervals (30min day / 2hr night)
- **Offline Support**: Caches weather/task/train data when network is unavailable
- **API Throttling**: Rate-limits OpenWeatherMap API calls to every 3 hours
- **Power Management**: AXP2101 power management with charging status monitoring

## Hardware Requirements

- **Board**: [Waveshare ESP32-S3 PhotoPainter](https://www.waveshare.com/esp32-s3-photopainter.htm)
  - ESP32-S3-WROOM-1-N16R8 (16MB Flash, 8MB PSRAM)
  - AXP2101 power management IC
  - PCF85063 RTC with backup battery
  - SHTC3 temperature/humidity sensor
  - ES7210/ES8311 audio codec (dual microphone array)
- **Display**: 7.3-inch E Ink Spectra 6 (ACeP 6-Color) E-Paper (800x480 resolution)
- **Power**: 3.7V lithium battery (optional, with onboard charging)

### Pin Configuration

| Function | GPIO Pin |
|----------|----------|
| SPI MOSI | 11       |
| SPI CLK  | 10       |
| CS       | 9        |
| DC       | 8        |
| RST      | 12       |
| BUSY     | 13       |
| I2C SDA  | 47       |
| I2C SCL  | 48       |

## Prerequisites

1. **ESP-IDF v5.x** - Install from https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/get-started/
2. **OpenWeatherMap API Key** - Register at https://openweathermap.org/api to get a free API key

## Quick Start

### 1. Clone and Configure

```bash
# Clone the repository
git clone <repository-url>
cd esp32-s3-photopainter

# Set your WiFi and API credentials using menuconfig
idf.py menuconfig
```

Navigate to **Weather Calendar Configuration** and set:
- **WiFi SSID**: Your WiFi network name
- **WiFi Password**: Your WiFi password
- **OpenWeatherMap API Key**: Your API key
- **Latitude/Longitude**: Your location coordinates
- **Timezone**: Your POSIX timezone string (e.g., `JST-9` for Japan)

### 2. Build and Flash

```bash
# Build the project
idf.py build

# Flash to device (adjust port as needed)
idf.py -p /dev/ttyUSB0 flash

# Monitor serial output
idf.py -p /dev/ttyUSB0 monitor
```

### 3. Expected Behavior

1. Device powers on and initializes components (including AXP2101 power management)
2. Connects to WiFi network
3. Synchronizes time via SNTP
4. Fetches data from APIs (weather, tasks, train status)
5. Renders and displays all information on E-Paper
6. Enters deep sleep for configured interval
7. Wakes up and repeats from step 2

**Note**: Task and train services are optional - the device will continue to operate with graceful degradation if these services are unavailable.

## Configuration Options

All configuration is done via `idf.py menuconfig` under **Weather Calendar Configuration**:

| Option | Default | Description |
|--------|---------|-------------|
| WiFi SSID | - | WiFi network name |
| WiFi Password | - | WiFi password |
| API Key | - | OpenWeatherMap API key |
| Latitude | 35.6762 | Location latitude |
| Longitude | 139.6503 | Location longitude |
| Timezone | JST-9 | POSIX timezone |
| Day Update Interval | 30 | Minutes between updates (6AM-10PM) |
| Night Update Interval | 120 | Minutes between updates (10PM-6AM) |

## Project Structure

```
esp32-s3-photopainter/
├── main/
│   ├── main.c              # Application entry point
│   ├── CMakeLists.txt      # Main component configuration
│   └── Kconfig.projbuild   # Configuration options
├── components/
│   ├── wifi_manager/       # WiFi and SNTP handling
│   ├── epd_driver/         # E-Paper display driver
│   ├── gfx_library/        # Graphics primitives and fonts
│   ├── weather_service/    # OpenWeatherMap API client
│   ├── task_service/       # Task list API client
│   ├── train_service/      # Train status API client (JR East)
│   ├── calendar_ui/        # UI layout and weather icons
│   ├── axpPower/           # AXP2101 power management
│   ├── i2c_bsp/            # I2C driver
│   └── epaper_port/        # E-Paper porting layer
├── sdkconfig.defaults      # Default ESP-IDF settings
├── partitions.csv          # Partition table
└── CMakeLists.txt          # Project configuration
```

## Troubleshooting

### Display Not Updating
- Check BUSY pin connection
- Verify SPI wiring
- Monitor serial output for timeout errors

### WiFi Connection Failed
- Verify SSID and password
- Check WiFi signal strength
- Device retries connection 3 times before using cached data

### Weather Data Not Loading
- Verify API key is valid
- Check internet connectivity
- API calls are throttled to every 3 hours

### Display Shows "Cached" Indicator
- Device is using previously fetched weather data
- This is normal if network is unavailable or API is throttled
- Data remains valid for 24 hours

### Task/Train Data Not Showing
- These services are optional and will gracefully degrade
- Check API endpoint configuration
- Device will continue operating with only weather data

## Power Consumption

The device is optimized for battery operation:
- **Active**: ~150mA during WiFi and display refresh
- **Deep Sleep**: ~10µA
- **Estimated Battery Life**: Several weeks on 2000mAh battery (depending on update frequency)

## License

MIT License - See LICENSE file for details
