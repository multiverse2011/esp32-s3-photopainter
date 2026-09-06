# PhotoPainter Device API v1（新設予定）

この契約のAPIはまだHAに実装されていない。HA標準のREST APIと混同しない。基底pathは`/api/photopainter/v1/devices/{device_id}`、同一HA originを使う。

## 認証と共通制約

全要求に`Authorization: Bearer <DEVICE_KEY>`を付ける。これはHAユーザーの長期トークンではなく、PhotoPainter専用ランダムキー。device IDとの組を定数時間で検証する。管理APIへの利用は拒否。401/403時に画像/設定を返さない。

HTTPS既定、HTTPは明示許可時のみ。redirectは追従しない。端末はmanifest内のframe pathが同一originかつ自身の固定prefixであることを検証する。任意URL/任意pathを取得しない。レスポンスでsecretを返さない。

manifest上限8KiB、report上限4KiB、frame固定192000 bytes。圧縮されたレスポンスはMVPでは使用せず、`Accept-Encoding: identity`。Content-Lengthがあっても実受信長で上限確認する。JSONの未知フィールドは無視、必須欠落/型違い/未対応major versionは拒否。

## GET /manifest

毎回200と小さなJSONを返し、予定時刻と再表示要否を更新できる。manifest自体に304を使わない。画像の生成はこのGETだけを理由に毎回実行しない。

```json
{
  "schema_version": 1,
  "device_id": "hall-display",
  "server_time": "2026-09-06T04:30:00Z",
  "frame": {
    "id": "<64 lowercase hex SHA-256>",
    "path": "/api/photopainter/v1/devices/hall-display/frames/<sha256>",
    "width": 800,
    "height": 480,
    "format": "packed4",
    "palette_id": "spectra6-ws73-v1",
    "byte_length": 192000,
    "sha256": "<64 lowercase hex SHA-256>",
    "generated_at": "2026-09-06T04:29:50Z",
    "fresh_until": "2026-09-06T05:00:00Z",
    "status_overlay": {"x": 344, "y": 24, "width": 432, "height": 40}
  },
  "schedule": {
    "next_poll_at": "2026-09-06T05:00:00Z",
    "retry_after_seconds": 1800,
    "min_refresh_seconds": 300
  },
  "redisplay_required": false
}
```

`<sha256>`部分は説明用プレースホルダー。fixtureでは実画像から計算した値を使う。SHAとidは同じ値。端末はサイズ/形式/パレット/上部状態欄範囲を検証してから画像を要求する。

`redisplay_required`はサーバーのACK検証不能/管理者の強制再表示に用いる。同一フレームでもこれがtrueなら再表示し、成功ACK後に解除。常に最小更新間隔を守る。

未生成は503、Retry-Afterを付ける。利用可能な前世代があれば古いgenerated_at/fresh_untilのまま200。未同期端末は接続成功時のserver_timeでソフト時刻を補えるが、HTTPS証明書検証を無効化しない。証明書検証に必要な初期時刻はSNTP/RTCで得る。

## GET /frames/{sha256}

200、`Content-Type: application/octet-stream`、`Content-Length: 192000`、`ETag: "{sha256}"`、`Cache-Control: private, max-age=86400, immutable`。正しいIf-None-Matchには304も可。端末は同じIDを保存済みならそもそもGETを省略できる。404/410ならmanifestを1回取り直す（起床予算内）。

生の4bppのみ。ヘッダー/BMP/PNGは付けない。行は上から下、各行左から右、偶数xが上位nibble、奇数xが下位。行paddingなし。400 bytes/行×480行。

候補パレットはblack=0x0、white=0x1、yellow=0x2、red=0x3、blue=0x5、green=0x6。0x4/0x7–0xFは拒否。**色票による実機確認が完了するまでこの符号表は暫定**。確認後にpalette IDと共に固定し、異なるパネルprofileを同じIDで配布しない。

