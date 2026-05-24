# MX8650A + Seeed Studio XIAO 動作確認

ChisaBall 70 のトラックボール部品検証用に、MX8650A 光学マウスセンサーから X/Y delta を読み、Arduino の Serial Monitor に出力する最小プロジェクトです。

このスケッチは `D2 = SDIO`, `D3 = SCLK` の 2 線構成を優先するため、デフォルトでは MX8650 ライブラリに依存せず、MX8650A の半二重シリアル通信を bit-bang で実装しています。

今回の実機確認では、XIAO ESP32C3 に書き込み、MX8650A から `pid1=0x30`, `pid2=0x5A` と、ボール操作時の `dx/dy` 非ゼロ値を確認できました。

## ファイル

- `MX8650A_XIAO_nRF52840_Test.ino`
  - Arduino IDE でそのまま開けるスケッチ
  - 115200 bps で `dx=12, dy=-3` のように出力
  - MX8650A のレジスタ読み取り処理を関数分離
- `README.md`
  - 配線、環境構築、書き込み、確認、トラブルシュート

## 配線

| MX8650A | XIAO nRF52840 / XIAO ESP32C3 | 備考 |
|---|---|---|
| Pin 1 NC | 未接続 | そのままで OK |
| Pin 2 MOTSWK | 未接続 | 今回はポーリングで読むため未接続 |
| Pin 3 SDIO | D2 / P0.28 / GPIO4 | 双方向データ線 |
| Pin 4 SCLK | D3 / P0.29 / GPIO5 | クロック線 |
| Pin 5 LED_CNTL | 元基板の LED/抵抗を流用 | LED が外れていると読めないことがあります |
| Pin 6 GND | GND | XIAO と MX8650A の GND を共通にする |
| Pin 7 VDD | 3V3 | MX8650A の VDD は 2.0V から 3.5V |
| Pin 8 VDDA | 元基板のコンデンサ周辺を流用 | 3.3V へ直接つながない。内部 1.8V 系のデカップリング想定 |

重要: ダイソーマウス元基板のマイコンが SDIO/SCLK にまだ接続されている場合、XIAO とバスを取り合う可能性があります。読めない場合は、元マイコン側の SDIO/SCLK パターンを切る、またはセンサー周辺だけを切り出して確認してください。

## XIAO ESP32C3 での確認済み手順

Arduino CLI では以下の設定で書き込みできました。

```powershell
arduino-cli compile --upload -p COM9 --fqbn esp32:esp32:XIAO_ESP32C3:FlashMode=dio .\MX8650A_XIAO_nRF52840_Test --additional-urls https://espressif.github.io/arduino-esp32/package_esp32_index.json
```

Serial Monitor は `115200 bps` です。Windows から直接読む場合は、`DTR=true`, `RTS=false` の組み合わせでスケッチ側の出力を確認できました。`DTR=false`, `RTS=true` は ESP32-C3 が USB bootloader に入ることがあるため、通常の動作確認では避けます。

今回の確認結果:

```text
debug pid1=0x30, pid2=0x5A, image_quality=136, op_state=0x00, config=0x06
dx=-1, dy=-13
dx=-6, dy=-6
dx=17, dy=34
```

## VSCode で dx/dy を確認する

このリポジトリを VSCode で開いた状態で、`Terminal > Run Task...` から以下を実行できます。

- `MX8650: Monitor dx/dy`
  - XIAO の COM ポートを自動検出し、`debug ...` と `dx/dy` を VSCode のターミナルに表示します。
  - `dx=0, dy=0` は 1 秒に 1 回だけ表示し、ボールを動かした時の非ゼロ値を見やすくしています。
- `MX8650: Monitor dx/dy all lines`
  - スケッチから出てくる `dx/dy` 行をすべて表示します。
- `MX8650: Upload XIAO ESP32C3`
  - `COM9` の XIAO ESP32C3 にこのスケッチを書き込みます。COM番号が変わった場合は `.vscode/tasks.json` の `COM9` を変更してください。

ターミナルから直接実行する場合:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\watch_mx8650.ps1
```

COMポートを明示する場合:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\watch_mx8650.ps1 -Port COM9
```

終了はターミナルで `Ctrl+C` です。

## Arduino IDE 環境構築 Windows

### 1. Arduino IDE を入れる

Arduino IDE 2.x をインストールします。

公式 Seeed 手順:
https://wiki.seeedstudio.com/XIAO_BLE/

### 2. Seeed のボード URL を追加

Arduino IDE を開きます。

`File > Preferences` を開き、`Additional Boards Manager URLs` に以下を追加します。

```text
https://files.seeedstudio.com/arduino/package_seeeduino_boards_index.json
```

既に他の URL がある場合は、右端のボタンから 1 行ずつ追加します。

### 3. ボードパッケージを入れる

`Tools > Board > Boards Manager...` を開き、検索欄に以下を入力します。

```text
seeed nrf52
```

まずは次のどちらかを入れてください。

- 推奨: `Seeed nRF52 mbed-enabled Boards`
  - Serial がそのまま使いやすいです。
