# 実装時の確認手順（未実装）

HA統合とファームウェアの実装・受入の順序を示す。対象デザインはLayout B。

1. [selected-layout.md](selected-layout.md)のLayout Bの寸法とgolden入力を確認する。部屋とentityの割当候補は[research.md](research.md)。
2. 実機のFlash容量と6色色票を確認する。現在のsdkconfigは2MBなのでREADMEの16MBだけを頼りに書き換えない。
3. テストHA 2026.9.1でカスタム統合を実装/検証する。日本語フォント、依存バージョン、ライセンスを同梱する。
4. HAの設定フローでデバイスを追加し、Star / Moonそれぞれのcalendar entity、温湿度4組、hourly対応weatherを選ぶ。候補weather.wu_wai_forecast_homeで3時間境界の時刻/天候/気温を確認する。電力/買い物/プラグは設定しない。端末専用キーを発行する。
5. ファームウェアのHA frameモードをbuild。シリアル入力でWi-Fi、HA origin、device ID、専用キーを設定する。秘密値をGit/shell履歴/シリアルログへ残さない。
6. ユーザー指定のHTTP originを使うなら明示設定する。HTTPSを使うなら端末が検証できる証明書と初期時刻を用意する。
7. HAの配信予定画像→端末表示→成功ACK→最終表示画像を確認する。battery不明時はunknownのままにする。
8. 通信断、認証失敗、破損画像、日付変更、HA再起動、電源再投入を試す。機器の実stateは変更せず、テストサーバー/fixtureで故障を注入する。
9. 起床時間・更新秒数・電池消費を測定し、更新間隔を決める。採用案のスクリーンショットだけで実機検証を代替しない。

実装前に必要な確認項目: パネルprofile、キャッシュpartition、calendar割当、HTTP/HTTPS運用。未決を解消したらspecのStatusとtasksを更新する。
