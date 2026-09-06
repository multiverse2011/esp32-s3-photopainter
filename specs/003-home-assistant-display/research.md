# 実装の根拠と確認事項

確認基準日: 2026-09-06。実装に関係するコードとHome Assistantの読取結果。パネル描画・電池・実フラッシュ容量の実測は未完了。

## 既存ファームウェア

| 箇所 | 確認した実装 | 必要な対応 |
|---|---|---|
| `main/CMakeLists.txt` | エントリは`main/main.c` | HAフレーム取得をこの状態機械へ接続 |
| `main/main.c` | Wi-Fi→SNTP→情報取得→描画→deep sleep。天気にはRTC/NVSと3時間の取得抑制 | HAの更新予定・成功判定を独立させる |
| `components/calendar_ui/calendar_ui.c` | 800×480のASCII中心のローカル組版 | Layout BはHA側で組版する |
| `components/gfx_library/gfx_paint.c` | 0x20–0x7E以外は空白。16/24/32pxフォント | 通常画面は画像として受信し、ローカルの状態欄にASCII描画を使う |
| `components/epd_driver/include/epd_driver.h` | 800×480、4bpp、192000 bytes。7色enumにORANGE=4 | 6色パネルの符号を色票で固定 |
| `components/epd_driver/epd_driver.c` | PSRAM確保。下位表示呼出後に常にESP_OKを返す | 表示成功ACKのためにエラーを伝播 |
| `components/epaper_port/epaper_port.c` | BUSY待ちは無期限、SPI失敗はassert | 有限タイムアウトと失敗経路を追加 |
| 同上 / `epd_driver_sleep` | 全画面更新後にPOWER_OFF。sleepラッパーはno-op | 実際の電源停止と消費電力を測定 |
| `sdkconfig.defaults` | Octal PSRAM、フラッシュ設定2MB | README記載の16MBとの不一致を実機確認 |
| `partitions.csv` | NVS24KiB、factory約1.875MiB | 実容量確認後、A/B画像保存領域を設計 |

[メーカー資料](https://www.waveshare.net/wiki/ESP32-S3-PhotoPainter)は800×480・6色・更新約25秒を示す。更新時間と消費電力は実測して受入値を確定する。パネルの照合先は[Waveshare公式コード](https://github.com/waveshareteam/ESP32-S3-PhotoPainter)。

## Home Assistantのデータ源

接続先は`http://homeassistant.home.arpa`。Core 2026.9.1、TZはAsia/Tokyo。温湿度の部屋割当はentity/device/area registryで確認した。

| 表示 | entity |
|---|---|
| Living 温度 / 湿度 | `sensor.0xa4c13818925dffff_temperature` / `sensor.0xa4c13818925dffff_humidity` |
| Study 温度 / 湿度 | `sensor.snzb_02b_01_temperature` / `sensor.snzb_02b_01_humidity` |
| Bedroom 温度 / 湿度 | `sensor.snzb_02b_03_temperature` / `sensor.snzb_02b_03_humidity` |
| Outdoor 温度 / 湿度 | `sensor.snzb_02b_04_temperature` / `sensor.snzb_02b_04_humidity` |
| 時間別予報 | `weather.wu_wai_forecast_home`（Met.no） |
| Star / Moonの予定 | 未割当。設定時に各1つのcalendar entityを指定 |

`weather.get_forecasts`の`type: hourly`は48件の取得に成功。取得アクションのchanged_statesは0件。温度単位はentityから読み、HA全体のunit systemと同じとは仮定しない。

`calendar.*`は確認時点のstate一覧に存在せず、`GET /api/calendars`は404。2人のカレンダーの連携元とentity IDは実装前に確認する。仮のentityや実際の個人予定をfixtureへ書かない。

認証付きのstate/service/registry読取を調査に使用した。認証値、位置情報、個人予定、API全応答は資料に保存しない。これらの読取結果は、製品用HA統合・端末通信・実機表示の検証を意味しない。

## 描画方式の根拠

HA側の専用レンダラーで英語/日本語・MDIアイコン・6色変換を一括処理し、端末は固定長画像を取得する。これにより端末のASCIIフォント制約を通常画面から切り離し、HAで配信予定と表示済み画像を確認できる。画面はHAのデータから生成し、LovelaceのUI構造へ依存しない。

[Home Assistantのアイコン仕様](https://www.home-assistant.io/docs/frontend/icons/)はMDIを使用する。所有者と天候に使うMDI名は`@mdi/font` 7.4.47への収録を確認済み。実機での最終パレットと可読性は未検証。

## APIの一次資料

- [REST API](https://developers.home-assistant.io/docs/api/rest/): state、取得アクションの応答、カレンダー期間取得。
- [WebSocket API](https://developers.home-assistant.io/docs/api/websocket/): 認証と読取要求。
- [Weather](https://www.home-assistant.io/integrations/weather/): hourly予報とcondition。
- [Image entity](https://developers.home-assistant.io/docs/core/entity/image/): 認証された画像プレビュー。
- [Integration manifest](https://developers.home-assistant.io/docs/creating_integration_manifest/) / [データ取得](https://developers.home-assistant.io/docs/integration_fetching_data/): HA統合と非同期処理。

## 実装前に確定する項目

calendar割当、実パネル型番/色コード、物理フラッシュ容量、画像partition、PMIC残量API、HA 2026.9.1と整合する描画ライブラリの版・ライセンスを確認する。性能と実機受入の基準は[spec.md](spec.md)。
