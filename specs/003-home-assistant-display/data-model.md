# Data Model

## DisplayConfig

| Field | 型/制約 | 意味 |
|---|---|---|
| device_id | 小文字英数字と`-`、1–64文字 | 安定ID。実MACをそのまま公開する必要はない |
| template_id | enum、初期`home_duo` | Layout Bの固定テンプレート。実行時のレイアウト切替は設けない |
| timezone | IANA文字列 | 初期Asia/Tokyo。端末初期はJST、一般DST対応は別途検証 |
| bindings | 用途→entity ID | rooms最大4、weatherはhourly対応1つ、calendarsは下記2人分 |
| calendars | 固定2枠 | owner_id=`calendar_1`/`calendar_2`、display_name、calendar_entity_id（未設定はnull）、icon_id（Star: mdi:star-four-points / Moon: mdi:moon-waning-crescent） |
| ui_locale | 初期`en` | 固定ラベルは英語。予定タイトルを翻訳しない |
| display_names | 最大40コードポイント/値 | 表示時に描画幅で省略 |
| day/night_interval_minutes | 5–120 / 30–360 | 初期30/120。パネル最小300秒の制約を常に優先 |
| day_start/day_end | 時刻 | 初期06:00/22:00 |
| device_key_hash | secret hash | 診断出力ではredact |

## DisplaySnapshot

version、snapshot ID、generated_at（UTC）、表示TZ、日付、各Sectionで構成。個別値は`value`/`unit`/`status`/`collected_at`/`source_updated_at`/`observed_at?`を持つ。

`status`は`ok`、`stale`、`unavailable`、`unconfigured`。`null`はunknownを表し、0へ変換しない。数値のNaN/Infinityは拒否。状態文字列は既知語へ正規化し、未知語は「状態不明」。

| Section | 件数/内容 | 鮮度ルールの初期値 |
|---|---|---|
| calendars | Star / Moon各3件、残件数、当日の日付 | 取得失敗時のキャッシュにはStaleを付ける。最終成功から60分を鮮度期限、6時間を保存値の表示上限とする。日付が変われば非表示 |
| rooms | 3部屋＋屋外、温度/湿度/場所 | 取得失敗時のキャッシュにはStaleを付ける。最終成功から60分を鮮度期限、6時間を表示上限とする。元entity unavailableは即反映 |
| forecast | weather entity、3時間刻み4枠の有効日時/condition/気温°C/状態 | 失敗してキャッシュを使う場合は即stale、最終成功から6時間以上で非表示。枠の元日時を変えない |

成功空/未設定/取得失敗は所有者ごとに区別する。通信失敗直後からキャッシュにStaleを添え、取得日時を上部状態欄に表示する。取得失敗が未確定でも鮮度期限を超えた保存値はstale扱いにする。UI文字列は`No events today` / `Calendar not linked` / `Unavailable` / `Stale`。電力・買い物・プラグ・積算・料金のfieldは本MVPへ追加しない。

これは**HAからの取得鮮度**のルール。HAが実際のセンサーを受信した時刻の保証ではない。sourceの観測日時/availabilityがあれば内部の鮮度判定で優先し、HAの`last_updated`が古いだけで同値が続くセンサーを故障と断定しない。時計未同期では時間差判定を行わず、取得日時を不明扱いにする。

## CalendarSection / CalendarEvent

各所有者はowner_id、display_name、icon_id、entity_id、status、display_date、collected_at、events（表示最大3件）、remaining_count、completeを持つ。display_nameは設定/アクセシビリティ用の固定ラベルStar / Moon。owner_idはcalendar_1 / calendar_2、並びは固定。個人名のフィールドは設けない。画面には名前の代わりにicon_idのMDI字形を白黒で描く。icon_idは上記Star / Moonに固定し、24pxで描く。owner_id自体は変更しない。owner_idはHA personと独立する。

CalendarEventはsource_event_id（取得できる場合）、title、start、end、all_day、ongoingを持つ。場所/説明/参加者は表示のために保存しない。タイトルは原文を保持し、表示時に1行へ省略する。

