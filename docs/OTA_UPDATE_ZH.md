# ESP32 股票小屏 OTA 在线升级说明

## 这次增加了什么

- Setup 网页显示当前固件版本
- 在 Setup 网页中检查和安装新版本
- LCD 显示下载、校验和重启进度
- 使用 `app0` / `app1` 双应用分区
- 下载失败或 MD5 校验失败时继续运行当前固件
- Cloudflare Worker 代理固件清单和 GitHub Release 下载

## 第一次必须用 USB 烧录

原来的 `Huge APP` 是单应用分区，不能直接通过 OTA 变成双分区。

因此第一次启用 OTA 时，必须通过 Arduino IDE 完整上传本项目一次。项目根目录中的 `partitions.csv` 会随这次 USB 上传写入开发板。完成后，后续版本才可以从 Setup 网页在线安装。

建议设置：

- Board: `ESP32S3 Dev Module`
- Flash Size: `16MB (128Mb)`
- Partition Scheme: `Custom`（若仍选择 `Huge APP`，项目内 `partitions.csv` 也会覆盖实际分区，但界面的容量提示会不准确）
- PSRAM: `OPI PSRAM`

## 用户如何在线升级

1. 设备已经连接可上网的 Wi-Fi。
2. 运行中长按 BOOT 进入 Setup Mode。
3. 手机连接热点 `Reckton-Stock-Setup`。
4. 浏览器打开 `http://192.168.4.1`。
5. 在 `Firmware Update` 区域点击 `Check for Update`。
6. 有新版本时点击 `Install`。
7. 保持供电，观察 LCD 进度，完成后设备自动重启。

如果设备当前没有连接互联网，Setup 网页仍可修改 Wi-Fi，但在线升级按钮不可用。

## 开发者如何发布 OTA 版本

1. 在 `FirmwareVersion.h` 中提升版本号，例如从 `2.1.0` 改为 `2.2.0`。
2. 编译项目，取得 `LVGL_Arduino.ino.bin`。
3. 运行：

```powershell
.\scripts\Prepare-OtaRelease.ps1 `
  -BinPath "固件 bin 文件的完整路径" `
  -Version "2.2.0" `
  -Notes "本次更新说明"
```

4. 脚本会生成：
   - `output/ota/v2.2.0/esp32-stock-ticker.bin`
   - `output/ota/v2.2.0/esp32-stock-ticker.bin.md5`
   - 更新后的 `firmware/manifest.json`
5. 在 GitHub 创建 `v2.2.0` Release，先上传前两个文件。
6. 确认 Release 下载链接可用后，再把 `firmware/manifest.json` 推送到 `main`。
7. Cloudflare Worker 最多缓存清单约 5 分钟，之后设备即可发现新版本。

## 安全与恢复边界

- 固件必须完整下载并通过 MD5 校验后才会切换启动分区。
- 下载中断不会破坏当前正在运行的版本。
- 版本比较只允许升级到更高版本，默认不会降级。
- 升级时应连接 USB 或保持电池电量充足。
- 当前属于个人原型级 OTA；后续面向公开用户时，建议再增加固件数字签名。
