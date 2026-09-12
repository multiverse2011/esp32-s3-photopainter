# ESP32-S3 PhotoPainter

English | [日本語](README.ja.md)

A calendar, weather, and room climate display for the Waveshare ESP32-S3 PhotoPainter.
The included Home Assistant integration lets you choose what to display and set the update schedule.

## Features

- Two calendars with up to three events each, including Japanese text
- Temperature and humidity for the living room, study, bedroom, and outdoors
- Four weather forecasts at three-hour intervals
- Separate day and night update schedules
- Image previews and battery status
- Cached display when offline

## Requirements

- Waveshare ESP32-S3 PhotoPainter (7.3-inch, 800×480)
- Home Assistant
- ESP-IDF v5.5.1 to build the firmware

Set up the calendar, temperature/humidity, and hourly weather entities you want to display in Home Assistant.

## Installation

### 1. Add the Home Assistant integration

Copy [custom_components/photopainter](custom_components/photopainter) into `custom_components/` in your Home Assistant configuration directory.

Restart Home Assistant, then select **PhotoPainter** under **Settings → Devices & services → Add integration**.
Follow the setup form to choose a device ID, entities, timezone, and update schedule. Save the generated device key.

### 2. Configure the firmware

Run the following in a terminal with the ESP-IDF environment activated:

```sh
git clone https://github.com/multiverse2011/esp32-s3-photopainter.git
cd esp32-s3-photopainter
idf.py set-target esp32s3
idf.py menuconfig
```

Configure these menus:

| Menu | Settings |
|---|---|
| PhotoPainter Configuration | WiFi SSID, password, and timezone |
| PhotoPainter Home Assistant client | Home Assistant origin and the device ID from the integration |

Set **Home Assistant origin** to your server address without a path, such as `https://ha.example.com`.
For HTTP, enable **Allow plain HTTP for the Home Assistant origin**. HTTPS requires certificate verification and time synchronization.

For battery operation, turn off **Disable deep sleep (debug)**. Sleep is disabled by default.

### 3. Flash the device

Replace `PORT` with your serial port, such as `COM5` or `/dev/ttyUSB0`:

```sh
idf.py build
idf.py -p PORT flash monitor
```

On first boot, the UART0 serial monitor prompts for the device key.
Paste it and press Enter within 60 seconds to save it on the device.

## Usage

The default schedule updates the display every 30 minutes from 06:00 to 22:00 and every two hours otherwise.
Change the schedule in the Home Assistant integration options.

| Home Assistant entity | Purpose |
|---|---|
| Pending frame | Image prepared for the next update |
| Displayed frame | Image the device has confirmed displaying |
| Regenerate display | Generate a new image for the next update |
| Last seen / Last error | Last contact time and error information |

Regenerated images are applied when the device next connects.
If the screen does not update, check the connection status and serial log.

## License

[MIT License](LICENSE)

See [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) for the licenses of bundled libraries, fonts, and icons.