1. HAの表示TZで今日00:00から翌日00:00の期間を求め、所有者ごとに期間イベントを取得する。calendar entityの単一stateだけで一日分を代用しない。
2. offset付き日時をUTCへ正規化し、期間が重なるイベントを扱う。終日dateのendも排他的。前日開始で今日まで続く予定も含める。
3. 時刻付きはendが表示生成時刻以下のものを除外。start <= now < endはongoing。終日は今日を含むものだけ残す。
4. 終日→進行中→未来の開始時刻順に安定ソートし、各3件を表示。時刻欄はAll day / Now / HH:mm。
5. 取得が完全なら、残件数=対象件数−表示件数を`+N more`とする。片方の余白へもう片方の予定を流し込まない。
6. 取得を1人200件で制限する場合、上限超過はcomplete=false、残件数はnullとし`More events`を表示する。取得の一部失敗/不正レコードだけで成功0件を装わない。
7. 同じカレンダー内の同一イベント識別子/同一起点の重複は除外できる。異なる所有者の間では重複除去しない。

両者のentity IDは未確認。設定例に架空の実entity IDを書かず、nullを初期値にする。未設定でも温湿度と予報は動作する。

フォントとMDIマスクはレンダラーの版固定アセットとし、entity IDやowner_idを表示用アイコンで置換しない。予報の有効日時と収集日時は分離する。詳細は下記。

## ForecastSection / ForecastSlot

ForecastSectionはentity_id、status、error_reason（unconfigured / unsupported / request_failed / no_valid_slots等）、collected_at、source_issued_at?、source_temperature_unit、slotsを持つ。未提供の発行日時をHAのlast_updatedで捏造しない。

slotsは生成時刻以上の最初の3時間境界から固定4件。各slotはvalid_at（UTC）、local_date、local_time、day_offset、condition?、icon_key、raw_temperature?、temperature_c?、statusを持つ。過去の配列添字でなくvalid_atでキャッシュと照合する。ラベルだけを新しい時刻に置換しない。

forecastは1回の応答を上限200件、正規化して日時で選ぶ。個別temperatureが不正/欠落なら気温だけ`—`、未知conditionは`?`、項目全体がなければアイコン/気温とも`—`。全4枠が欠けるなら`Forecast unavailable`。降水確率/降水量/風速はMVPの表示モデルへ追加しない。具体例と境界規則は[forecast.md](forecast.md)。

## RenderedFrame

immutable frame ID（SHA-256の64桁hex）、schema_version、width=800、height=480、byte_length=192000、palette_id、generated_at、source取得時刻の要約、fresh_until、reserved_status_overlay、bytes。frame IDは**最終4bpp bytesだけ**から計算する。HTTPの配信時刻や次回接続予定はmanifest側に置き、同じ画像を不要に別ID化しない。

`fresh_until`は表示している有効セクションの鮮度期限の最小値。古いframeをサーバーが再配信しても延長しない。期限到来は再取得/ローカル注意表示の理由になるが、取得成功とは扱わない。端末が完全停止していれば上部状態欄の状態も自動では変わらないため、フレームは常に絶対取得日時を含める。

## DeviceReport

report ID、boot ID、firmware version、battery percent?、RSSI?、last received frame、last displayed frame、display result、display_completed_at?、local_overlay（none/offline/time_unknown）、next wake予定、エラーコード。時刻不明時はdisplay_completed_at=nullとし、サーバー受信時刻を代わりの「観測時刻」として保存する。

reportの再送は同じreport IDで冪等に処理し、古いreportが新しいACKを上書きしない。サーバーが知らないframe IDは表示成功の証拠として採用しない。エラー詳細へURL認証情報やraw responseを含めない。

## キャッシュの遷移

`empty → downloading(inactive slot) → validated → committed → display_pending → displayed`。

途中失敗は前世代へ戻る。パネル成功前の`committed`はディスク保存完了だけを意味する。displayed metadataは成功後に更新。再起動時に最後の正常フレームを再表示してからACKし、物理パネル内容の不確実性を解消する。

HAは配信予定フレーム、最後にACKしたフレーム、配信済み未ACKフレームを保持する。未ACKの中間フレームは少なくとも24時間保持（1端末あたり64世代以内）。期限後のreportは検証不能としてmanifestで再表示を要求する。HA再起動でACK済み世代と鍵/設定を失わない。