受信完了→長さ検証→SHA検証→色コード走査→キャッシュcommit→パネル更新を順番に行う。未検証bytesをパネルへ直接ストリームしない。認証された通信であっても破損/過大データを拒否する。SHAは整合性検証でありHTTP上の改ざん防止の代替ではない。

## POST /reports

端末の状態と表示ACK。認証された自身の状態だけを更新する。HA家電のstate/serviceへ転送しない。

```json
{
  "schema_version": 1,
  "report_id": "boot-a13-seq-2",
  "boot_id": "boot-a13",
  "firmware_version": "0.1.0",
  "received_frame_id": "<sha256>",
  "displayed_frame_id": "<sha256>",
  "display_result": "success",
  "display_completed_at": "2026-09-06T04:30:40Z",
  "local_overlay": "none",
  "battery_percent": null,
  "wifi_rssi_dbm": -58,
  "next_wake_at": "2026-09-06T05:00:00Z",
  "error_code": null
}
```

`display_result`: success/skipped/failed。失敗時はdisplayed_frame_idを前の成功IDまたはnullとし、received IDだけを新世代へ進める。skipは前の成功を再報告する意味であり、新しい画像成功にしない。電池値は計測不可ならnull。local_overlayはnone/offline/time_unknownで、overlayの最終取得日時も別の`overlay_source_time`として送れる。

成功200 `{"accepted":true,"report_id":"..."}`。同じreport IDの再送も200。表示成功/overlay/次回起床は不揮発で再送可能にする。古いACKは新しい表示世代を巻き戻さない。サーバー側の既知配信世代と照合し、不明な世代には409を返す。端末は409後にmanifestを再取得して再表示を予約する。

失敗HTTP: 400/413/422（入力不正）、401/403（認証）、404（対象なし）、409（世代不明/矛盾）、429（レート制限）、503（一時不可）。詳細にsecret/body全体を反映しない。

## 上部状態欄契約

上部バー右側のx=344/y=24/w=432/h=40を白地で予約し、最終取得日時と次回予定を16px以上で表示する。下端に情報欄は置かない。通常はHAが組版。通信断時は端末がキャッシュの上部予約領域だけをメモリ上で再描画し、全画面更新する。通常ラベルはUpdated / Next、障害時はOffline / Last / Time not syncedを使う。端末側は既存ASCII描画を再利用できる。年月日が長い場合や時計未同期は予約領域内の2行で表示する。[Layout Bの上部バー仕様](../selected-layout.md)に従う。予定本文の日本語fallbackはHA側で処理する。

元frame bytesのSHAをoverlay後のbytesへ流用しない。元frameは変更せず、表示bufferに合成する。reportのlocal_overlayと日時によりHA側も同一の最終表示プレビューを再現する。色プロファイル/日時字形/配置はgolden fixtureで共有する。

## HAユーザー向けentity契約

| 種類/末尾 | 振る舞い |
|---|---|
| sensor `last_seen` | HAが最後に端末通信を受信した時刻 |
| sensor `last_displayed` | 最後の成功ACK時刻。端末時刻不明なら属性で区別 |
| sensor `battery` / `wifi_signal` | 実測値またはunknown。端末電池とiPhone電池を混ぜない |
| sensor `next_wake` / `last_error` | 起床予定/機械判定可能なコード |
| binary_sensor `update_pending` | 最新配信ID/再表示要求がまだACKされていない |
| binary_sensor `connection_delayed` | 次回予定＋10分経過、または初期登録から10分で未接続 |
| button `regenerate` | 再生成を予約。説明「次回接続で反映」。即時物理更新ではない |
| image `pending_frame` / `displayed_frame` | 配信予定とACK済み画面のPNGプレビュー |

IDはdevice ID由来のstable unique_idを持たせる。platform/entity表示名は英語を既定として必要に応じて日本語translationsを用意する。画像プレビューはHAの認証されたImage entity経路を利用し、`/local/`へ私有フレームを公開しない。
