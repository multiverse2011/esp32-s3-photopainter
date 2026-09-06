# 3時間刻みの天気予報

Layout BのForecastに時刻・天候・気温を4枠表示する。配置は[画面仕様](selected-layout.md)。

## データ取得

- 初期候補は`weather.wu_wai_forecast_home`（Met.no）。hourly 48件の取得を確認済み。根拠は[research.md](research.md)。
- HAの`weather.get_forecasts`へ`type: hourly`と対象entityを指定し、応答の対象entityにある`forecast`配列を読む。現在のweather stateだけでは時間別予報にならない。[HA公式Weather](https://www.home-assistant.io/integrations/weather/)
- 日次予報だけのentityは選択時にhourly非対応と示す。実行時に非対応へ変われば予報だけを`Forecast unavailable`とする。未設定は`Weather not linked`。
- 各項目の`datetime`、`condition`、`temperature`とentityの`temperature_unit`を使う。offsetのない日時や非数値/NaN/Infinityは不正として扱う。°Fは°Cへ変換し、未知単位は気温を欠損扱いにする。
- 表示モデルは時刻・天候・気温のみ。降水確率/降水量/風速を追加しない。

## 時刻の選択

1. 生成時刻を表示TZ（初期Asia/Tokyo）へ変換する。
2. 当日と翌日のローカル00/03/06/09/12/15/18/21時から、生成時刻以上の最初の境界を選ぶ。境界ちょうどならその枠を含める。
3. その境界から続く4枠を表示する。JSTでは3時間間隔。ラベルは`HH:mm`、翌日の枠に`+1d`を添える。
4. 応答をUTCの日時で正規化/ソートし、各境界と同じ瞬間の項目を選ぶ。配列の先頭から3件ごとに取り出したり、近い時刻の値を移したり、補間したりしない。
5. 同一瞬間の重複があれば完全一致は除去する。矛盾する値は該当フィールドを欠損扱いとし、恣意的に先頭を選ばない。
6. キャッシュも元の有効日時で照合する。新しい時刻ラベルを古い値へ付けない。期限内でも対応する項目がなければ欠損とする。

| 生成時刻（JST） | 表示時刻 |
|---|---|
| Sep 06 13:30 | 15:00 / 18:00 / 21:00 / 00:00 +1d |
| Sep 06 15:00 | 15:00 / 18:00 / 21:00 / 00:00 +1d |
| Sep 06 15:01 | 18:00 / 21:00 / 00:00 +1d / 03:00 +1d |
| Sep 06 23:59 | 00:00 +1d / 03:00 +1d / 06:00 +1d / 09:00 +1d |
| Sep 07 00:00 | 00:00 / 03:00 / 06:00 / 09:00 |

UTC `2026-09-06T06:00:00Z`はJSTの15:00に対応する。00:00/翌日ラベルの基準はフレーム生成時点の日付。DSTを使うTZは追加検証が必要で、上記例はJSTの受入基準とする。

3時間は予報の有効時刻の刻みであり、取得周期や画面更新周期ではない。既定の昼30分/夜120分の更新時に予報を読み直し、HA/提供元の更新頻度を超えるリアルタイム性は想定しない。

## 描画

x=394/y=324/w=382/h=132の領域。見出し`FORECAST`、右端に`3-HOURLY · °C`。4列に時刻16px、天候32px、気温24pxを配置する。気温は整数へ四捨五入（負数のちょうど半分は絶対値を大きくする）、負のゼロを0へ正規化する。例: `27°`、`−3°`。元の精度はデータモデルへ保持する。

MDIの単色字形をHA側でラスタライズする。所有者マークは黒白とし、天候には下記のパネル色を使う。文字/字形をOSのカラー絵文字へfallbackしない。字形の収録/ライセンス/最終パレットをT007で検証し、必要な字形の固定マスクを同梱する。色だけに依存せず形でも区別する。

| HA condition | MDI名（mdi:接頭辞） | 色 |
|---|---|---|
| sunny / clear-night | weather-sunny / weather-night | 赤 / 青 |
| partlycloudy（昼/夜） | weather-partly-cloudy / weather-night-partly-cloudy | 黒 / 青 |
| cloudy | weather-cloudy | 黒 |
| rainy / pouring | weather-rainy / weather-pouring | 青 |
| snowy / snowy-rainy | weather-snowy / weather-snowy-rainy | 青 |
| lightning / lightning-rainy | weather-lightning / weather-lightning-rainy | 赤 |
| fog | weather-fog | 黒 |
| windy / windy-variant | weather-windy / weather-windy-variant | 黒 |
| hail | weather-hail | 青 |
| exceptional | alert-circle-outline | 赤 |
| 未知の非空condition | `?` | 黒 |
| condition欠損 / 時刻項目なし | `—` | 黒 |

上記MDI名は@mdi/font 7.4.47に収録されることを確認済み。夜のpartlycloudyは予報時点の昼夜情報を優先し、なければHAの設定位置とTZによる太陽高度計算を使う。判定できなければweather-cloudy＋`P`（16px）。現在のsun stateを未来へ流用しない。アイコン全数の最終パレット/実機確認はT007に残す。

## 欠損と鮮度

- 温度のみ欠損なら天候は残す。天候のみ未知なら`?`とし、温度は残す。一枠全体の欠損は時刻＋`—`＋`—`。
- 4枠すべて欠ける場合は`Forecast unavailable`。失敗した処理の原因はHA診断へ保持し、画面に実装用エラーコードを載せない。
- リクエスト失敗時、最終成功から6時間未満のキャッシュに対象時刻があれば使用できるが、見出し右側を`Stale · °C`にする。キャッシュ使用時は失敗直後からStale。6時間以上なら予報値を隠す。
- 再生成でキャッシュのcollected_atを更新しない。source_issued_atは提供された場合のみ保持する。正常なAPI応答は提供元予報の更新を意味しない。
- 予報が古い場合の共通上部状態欄は`Stale · <最も古い表示情報の取得日時>`へ切り替える。次回更新は右側に残す。幅が不足すれば取得日時を`Sep 06 13:30`形式に短縮し、別のAs of行を増設しない。
- 予報だけの失敗はカレンダー/室内温湿度を止めない。全体が通信断なら既存のフレーム/障害上部状態欄の契約を使う。

## 受入

時刻境界、UTC/JST、日付跨ぎ、順不同、重複矛盾、1枠欠損、4枠欠損、未知天候、hourly非対応、°Fと負の温度、昼夜判定、6時間失効、Stale時の上部状態欄幅をfixtureで確認する。800×480の最終4bppで全アイコン・長い負温度・+1dが枠に収まり、画面とHAの取得対象が仕様の範囲内であることを確認する。