- 代替: `Seeed nRF52 Boards`
  - Serial のコンパイルで止まる場合があります。その場合は Library Manager で `Adafruit TinyUSB Library` を入れ、`.ino` 冒頭の `#include <Adafruit_TinyUSB.h>` をコメント解除してください。

### 4. ボードを選ぶ

通常版 XIAO nRF52840 の場合:

`Tools > Board` から `Seeed XIAO nRF52840` を選びます。

表示名が `Seeed XIAO nRF52840 Sense` しかない環境では、通常版でも D2/D3 の GPIO 検証は同じピン配置で進められます。

### 5. ポートを選ぶ

XIAO を USB-C データケーブルで PC に接続します。

`Tools > Port` から XIAO の COM ポートを選びます。COM1/COM2 ではなく、通常は COM3 以降です。

アップロードで止まる場合は、XIAO の Reset を 1 回押すか、素早く 2 回押してブートローダーモードに入れてから再度アップロードします。

## 必要ライブラリ

このスケッチのデフォルト動作には、MX8650 用の外部ライブラリは不要です。

必要になる可能性があるもの:

- `Adafruit TinyUSB Library`
  - `Seeed nRF52 Boards` を選び、Serial でコンパイルエラーが出た場合のみ使用
- `styropyr0/MX8650`
  - 比較検証用。今回の `.ino` ではデフォルトでは使いません。

## styropyr0/MX8650 の導入方法

まず試す候補としては `styropyr0/MX8650` を優先します。

GitHub:
https://github.com/styropyr0/MX8650

### Library Manager から入れる

1. Arduino IDE を開く
2. `Sketch > Include Library > Manage Libraries...`
3. `MX8650` で検索
4. `MX8650 Mouse sensor library` を Install

### ZIP で入れる

1. GitHub ページで `Code > Download ZIP`
2. Arduino IDE で `Sketch > Include Library > Add .ZIP Library...`
3. ダウンロードした ZIP を選ぶ

### libraries フォルダへ置く

ZIP を展開して、以下のような場所に置きます。

```text
C:\Users\<ユーザー名>\Documents\Arduino\libraries\MX8650
```

その後 Arduino IDE を再起動します。

## ライブラリ選定メモ

### styropyr0/MX8650

API は今回の目的に近く、`getMotionData()`, `getDeltaX()`, `getDeltaY()`, `getPID()` などがあります。Arduino Library Manager にも登録されています。

ただし、XIAO nRF52840 の現在の配線 `D2=SDIO`, `D3=SCLK` では、そのまま動かない可能性があります。理由は次の通りです。

- ライブラリ内部が `SPI.transfer()` を使うハードウェア SPI 前提の実装です。
- Seeed のピン表では、XIAO nRF52840 のハードウェア SPI は主に D8/D9/D10 側です。
- D2/P0.28 と D3/P0.29 は通常 GPIO/Analog ピンなので、コンストラクタで渡しても `SPI.transfer()` の実ピンが D2/D3 に切り替わるとは限りません。
- MX8650A の SDIO は 1 本の双方向線なので、ハードウェア SPI で使う場合は MOSI/MISO の扱いに注意が必要です。
- `getDeltaX()` / `getDeltaY()` の戻り値は `uint8_t` なので、移動量として扱うときは `int8_t` にキャストする必要があります。

修正案:

- 今回のスケッチのように、D2/D3 を GPIO として bit-bang する。
- ライブラリ側を修正して、`SPI.transfer()` を使わず、設定された SCLK/SDIO ピンで半二重通信する。
- どうしてもライブラリを無改造で試す場合は、XIAO のハードウェア SPI ピンへ配線を寄せ、SDIO の双方向衝突を避けるために MOSI/MISO 接続を抵抗経由にする。ただし、今回の D2/D3 配線確認には向きません。

### OptiMouse

OptiMouse は古い optical mouse sensor 向けの参考実装として扱います。

GitHub:
https://github.com/zapmaker/OptiMouse

MX8650A 専用ではないため、この検証では直接依存しません。半二重の光学センサー通信や、古い ADNS 系センサーの扱いを見る参考用です。

## スケッチの設定値

`.ino` の先頭付近で変更できます。

```cpp
const uint8_t PIN_SDIO = D2;
const uint8_t PIN_SCLK = D3;

const bool INVERT_X = false;
const bool INVERT_Y = false;
const bool SWAP_XY = false;
const uint32_t PRINT_INTERVAL_MS = 20;
```

方向が逆なら `INVERT_X` / `INVERT_Y` を `true` にします。

X/Y が入れ替わる場合は `SWAP_XY` を `true` にします。

分解能は以下で変更できます。

```cpp
const uint8_t CONFIGURATION_VALUE = CONFIG_1200_CPI;
```

選択肢:

- `CONFIG_800_CPI`
- `CONFIG_1000_CPI`
- `CONFIG_1200_CPI`
- `CONFIG_1600_CPI`

## 書き込み手順

