# Windows システムコマンド

## PowerShellコマンド基本

### ディレクトリ操作
```powershell
# カレントディレクトリ表示
pwd
# または
Get-Location

# ディレクトリ移動
cd path\to\directory
Set-Location path\to\directory

# 親ディレクトリへ移動
cd ..

# ホームディレクトリへ移動
cd ~
```

### ファイル・ディレクトリ一覧
```powershell
# ファイル一覧(簡易)
ls
dir
Get-ChildItem

# 詳細表示
ls -Force  # 隠しファイル含む
Get-ChildItem -Force

# 特定の拡張子のみ
ls *.c
Get-ChildItem -Filter "*.c"

# 再帰的に表示
ls -Recurse
Get-ChildItem -Recurse

# ツリー表示(コマンドプロンプト)
tree /F
```

### ファイル内容表示
```powershell
# ファイル全体表示
type file.txt
Get-Content file.txt
cat file.txt  # エイリアス

# 先頭10行表示
Get-Content file.txt -Head 10

# 末尾10行表示
Get-Content file.txt -Tail 10

# リアルタイム監視(ログファイル等)
Get-Content file.txt -Wait -Tail 10
```

### ファイル・ディレクトリ作成
```powershell
# ディレクトリ作成
mkdir directory_name
New-Item -ItemType Directory -Path directory_name

# 空ファイル作成
New-Item -ItemType File -Path file.txt

# ファイルに内容書き込み
"content" | Out-File file.txt
echo "content" > file.txt  # 上書き
echo "content" >> file.txt  # 追記
```

### ファイル・ディレクトリ削除
```powershell
# ファイル削除
Remove-Item file.txt
del file.txt
rm file.txt

# ディレクトリ削除(中身含む)
Remove-Item -Recurse -Force directory_name
rm -r -fo directory_name
```

### ファイルコピー・移動
```powershell
# ファイルコピー
Copy-Item source.txt destination.txt
copy source.txt destination.txt
cp source.txt destination.txt

# ディレクトリコピー(再帰的)
Copy-Item -Recurse source_dir destination_dir
cp -r source_dir destination_dir

# ファイル移動
Move-Item source.txt destination.txt
move source.txt destination.txt
mv source.txt destination.txt
```

## ファイル検索

### ファイル名検索
```powershell
# カレントディレクトリから再帰的に検索
Get-ChildItem -Recurse -Filter "*.c"

# 特定ディレクトリから検索
Get-ChildItem -Path components -Recurse -Filter "*.c"

# ファイル名の部分一致
Get-ChildItem -Recurse | Where-Object { $_.Name -like "*driver*" }

# ディレクトリのみ検索
Get-ChildItem -Recurse -Directory

# ファイルのみ検索
Get-ChildItem -Recurse -File
```

### ファイル内容検索
```powershell
# 単一ファイル内検索
Select-String -Path file.c -Pattern "epd_init"

# 複数ファイル検索
Select-String -Path *.c -Pattern "epd_init"

# 再帰的に検索
Get-ChildItem -Recurse -Filter "*.c" | Select-String -Pattern "epd_init"

# 大文字小文字を区別しない
Select-String -Path *.c -Pattern "epd_init" -CaseSensitive:$false

# 行番号表示
Select-String -Path *.c -Pattern "epd_init" | Select-Object Line, LineNumber, Path

# コンテキスト表示(前後2行)
Select-String -Path file.c -Pattern "epd_init" -Context 2,2
```

## Git操作

### 基本コマンド
```powershell
# リポジトリ初期化
git init

# クローン
git clone https://github.com/user/repo.git

# ステータス確認
git status

# 差分確認
git diff
git diff --staged  # ステージング済みの差分

# ファイル追加
git add file.c
git add .  # 全ての変更

# コミット
git commit -m "commit message"

# プッシュ
git push origin main

# プル
git pull origin main
```

### ログ・履歴
```powershell
# コミット履歴(簡易)
git log --oneline

# 最新10件
git log --oneline -10

# グラフ表示
git log --graph --oneline --all

# 特定ファイルの履歴
git log --follow -- file.c

# 特定日時以降
git log --since="2025-12-01"
```

### ブランチ操作
```powershell
# ブランチ一覧
git branch

# ブランチ作成
git branch feature-name

# ブランチ切り替え
git checkout feature-name
git switch feature-name  # 新しい方法

# ブランチ作成+切り替え
git checkout -b feature-name
git switch -c feature-name

# ブランチ削除
git branch -d feature-name
```

