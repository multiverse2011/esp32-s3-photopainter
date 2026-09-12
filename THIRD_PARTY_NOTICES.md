# Third-party notices

The project's MIT license covers its own code and documentation. Third-party
materials retain the licenses and copyright notices below. They are not
relicensed by the top-level LICENSE.

## Waveshare PhotoPainter board support

Board-support adaptations in `components/epaper_port/`,
`components/i2c_bsp/`, and `components/axpPower/` use the Waveshare
PhotoPainter sample as a reference/source.

- Source: [waveshareteam/ESP32-S3-PhotoPainter](https://github.com/waveshareteam/ESP32-S3-PhotoPainter).
- License scope: the [01_Example/xiaozhi-esp32 sample license](https://github.com/waveshareteam/ESP32-S3-PhotoPainter/blob/main/01_Example/xiaozhi-esp32/LICENSE), not a blanket claim about every directory in the upstream repository.
- License: MIT; [included text](licenses/Waveshare-MIT.txt).
- Copyright (c) 2025 Shenzhen Xinzhi Future Technology Co., Ltd.
- Copyright (c) 2025 Project Contributors.

Local adaptations include changes to the display transport, timeouts, power
management, and I2C integration. The upstream MIT notice must accompany
redistributed copies or substantial portions of these adaptations.

## XPowersLib

- Local files: `components/axpPower/src/`.
- Source: [lewisxhe/XPowersLib](https://github.com/lewisxhe/XPowersLib).
- License: [MIT](https://github.com/lewisxhe/XPowersLib/blob/master/LICENSE);
  [included text](licenses/XPowersLib-MIT.txt).
- Copyright (c) 2022 lewis he. Individual bundled source headers also contain
  2024 notices; preserve the notices in each file.

## Espressif examples and SDK

`main/hello_world_main.c` and `pytest_hello_world.py` carry
`SPDX-License-Identifier: CC0-1.0` and Espressif copyright notices.
The sample C file is not part of the application's source list.

- Source: [ESP-IDF hello_world example](https://github.com/espressif/esp-idf/tree/v5.5.1/examples/get-started/hello_world).
- [CC0 1.0 text](https://creativecommons.org/publicdomain/zero/1.0/legalcode).
- ESP-IDF itself is a separately installed build dependency with
  [its own licensing](https://github.com/espressif/esp-idf/blob/v5.5.1/LICENSE);
  preserve SDK component notices when distributing firmware.

## Fonts

| Material | Upstream | License text |
|---|---|---|
| Noto Sans JP | [Google Fonts](https://github.com/google/fonts/tree/main/ofl/notosansjp) | [SIL OFL 1.1](custom_components/photopainter/assets/LICENSE-NOTO) |
| IBM Plex Sans / Mono | [IBM/plex](https://github.com/IBM/plex) | [SIL OFL 1.1](custom_components/photopainter/assets/LICENSE-IBM-PLEX) |

The bundled notices identify Adobe (2014–2021, Reserved Font Name "Source")
and IBM Corp. (2017, Reserved Font Name "Plex"), respectively.
The fonts remain under the OFL, including its redistribution and reserved-name
conditions; the project's MIT license does not replace it.

## Material Design icons and webfont

- Source: [Templarian/MaterialDesign-Webfont](https://github.com/Templarian/MaterialDesign-Webfont), version 7.4.47.
- Local materials: `materialdesignicons-webfont.ttf`,
  `materialdesignicons.css`, and `fallback-mdi-masks.json` in
  `custom_components/photopainter/assets/`; also the design mockup assets.
- [Upstream license](https://github.com/Templarian/MaterialDesign-Webfont/blob/master/LICENSE)
  / [included notice](custom_components/photopainter/assets/LICENSE-MDI).
- Fonts and icons: Apache License 2.0, subject to any individual upstream
  icon licenses. The raster fallback masks are derived from the icon font
  and retain its licensing.
- Non-font, non-icon code (including CSS): MIT.
- [Apache License 2.0 text](custom_components/photopainter/assets/LICENSE-APACHE-2.0).

Asset source URLs and hashes are recorded in
[assets/manifest.json](custom_components/photopainter/assets/manifest.json).
Keep the asset license files with distributions of the Home Assistant integration.
