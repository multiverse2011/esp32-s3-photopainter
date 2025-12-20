# Research: E-ink Display Design Improvement

**Feature**: 002-improve-display-design
**Date**: 2025-12-20

## Research Topics

### 1. OpenWeatherMap 3時間予報API

**Question**: 更新時間から3時間間隔で5つの時間帯の天気を取得する方法

**Decision**: OpenWeatherMap 5 Day / 3 Hour Forecast APIを使用

**Rationale**:
- 既存プロジェクトでOpenWeatherMap APIを使用済み
- 3時間間隔の予報データが取得可能（40データポイント = 5日分）
- 無料プランで1000回/日のAPI呼び出しが可能

**API Endpoint**:
```
https://api.openweathermap.org/data/2.5/forecast?lat={lat}&lon={lon}&appid={API_KEY}&units=metric
```

**Response Structure** (relevant fields):
```json
{
  "list": [
    {
      "dt": 1702281600,
      "main": {
        "temp": 5.0,
        "humidity": 95
      },
      "weather": [{"icon": "01d", "description": "clear sky"}],
      "wind": {"speed": 2.0, "deg": 315}
    }
  ]
}
```

**Implementation**:
- 現在時刻に最も近い予報から5つを選択
- 3時間間隔なので、0, 3, 6, 9, 12時間後の予報を取得

**Alternatives Considered**:
- One Call API 3.0: 有料サブスクリプション必要
- Weather API (weather.com): ESP32向けライブラリなし

---

### 2. タスクAPI統合

**Question**: Google Calendar または Todoist からタスクを取得する方法

**Decision**: Google Calendar API を第一候補、Todoist APIをフォールバック

**Rationale**:
- Google Calendar: 多くのユーザーが既に使用、イベントをタスクとして表示可能
- Todoist: シンプルなタスク管理に特化、APIが使いやすい

#### Google Calendar API

**Authentication**: OAuth 2.0 Service Account または API Key（読み取り専用）

**Endpoint**:
```
https://www.googleapis.com/calendar/v3/calendars/{calendarId}/events?
  timeMin={today_start}&
  timeMax={today_end}&
  maxResults=5&
  singleEvents=true&
  orderBy=startTime
```

**ESP32制約**:
- OAuth 2.0フローはESP32では複雑
- API Keyでの公開カレンダーアクセスが最もシンプル
- または: プロキシサーバー経由でトークン管理

#### Todoist API

**Endpoint**:
```
https://api.todoist.com/rest/v2/tasks?filter=today
```

**Header**: `Authorization: Bearer {API_TOKEN}`

**Response**:
```json
[
  {
    "id": "123",
    "content": "ゴミ出し",
    "due": {"date": "2025-12-20"},
    "priority": 4
  }
]
```

**Implementation Decision**:
- 初期実装: Todoist API（シンプルなBearer Token認証）
- 将来拡張: Google Calendar対応

**Alternatives Considered**:
- ローカルファイル/NVS: ユーザーがタスクを更新できない
- Apple Reminders: macOS/iOSのみ、API非公開

---

### 3. JR東日本運行情報スクレイピング

**Question**: https://traininfo.jreast.co.jp/train_info/kanto.aspx から遅延情報を取得する方法

**Decision**: HTTPSでHTMLを取得し、シンプルな文字列パースで遅延情報を抽出

**Target URL**: `https://traininfo.jreast.co.jp/train_info/kanto.aspx`

**ページ構造分析**:
- ページは路線ごとの運行状況テーブルを含む
- 「平常運転」= 遅延なし
- 「遅延」「運転見合わせ」= 遅延あり

**パース戦略**:
```c
// 1. HTTPSでページ全体を取得
// 2. 特定路線名を検索（例: "中央線"）
// 3. 近接する「平常運転」または「遅延」を検出
```

**実装上の注意**:
- HTMLが大きい（数十KB）ため、ストリーミングパースが望ましい
- 文字コード: Shift_JIS → UTF-8変換が必要
- エラーハンドリング: ページ構造変更時のフォールバック

**Alternatives Considered**:
- 鉄道遅延情報のJSON API (rti-giken.jp): 無料だが信頼性不明
- Yahoo!路線情報: 同様にスクレイピング必要、ページ構造がより複雑

---

### 4. 2カラムレイアウト実装

**Question**: 800x480の画面で最適なレイアウト分割

**Decision**: 左サイドバー200px、右コンテンツ600px

**Layout Design**:
```
+------------------+----------------------------------------+
|                  |  Weather                               |
|   10             |  +------+------+------+------+------+  |
|   (huge)         |  |6:00  |9:00  |12:00 |15:00 |18:00 |  |
|                  |  |☀️    |⛅    |🌧️    |🌧️    |☀️    |  |
|   December       |  |5°C   |8°C   |10°C  |3°C   |2°C   |  |
|                  |  |95%   |70%   |30%   |20%   |15%   |  |
|   Tasks          |  +------+------+------+------+------+  |
|   • ゴミ出し      |----------------------------------------|
|   • 買い物        |  Train                                 |
|                  |  Not delayed                           |
|                  |----------------------------------------|
|                  |                    Updated 2025/12/20  |
+------------------+----------------------------------------+
   200px                          600px
```

**Font Sizes**:
- 日付（大）: 独自大フォント（約100px相当、複数フォントで構成）
- 月名: 24px
- 時刻ラベル: 16px
- 気温: 24px
- 湿度/風速: 16px
- タスク: 16px
- Train状態: 24px
- Updated: 16px

**Color Usage**:
- 日付: 赤アウトライン + 黒塗り（スケッチ参考）
- 晴れアイコン: 黄色/オレンジ
- 雨アイコン: 青
- 曇りアイコン: 黒アウトライン + 白塗り
- 遅延あり: 赤
- 遅延なし: 緑

---

### 5. 大きな日付数字の描画

**Question**: 既存フォント（16/24/32px）で大きな日付を表現する方法

**Decision**: 複数の矩形と線で数字を構成するセグメントディスプレイ風描画

**Rationale**:
- 新フォント追加はスコープ外
- セグメントディスプレイ風なら既存プリミティブ（矩形、線）で描画可能
- カスタム数字描画関数を実装

**Implementation**:
```c
// 7セグメント風の大きな数字描画
void draw_large_digit(uint16_t x, uint16_t y, int digit, uint16_t height);
void draw_large_number(uint16_t x, uint16_t y, int number, uint16_t height);
```

**Alternative Considered**:
- ビットマップフォント: フラッシュ容量消費、実装コスト高
- 既存フォントの拡大: ジャギーが目立つ

---

## Summary of Decisions

| Topic | Decision | Key Reason |
|-------|----------|------------|
| 天気API | OpenWeatherMap 3時間予報 | 既存実装あり、無料枠十分 |
| タスクAPI | Todoist API（初期） | シンプルなBearer認証 |
| 電車遅延 | JR東日本ページスクレイピング | ユーザー指定、公式ソース |
| レイアウト | 200px + 600px 2カラム | スケッチに基づく |
| 大きな数字 | セグメント風カスタム描画 | 新フォント不要 |