### 取り消し操作
```powershell
# ステージング取り消し
git restore --staged file.c

# ファイルの変更を取り消し
git restore file.c

# 最新コミットを取り消し(コミットは残す)
git reset --soft HEAD~1

# 最新コミットを取り消し(変更も破棄)
git reset --hard HEAD~1
```

## テキスト処理

### 文字列検索・置換
```powershell
# 文字列検索
Select-String -Path *.c -Pattern "search_text"

# 正規表現検索
Select-String -Path *.c -Pattern "^void.*\(" 

# 文字列置換(ファイル内)
(Get-Content file.c) -replace "old_text", "new_text" | Set-Content file.c

# 複数ファイル一括置換
Get-ChildItem *.c | ForEach-Object {
    (Get-Content $_) -replace "old_text", "new_text" | Set-Content $_
}
```

### フィルタリング
```powershell
# 特定行を抽出
Get-Content file.txt | Select-String "pattern"

# 除外
Get-Content file.txt | Where-Object { $_ -notmatch "pattern" }

# ソート
Get-Content file.txt | Sort-Object

# ユニーク行のみ
Get-Content file.txt | Sort-Object -Unique

# 行数カウント
(Get-Content file.txt).Count
Get-Content file.txt | Measure-Object -Line
```

## プロセス管理

### プロセス確認
```powershell
# プロセス一覧
Get-Process

# 特定プロセス検索
Get-Process | Where-Object { $_.Name -like "*python*" }

# プロセス終了
Stop-Process -Name process_name
Stop-Process -Id 1234
```

### シリアルポート確認
```powershell
# COMポート一覧
mode

# 詳細情報
Get-WmiObject Win32_SerialPort | Select-Object Name, DeviceID

# より詳細な情報
Get-WmiObject Win32_PnPEntity | Where-Object { $_.Name -like "*COM*" }
```

## ネットワーク

### 接続確認
```powershell
# Ping
ping google.com
Test-Connection google.com

# DNS確認
nslookup google.com
Resolve-DnsName google.com

# ポート確認
Test-NetConnection -ComputerName google.com -Port 443
```

### HTTP確認
```powershell
# Webページ取得
Invoke-WebRequest https://example.com

# APIテスト
Invoke-RestMethod -Uri https://api.openweathermap.org/data/2.5/weather?q=Tokyo&appid=YOUR_KEY
```

## 環境変数

### 環境変数確認
```powershell
# 全ての環境変数
Get-ChildItem Env:

# 特定の環境変数
$Env:PATH
$Env:IDF_PATH

# 環境変数設定(セッション中のみ)
$Env:VARIABLE_NAME = "value"
```

### PATHに追加
```powershell
# セッション中のみ
$Env:PATH += ";C:\new\path"

# 永続的に追加(管理者権限必要)
[Environment]::SetEnvironmentVariable("PATH", $Env:PATH + ";C:\new\path", "User")
```

## その他便利コマンド

### システム情報
```powershell
# システム情報
systeminfo

# OSバージョン
winver

# ディスク使用量
Get-PSDrive

# メモリ使用量
Get-WmiObject Win32_OperatingSystem | Select-Object FreePhysicalMemory, TotalVisibleMemorySize
```

### クリップボード操作
```powershell
# クリップボードにコピー
"text" | Set-Clipboard
Get-Content file.txt | Set-Clipboard

# クリップボードから取得
Get-Clipboard
```

### エイリアス
```powershell
# エイリアス一覧
Get-Alias

# エイリアス作成
Set-Alias ll Get-ChildItem

# エイリアス削除
Remove-Alias ll
```

## PowerShellスクリプト実行

### 実行ポリシー
```powershell
# 現在のポリシー確認
Get-ExecutionPolicy

# ポリシー変更(管理者権限必要)
Set-ExecutionPolicy RemoteSigned

# 一時的にバイパス
PowerShell -ExecutionPolicy Bypass -File script.ps1
```

### スクリプト実行
```powershell
# スクリプト実行
.\script.ps1

# 引数付き実行
.\script.ps1 -Param1 value1 -Param2 value2
```

## 便利なショートカット

- `Tab`: 補完
- `Ctrl+C`: コマンド中断
- `Ctrl+L`: 画面クリア(cls)
- `↑/↓`: コマンド履歴
- `Ctrl+R`: コマンド履歴検索(PSReadLine)
- `F7`: コマンド履歴一覧(コマンドプロンプト)
