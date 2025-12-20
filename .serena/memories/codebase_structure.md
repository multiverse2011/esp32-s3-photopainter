# コードベース構造

## 現在のディレクトリ構成

```
esp32-s3-photopainter/
├── .claude/                  # Claude Code設定
├── .devcontainer/            # Docker開発環境設定
│   ├── devcontainer.json
│   └── Dockerfile
├── .git/                     # Gitリポジトリ
├── .serena/                  # Serenaエージェント設定
├── .vscode/                  # VSCode設定
│   ├── c_cpp_properties.json
│   ├── launch.json
│   └── settings.json
├── build/                    # ビルド成果物(自動生成)
├── main/                     # メインアプリケーション
│   ├── CMakeLists.txt
│   └── hello_world_main.c    # 現在のエントリーポイント
├── .clangd                   # Clangd設定
├── .gitignore                # Git除外設定
├── CMakeLists.txt            # ルートCMake設定
├── IMPLEMENTATION_PLAN.md    # 実装計画書(詳細)
├── pytest_hello_world.py     # Pythonテストスクリプト
├── README.md                 # プロジェクトREADME
├── sdkconfig                 # ESP-IDF設定(自動生成)
└── sdkconfig.ci              # CI用設定
```

## 予定されているディレクトリ構成(実装後)

```
esp32-s3-photopainter/
├── components/               # ESP-IDFコンポーネント(実装予定)
│   ├── epd_driver/          # E-Paperディスプレイドライバ
│   │   ├── include/
│   │   │   └── epd_driver.h
│   │   ├── epd_driver.c
│   │   ├── epd_spi.c
│   │   └── CMakeLists.txt
│   │
│   ├── gfx_library/         # グラフィックライブラリ
│   │   ├── include/
│   │   │   ├── gfx_paint.h
│   │   │   └── gfx_fonts.h
│   │   ├── gfx_paint.c
│   │   ├── gfx_primitives.c
│   │   ├── fonts/
│   │   │   ├── font_16.c
│   │   │   ├── font_24.c
│   │   │   └── font_32.c
│   │   └── CMakeLists.txt
│   │
│   ├── weather_service/     # 天気情報取得
│   │   ├── include/
│   │   │   ├── weather_service.h
│   │   │   └── weather_types.h
│   │   ├── weather_http.c
│   │   ├── weather_parser.c
│   │   └── CMakeLists.txt
│   │
│   ├── calendar_ui/         # カレンダーUI
│   │   ├── include/
│   │   │   └── calendar_ui.h
│   │   ├── calendar_ui.c
│   │   ├── weather_icons.c
│   │   └── CMakeLists.txt
│   │
│   └── wifi_manager/        # WiFi接続管理
│       ├── include/
│       │   └── wifi_manager.h
│       ├── wifi_manager.c
│       └── CMakeLists.txt
│
├── main/                     # メインアプリケーション
│   ├── main.c               # メインロジック(実装予定)
│   ├── Kconfig.projbuild    # menuconfig設定(実装予定)
│   └── CMakeLists.txt
│
├── sdkconfig.defaults        # デフォルト設定(作成予定)
└── partitions.csv            # パーティションテーブル(作成予定)
```

## 主要ファイルの役割

### ルートディレクトリ
| ファイル | 役割 |
|---------|------|
| `CMakeLists.txt` | プロジェクト全体のビルド設定 |
| `sdkconfig` | ESP-IDFの設定(menuconfig生成) |
| `sdkconfig.defaults` | デフォルト設定値 |
| `IMPLEMENTATION_PLAN.md` | 実装計画の詳細 |
| `README.md` | プロジェクト概要 |

### main/
| ファイル | 役割 |
|---------|------|
| `main.c` | アプリケーションのエントリーポイント |
| `Kconfig.projbuild` | menuconfigカスタム設定 |
| `CMakeLists.txt` | mainコンポーネントのビルド設定 |

### components/
各コンポーネントは独立したディレクトリとして作成:
- `include/`: 公開ヘッダーファイル
- `*.c`: 実装ファイル
- `CMakeLists.txt`: コンポーネントのビルド設定

## ビルドシステム

### CMake構成
1. **ルートCMakeLists.txt**: プロジェクト全体の定義
2. **components/*/CMakeLists.txt**: 各コンポーネントの定義
3. **main/CMakeLists.txt**: メインアプリケーションの定義

### 依存関係の管理
CMakeLists.txtで`REQUIRES`キーワードで依存を宣言:
```cmake
idf_component_register(
    SRCS "component.c"
    INCLUDE_DIRS "include"
    REQUIRES driver esp_http_client json
)
```

## コンポーネント間の依存関係(予定)

```
main
├── wifi_manager
├── epd_driver
│   └── driver (ESP-IDFコンポーネント)
├── gfx_library
│   └── epd_driver
├── weather_service
│   ├── esp_http_client
│   ├── json
│   └── wifi_manager
└── calendar_ui
    ├── gfx_library
    └── weather_service
```

## 設定ファイル

### sdkconfig
- ESP-IDF menuconfigで自動生成
- PSRAM、WiFi、コンポーネント設定等
- バージョン管理対象(プロジェクト固有の設定)

### sdkconfig.defaults
- デフォルト設定値を定義
- 初回ビルド時やクリーンビルド時に使用
- バージョン管理推奨

### Kconfig.projbuild
- カスタムmenuconfig項目を定義
- WiFi SSID、API Key等のユーザー設定
- バージョン管理推奨

## ビルド成果物(build/)

```
build/
├── bootloader/          # ブートローダー
├── partition_table/     # パーティションテーブル
├── esp-idf/             # ESP-IDFコンポーネント
├── project_name.bin     # アプリケーションバイナリ
├── project_name.elf     # ELFファイル(デバッグ用)
└── project_name.map     # メモリマップ
```

**注意**: `build/`ディレクトリは`.gitignore`で除外されている

## 開発フロー

### 1. 新規コンポーネント追加
```powershell
# ディレクトリ作成
New-Item -ItemType Directory -Path components\new_component\include

# CMakeLists.txt作成
New-Item -ItemType File -Path components\new_component\CMakeLists.txt

# ソースファイル作成
New-Item -ItemType File -Path components\new_component\new_component.c
New-Item -ItemType File -Path components\new_component\include\new_component.h
```

### 2. ビルド・テスト
```powershell
# ビルド
idf.py build

# フラッシュ+実行
idf.py -p COM4 flash monitor
```

### 3. 反復開発
- コード変更
- ビルド(`idf.py build`)
- フラッシュ(`idf.py flash`)
- テスト・デバッグ

## 重要な制約

### MINIMAL_BUILD設定
現在のCMakeLists.txtで`MINIMAL_BUILD ON`が設定されている:
```cmake
idf_build_set_property(MINIMAL_BUILD ON)
```

**影響**:
- 必要最小限のコンポーネントのみビルド
- 多くのESP-IDFコンポーネントが無効化

**対応**:
実装フェーズ1で`MINIMAL_BUILD`行を削除またはコメントアウトが必要

## 参照すべきドキュメント

- [IMPLEMENTATION_PLAN.md](IMPLEMENTATION_PLAN.md): 詳細な実装計画
- [ESP-IDF Build System](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-guides/build-system.html): ビルドシステム公式ドキュメント
- サンプルコード: `C:\Users\iris\Projects\xiaozhi-esp32-sample`
