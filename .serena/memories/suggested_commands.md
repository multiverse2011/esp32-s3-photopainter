# 推奨コマンド一覧

## ビルド関連

### プロジェクト設定
```powershell
# menuconfig を開く(設定変更)
idf.py menuconfig

# 設定を保存してビルド
idf.py build
```

### クリーンビルド
```powershell
# ビルドディレクトリを削除してクリーンビルド
idf.py fullclean
idf.py build
```

### 設定確認
```powershell
# sdkconfig の内容を表示
type sdkconfig | Select-String "CONFIG_SPIRAM"
```

## フラッシュ・実行

### フラッシュ書き込み
```powershell
# ビルド + フラッシュ書き込み
idf.py -p COM4 flash

# フラッシュ書き込みのみ(ビルド済みの場合)
idf.py -p COM4 app-flash
```

### モニター
```powershell
# シリアルモニター起動(ログ表示)
idf.py -p COM4 monitor

# ビルド + フラッシュ + モニター (一度に実行)
idf.py -p COM4 flash monitor
```

### COMポート確認
```powershell
# 利用可能なCOMポート一覧
mode
# または
Get-WmiObject Win32_SerialPort | Select-Object Name,DeviceID
```

## デバッグ・解析

### サイズ解析
```powershell
# バイナリサイズ確認
idf.py size

# コンポーネント別サイズ詳細
idf.py size-components

# ファイル別サイズ詳細
idf.py size-files
```

### メモリ解析
```powershell
# メモリマップ表示
idf.py partition-table
```

## テスト

### Pytest実行
```powershell
# テストスクリプト実行
python pytest_hello_world.py
```

## コンポーネント管理

### コンポーネント作成
```powershell
# componentsディレクトリに新規コンポーネント作成
New-Item -ItemType Directory -Path components\component_name
New-Item -ItemType Directory -Path components\component_name\include
New-Item -ItemType File -Path components\component_name\CMakeLists.txt
```

### CMakeLists.txt サンプル(コンポーネント用)
```cmake
idf_component_register(
    SRCS "component_name.c"
    INCLUDE_DIRS "include"
    REQUIRES driver esp_http_client json
)
```

## Git操作(Windows)

### 基本操作
```powershell
# ステータス確認
git status

# 変更の差分確認
git diff

# コミット
git add .
git commit -m "commit message"

# プッシュ
git push origin main
```

### ログ確認
```powershell
# コミット履歴
git log --oneline -10

# 特定ファイルの変更履歴
git log --follow -- path/to/file
```

## ファイル操作(Windows PowerShell)

### ディレクトリ操作
```powershell
# カレントディレクトリ移動
cd path\to\directory

# ディレクトリ一覧
ls
# または
dir

# ツリー表示
tree /F
```

### ファイル検索
```powershell
# ファイル名検索
Get-ChildItem -Recurse -Filter "*.c"

# ファイル内容検索
Select-String -Path *.c -Pattern "epd_init"

# 再帰的に検索
Get-ChildItem -Recurse -Filter "*.c" | Select-String -Pattern "epd_init"
```

### ファイル内容表示
```powershell
# ファイル全体表示
type file.c
# または
Get-Content file.c

# 先頭10行表示
Get-Content file.c -Head 10

# 末尾10行表示
Get-Content file.c -Tail 10
```

## ESP-IDF特有コマンド

### 開発環境セットアップ
```powershell
# ESP-IDF環境変数設定(初回のみ)
C:\Users\iris\esp\v5.5.1\esp-idf\export.ps1
```

### パーティションテーブル
```powershell
# パーティション情報表示
idf.py partition-table
```

### アプリケーション情報
```powershell
# アプリケーション情報表示
idf.py app-info
```

## トラブルシューティング

### ビルドエラー時
```powershell
# フルクリーンビルド
idf.py fullclean
idf.py build
```

### フラッシュ失敗時
```powershell
# ボーレート下げて書き込み
idf.py -p COM4 -b 115200 flash

# 強制的にブートローダモードで書き込み
# (BOOTボタン押しながらENボタン押下後に実行)
idf.py -p COM4 flash
```

### モニター文字化け時
```powershell
# ボーレート指定
idf.py -p COM4 -b 115200 monitor
```

## よく使うショートカット(idf.py monitor)

- `Ctrl+]`: モニター終了
- `Ctrl+T Ctrl+H`: ヘルプ表示
- `Ctrl+T Ctrl+R`: ESP32リセット
- `Ctrl+T Ctrl+F`: PSRAM情報表示

## プロジェクト構造確認

### コンポーネント一覧
```powershell
# componentsディレクトリ内のコンポーネント表示
ls components
```

### ビルド成果物確認
```powershell
# ビルド済みファイル確認
ls build
```
