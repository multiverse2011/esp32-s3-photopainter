# ESP32-S3 PhotoPainter

Firmware for the Waveshare ESP32-S3 PhotoPainter and a Home Assistant custom
integration that renders an 800×480 household dashboard. Home Assistant renders
frames; the ESP32 downloads, validates, caches, and displays them.

Home Assistant is the only firmware runtime. Local weather/calendar rendering
and direct OpenWeatherMap, Todoist, and JR East clients have been removed.

## Hardware

- ESP32-S3 PhotoPainter: 16 MB flash, 8 MB octal PSRAM, 7.3-inch e-paper panel.
- AXP2101 power management with optional battery operation.
- SPI: MOSI 11, CLK 10, CS 9, DC 8, RST 12, BUSY 13.
- I2C: SDA 47, SCL 48.

The six-color frame profile is `spectra6-ws73-v1`. Its palette mapping still
requires verification on the physical panel; see `PALETTE_HARDWARE_VERIFIED`
in [const.py](custom_components/photopainter/const.py).

## Home Assistant setup

1. Copy the complete [custom_components/photopainter](custom_components/photopainter)
   directory, including assets and translations, into your Home Assistant
   configuration directory under `custom_components/photopainter/`.
2. Restart Home Assistant and add **PhotoPainter** under
   **Settings → Devices & services → Add integration**.
3. Choose a unique device ID (for example `hall-display`) and timezone.
   Assign up to two calendars, four temperature/humidity pairs (Living, Study,
   Bedroom, Outdoor), and a weather entity supporting hourly forecasts.
4. Configure the day/night schedule and save the generated device key for
   firmware provisioning. Home Assistant stores its hash; the device needs the
   original key.

Missing or unavailable entities display placeholders. The dashboard includes
up to three events per calendar and four forecast slots at three-hour boundaries.
Home Assistant supplies the next connection time. The default day/night intervals
are 30/120 minutes, with a daytime window of 06:00–22:00.

## Build and provision

Use an activated ESP-IDF **v5.5.1** terminal:

```sh
git clone https://github.com/multiverse2011/esp32-s3-photopainter.git
cd esp32-s3-photopainter
idf.py set-target esp32s3
idf.py menuconfig
```

| Menu | Configuration |
|---|---|
| PhotoPainter Configuration | WiFi SSID and password; POSIX timezone for device time |
| PhotoPainter Configuration | Turn off **Disable deep sleep (debug)** for battery operation |
| PhotoPainter Home Assistant client | Home Assistant origin, matching device ID, request timeout |
| PhotoPainter firmware frame cache | Keep the cache enabled with the supplied partition table |

The origin contains only a scheme, host, and optional port (for example
`https://ha.example.com`). Paths, query strings, and redirects are unsupported.
HTTPS uses the ESP certificate bundle and requires time synchronization.
For a trusted network with an HTTP origin such as `http://192.168.1.10:8123`,
explicitly enable **Allow plain HTTP for the Home Assistant origin**.

```sh
idf.py build
idf.py -p PORT flash monitor
```

Replace `PORT` with your serial port, such as `COM5` or `/dev/ttyUSB0`.
On first boot without a stored key, paste the device key into the **UART0**
prompt and press Enter within 60 seconds. The input is not echoed. Firmware
stores the key in NVS namespace `photopainter`, key `ha_key`; a timeout retries
on the next cycle. WiFi, origin, and device ID are build settings, not UART prompts.

The supplied defaults disable deep sleep (`CONFIG_DISABLE_DEEP_SLEEP=y`).
Existing `sdkconfig` values take precedence over `sdkconfig.defaults`.
NVS encryption is not enabled by the defaults. Keep credentials out of commits.
HA supports key rotation; the firmware currently prompts only when `ha_key` is
absent, so replacing a key also requires updating its stored device credential.

## Migrating an existing device

There is no mode selector or `CONFIG_PHOTOPAINTER_HA_MODE` setting anymore.
Rebuilds always use Home Assistant, including builds using an old `sdkconfig`
where HA mode was disabled. Configure the integration, origin, device ID, and
key before expecting the dashboard to update. Old API keys, location settings,
and firmware day/night intervals are no longer used; configure content and
scheduling in Home Assistant instead.

Use the supplied [partitions.csv](partitions.csv) when flashing. It contains a
3 MiB factory application and a 1 MiB `photocache` partition for the A/B frame
cache; older layouts without that partition cannot provide persistent recovery.
There are no OTA slots. Historical designs remain in [specs/](specs/).

## Operation and troubleshooting

The A/B cache retains the last verified frame. Firmware supports cold-boot
redisplay, offline/time-unknown overlays, and durable reports retried after
communication failures. Unchanged frames can skip refreshes; normal refreshes
have a five-minute minimum interval with a cold-boot recovery exception.

HA exposes Pending/Displayed frame images, status sensors, update/connection
binary sensors, and a **Regenerate display** button. Displayed frame reflects a
successful device report. Regenerate display prepares the next frame and does
not wake a sleeping device.

| Symptom | Check |
|---|---|
| No HA dashboard | WiFi, origin, matching device ID, and UART key provisioning |
| HTTPS failure | DNS, certificate trust, and SNTP synchronization |
| HTTP rejected | Explicit plain-HTTP option |
| Authentication failure | Device ID/key pairing, including key rotation |
| Panel unchanged | Next connection time, Last error, Last seen, and display reports |
| Cache not restored | Cache configuration and `photocache` partition |
| Panel refresh failure | BUSY/SPI wiring and serial logs |
| Missing content | Assigned entities and hourly forecast support |
| Device stays awake | Disable the debug deep-sleep override |

Physical palette, panel, power, and network behavior require hardware testing.

## Development and tests

```sh
python -m pip install Pillow==12.3.0 tzdata
python -m unittest discover -s tests/photopainter -p "test_*.py"
```

Run the HA adapter smoke test in the target image (POSIX shell):

```sh
docker run --rm --entrypoint python -v "$PWD:/project" -w /project ghcr.io/home-assistant/home-assistant:2026.9.1 tests/photopainter/ha_smoke.py
```

See [tests/firmware/README.md](tests/firmware/README.md) for native C policy tests.
These tests do not replace an ESP-IDF build or hardware verification.

| Path | Purpose |
|---|---|
| [main/](main/) | Home Assistant display runtime |
| [custom_components/photopainter/](custom_components/photopainter/) | HA configuration, collection, rendering, API, and entities |
| [components/ha_frame_client/](components/ha_frame_client/) | HTTP client and recovery policy |
| [components/frame_store/](components/frame_store/) | Persistent A/B frame cache |
| [components/epd_driver/](components/epd_driver/) | Panel driver |
| [components/](components/) | WiFi/SNTP, power, I2C, and display support |

Bundled font and icon licenses are in
[assets/](custom_components/photopainter/assets/). Consult individual source
notices for other bundled code.
