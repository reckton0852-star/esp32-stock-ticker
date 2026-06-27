# v2.0.0

## English

This release turns the project into a much more complete standalone device version, with a stable boot flow, built-in setup mode, and much better WiFi behavior.

### Highlights

- Added a 3 second boot splash screen
- Added WiFi bootstrap flow during startup
- Added automatic fallback across multiple saved WiFi networks
- Added automatic setup mode when WiFi connection fails
- Added built-in hotspot setup mode:
  - SSID: `Reckton-Stock-Setup`
  - Config page: `http://192.168.4.1`
- Added browser-based configuration for:
  - WiFi credentials
  - proxy base URL
  - stock symbols
  - brightness
  - refresh interval
  - auto rotate interval

### Stability Improvements

- Fixed boot-time black screen / restart loop behavior
- Fixed Driver task stack overflow
- Fixed WiFi reconnect flow conflicts
- Fixed TCP/IP core assertion crashes caused by overlapping network activity
- Improved LCD update behavior and startup display timing
- Improved backlight PWM frequency
- Improved stock refresh stability with queue-based background request handling

### UI Improvements

- Added product-style boot splash screen
- Added WiFi scan / bootstrap status screen
- Improved setup mode screen flow
- Refined startup experience so the device now:
  1. shows boot splash
  2. scans and tries saved WiFi
  3. enters stock UI if connected
  4. enters setup mode if WiFi fails

### Proxy and Data Improvements

- Improved proxy support for cloud and local relay modes
- Improved symbol handling for configurable stock lists
- Preserved last successful quote while refreshing
- Kept stock requests stable through a proxy-first architecture

### Notes

- Tag: `v2.0.0`
- Recommended for users who want a more standalone device workflow instead of a hardcoded WiFi-only startup
- The `enclosure/` prototype files were intentionally not included in this software release

---

## 中文

这个版本把项目推进到了更接近成品设备的状态，重点完成了稳定启动、WiFi 引导连接、失败自动进入配置模式，以及更完整的独立使用体验。

### 主要更新

- 新增 3 秒启动画面
- 新增开机 WiFi bootstrap 流程
- 支持多个已保存 WiFi 自动轮询连接
- 当 WiFi 连接失败时，自动进入 setup mode
- 新增设备自带热点配置模式：
  - 热点名称：`Reckton-Stock-Setup`
  - 配置地址：`http://192.168.4.1`
- 新增网页配置能力，可设置：
  - WiFi 账号密码
  - 代理地址
  - 股票代码列表
  - 屏幕亮度
  - 刷新时间
  - 自动轮播时间

### 稳定性修复

- 修复启动黑屏 / 循环重启问题
- 修复 Driver task 栈溢出问题
- 修复 WiFi 重连流程互相打架的问题
- 修复网络并发导致的 TCP/IP core 断言崩溃
- 优化 LCD 启动显示时序
- 优化背光 PWM 频率，改善显示稳定性
- 优化股票后台刷新机制，改为更稳定的队列式请求流程

### 界面与体验优化

- 新增更像产品的启动页
- 新增 WiFi 扫描 / 引导连接状态页
- 优化 setup mode 页面流程
- 现在设备启动流程为：
  1. 显示启动页
  2. 扫描并尝试已保存 WiFi
  3. 连接成功则进入股票页面
  4. 连接失败则进入 setup mode

### 代理与数据侧优化

- 增强本地代理与云端代理两种模式支持
- 增强可配置股票代码列表支持
- 刷新失败时保留上一次成功价格，不清屏
- 继续采用“代理优先”架构，提升 ESP32 端稳定性

### 说明

- 版本标签：`v2.0.0`
- 这是一个更适合作为独立小设备使用的版本
- `enclosure/` 外壳原型文件本次没有一起打进软件版本发布
