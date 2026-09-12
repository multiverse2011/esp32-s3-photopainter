# ESP32-S3 PhotoPainter

[English](README.md) | 日本語

カレンダー・天気予報・温湿度を電子ペーパーに表示する、Waveshare ESP32-S3 PhotoPainter 用ファームウェアです。
Home Assistant のカスタム統合を同梱しており、表示内容や更新間隔を Home Assistant から設定できます。

## 主な機能

- 2つのカレンダーの予定を表示（各3件まで、日本語対応）
- リビング・書斎・寝室・屋外の温湿度を表示
- 3時間ごとの天気予報を4件表示
- 昼間・夜間で更新間隔を設定
- 表示画像のプレビュー、バッテリー残量の確認
- オフライン時のキャッシュ表示

## 動作環境

- Waveshare ESP32-S3 PhotoPainter（7.3インチ、800×480）
- Home Assistant
- ESP-IDF v5.5.1（ファームウェアのビルド用）

Home Assistant に、表示したいカレンダー・温湿度センサー・時間別予報に対応した天気エンティティを用意してください。

## インストール

### 1. Home Assistant に統合を追加する

[custom_components/photopainter](custom_components/photopainter) を、Home Assistant の設定ディレクトリにある `custom_components/` 以下へコピーします。

Home Assistant を再起動し、**設定 → デバイスとサービス → 統合を追加** から **PhotoPainter** を選択します。
画面に従ってデバイス ID、エンティティ、タイムゾーン、更新間隔を設定し、発行されたデバイスキーを控えておきます。

### 2. ファームウェアを設定する

ESP-IDF の環境を有効にしたターミナルで、以下を実行します。

```sh
git clone https://github.com/multiverse2011/esp32-s3-photopainter.git
cd esp32-s3-photopainter
idf.py set-target esp32s3
idf.py menuconfig
```

menuconfig で次の項目を設定します。

| メニュー | 設定内容 |
|---|---|
| PhotoPainter Configuration | WiFi の SSID・パスワード、タイムゾーン |
| PhotoPainter Home Assistant client | Home Assistant origin、統合で設定したデバイス ID |

**Home Assistant origin** には `https://ha.example.com` のように、パスを含めずに接続先を指定します。
HTTP を使う場合は **Allow plain HTTP for the Home Assistant origin** を有効にしてください。HTTPS では、証明書の検証と時刻同期が必要です。

バッテリーで使用する場合は **Disable deep sleep (debug)** を無効にしてください。初期設定ではスリープしません。

### 3. 書き込み

`PORT` を接続先のシリアルポート（`COM5`、`/dev/ttyUSB0` など）に置き換えて実行します。

```sh
idf.py build
idf.py -p PORT flash monitor
```

初回起動時、UART0 のシリアルモニターにキーの入力案内が表示されます。
60秒以内にデバイスキーを貼り付けて Enter を押すと、設定が保存されます。

## 使い方

初期設定では、6:00〜22:00 は30分ごと、それ以外は2時間ごとに更新します。
更新間隔は Home Assistant の統合オプションから変更できます。

| Home Assistant の項目 | 内容 |
|---|---|
| Pending frame | 次回表示する画像 |
| Displayed frame | デバイスから表示成功の報告を受けた画像 |
| Regenerate display | 次回表示する画像を再生成 |
| Last seen / Last error | 最終通信時刻・エラー情報 |

画像の再生成は、次回のデバイス接続時に反映されます。
画面が更新されない場合は、通信状態とシリアルログを確認してください。

## ライセンス

[MIT License](LICENSE)

同梱ライブラリ・フォント・アイコンのライセンスは [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) を参照してください。