1. `MX8650A_XIAO_nRF52840_Test.ino` を Arduino IDE で開く
2. ボードを `Seeed XIAO nRF52840` にする
3. ポートを XIAO の COM ポートにする
4. Upload を押す
5. 書き込み後、`Tools > Serial Monitor` を開く
6. 右下の baud rate を `115200` にする

## 正常時の出力例

起動直後:

```text
MX8650A XIAO nRF52840 trackball test
sdio_pin=2, sclk_pin=3
pid1=0x30, pid2=0x50, op_mode=0xA0, config=0x06, image_quality=38, op_state=0x00
```

ボールを転がしたとき:

```text
dx=0, dy=0
dx=12, dy=-3
dx=7, dy=1
dx=-5, dy=0
```

動きがないときも、デフォルトでは `dx=0, dy=0` を 20ms ごとに出します。

## デバッグの見方

1 秒ごとに次のような行を出します。

```text
debug pid1=0x30, pid2=0x50, image_quality=42, op_state=0x00, config=0x06
```

見るポイント:

- `pid1=0x30`
  - 通信できているかの最重要確認です。
- `pid2=0x5?`
  - 上位 nibble が `0x5` なら Product ID として妥当です。下位 nibble は予約扱いです。
- `image_quality`
  - 0 に近いままなら、LED、レンズ、ボール距離、表面条件を疑います。
- `op_state`
  - `0x00` なら通常状態です。sleep に入っている場合は配線や初期化を見直します。
- `config`
  - デフォルトでは `0x06`、つまり 1200 CPI 設定です。

## トラブルシュート

### Serial Monitor に何も出ない

- baud rate が `115200` か確認する。
- Arduino IDE の Port が XIAO の COM ポートか確認する。
- USB-C ケーブルが充電専用ではなくデータ対応か確認する。
- `Seeed nRF52 Boards` で Serial のコンパイルに失敗する場合は、`Adafruit TinyUSB Library` を入れ、`.ino` 冒頭の include をコメント解除する。

### `pid1=0x30` にならない

- SDIO が XIAO D2/P0.28 に入っているか確認する。
- SCLK が XIAO D3/P0.29 に入っているか確認する。
- GND が共通か確認する。
- VDD が XIAO 3V3 につながっているか確認する。
- MX8650A の VDD 範囲は 2.0V から 3.5V なので、5V を入れない。
- 元マウス基板のマイコンが SDIO/SCLK をまだ駆動していないか確認する。
- SDIO/SCLK の配線が長すぎる、または半田不良がないか確認する。
- `debug ... pid_ng` が続く場合は、センサーが応答していません。

### PID は読めるが dx/dy が変わらない

- LED が点灯または発光しているか確認する。可視赤色で見えない場合もあります。
- LED/抵抗/レンズを元基板から外していないか確認する。
- レンズとボール表面の距離を調整する。
- MX8650A データシートの目安では、レンズ基準面から表面までの Z はおよそ 2.3mm から 2.5mm です。トラックボールでは球面なので、実機で高さ調整が必要です。
- センサーの中心、レンズ、ボール中心が大きくずれていないか確認する。
- ELECOM 25mm ボールの表面状態、色、照明条件によって image_quality が低くなる可能性があります。
- `image_quality` が常に 0 付近なら、焦点距離か LED を優先して見る。

### dx/dy が逆向き

`.ino` の先頭で変更します。

```cpp
const bool INVERT_X = true;
const bool INVERT_Y = false;
```

### X と Y が入れ替わる

`.ino` の先頭で変更します。

```cpp
const bool SWAP_XY = true;
```

### 大きく動かすと変な値になる

MX8650A の Delta_X / Delta_Y は signed 8-bit で、範囲は -128 から +127 です。読み取り周期より速く動かすと overflow します。

overflow 時は次のような表示になります。

```text
dx=127, dy=-128, status=0x9C, x_overflow, y_overflow
```

この場合は以下を試します。

- `PRINT_INTERVAL_MS` を 10 などに下げる。
- CPI を下げる。
- ボールをゆっくり転がす。

## ZMK/Zephyr へ移植するときの対応箇所

移植対象になりやすい関数:

- `mx8650ReadRegister(uint8_t reg)`
- `mx8650WriteRegister(uint8_t reg, uint8_t value)`
- `mx8650ReadMotion()`
- `applyAxisOptions(...)`

ZMK 側では Arduino の `digitalWrite`, `digitalRead`, `pinMode`, `delayMicroseconds` を Zephyr の GPIO API と busy wait に置き換える想定です。

読み取り順序は維持してください。

```text
Motion_Status -> Delta_X -> Delta_Y
```

MX8650A は Motion_Status を読むと Delta_X / Delta_Y を凍結し、その後 Delta_X / Delta_Y を読むことで値がクリアされます。

## 参考

- Seeed Studio XIAO nRF52840 Arduino setup: https://wiki.seeedstudio.com/XIAO_BLE/
- styropyr0/MX8650: https://github.com/styropyr0/MX8650
- OptiMouse: https://github.com/zapmaker/OptiMouse
- MX8650A datasheet mirror: https://atta.szlcsc.com/upload/public/pdf/source/20211022/405499419CB7778ED1BB93A5947099D9.pdf
