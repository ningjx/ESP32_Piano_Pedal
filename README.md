# ESP32 钢琴踏板控制器

[![Build and Release](https://github.com/ningjx/ESP32_Piano_Pedal/actions/workflows/build-release.yml/badge.svg)](https://github.com/ningjx/ESP32_Piano_Pedal/actions/workflows/build-release.yml)

[中文](#中文文档) | [English](#english-documentation)

一个基于 ESP32 的智能钢琴踏板控制器，将传统钢琴三个踏板（延音、持音、弱音）转换为电信号输出，支持蓝牙翻页和 OTA 固件更新。

---

## 中文文档

### 功能特性

- 🎹 **三踏板支持**：延音(Sustain)、持音(Sostenuto)、弱音(Soft)
- 📡 **模拟电压输出**：通过 DAC 输出连续电压信号，兼容传统钢琴踏板接口
- 🔄 **半踏兼容模式**：延音踏板支持三档输出（0V/半踏电压/最大电压），兼容不支持连续踏板信号的设备
- 📱 **蓝牙翻页**：通过 BLE 键盘协议实现乐谱翻页
- 📡 **OTA 更新**：支持 WiFi 热点 Web 界面固件升级
- ⚙️ **霍尔传感器校准**：自动适应不同踏板行程范围
- 🔋 **低功耗设计**：动态电源管理，支持轻睡眠模式

### 硬件要求

| 组件 | 规格 |
|-----|------|
| MCU | ESP32-WROOM-32 (4MB Flash, 520KB SRAM) |
| 传感器 | 霍尔传感器 x3 |
| 晶振 | 40MHz |
| 工作电压 | 3.3V |

### 项目结构

```
ESP32_Piano_Pedal/
├── arduino_src/              # Arduino 框架源代码
│   ├── src/
│   │   ├── main.cpp          # 主程序
│   │   └── ota_portal.cpp    # OTA 更新模块
│   ├── include/
│   │   └── ota_portal.h
│   ├── platformio.ini        # PlatformIO 配置
│   └── partition.csv         # 分区表
├── espressif_src/            # ESP-IDF 框架源代码 (备用)
├── Documents/
│   └── 业务功能逻辑文档.md    # 详细功能文档
├── PCB/                      # PCB 设计文件
└── .github/
    └── workflows/
        └── build-release.yml # GitHub Action 自动构建
```

### 快速开始

#### 环境准备

1. 安装 [PlatformIO](https://platformio.org/)
2. 克隆项目：
   ```bash
   git clone https://github.com/ningjx/ESP32_Piano_Pedal.git
   cd ESP32_Piano_Pedal
   ```

#### 编译固件

```bash
cd arduino_src
pio run -e esp32dev
```

#### 烧录固件

```bash
pio run -e esp32dev -t upload
```

#### 串口监视

```bash
pio device monitor -b 115200
```

### 使用说明

#### 开机模式

| 操作 | 模式 | 说明 |
|-----|------|------|
| 正常开机 | 正常模式 | 踏板信号输出 |
| 踩住持音踏板开机 | 校准模式 | 校准霍尔传感器范围 |
| 踩住弱音踏板开机 | OTA 模式 | 启动 WiFi 热点进行固件更新 |
| 踩住延音踏板开机 | 蓝牙切换 | 开启/关闭蓝牙翻页功能 |

#### 校准流程

1. 踩住持音踏板开机，听到蜂鸣提示 (Do → Sol)
2. 将三个踏板分别踩到底和松开，记录最大最小值
3. 踩住持音踏板 2 秒完成校准，听到蜂鸣提示 (Do 长音)
4. 设备自动重启

> 校准超时 20 秒将自动取消，不保存本次校准结果。

#### OTA 固件更新

1. 踩住弱音踏板开机，听到蜂鸣提示 (Do→Sol→Si)
2. 使用手机或电脑连接 WiFi 热点：`钢琴踏板固件更新`
3. 浏览器自动弹出或访问 `http://192.168.4.1`
4. 上传 `.bin` 固件文件
5. 等待上传完成，设备自动重启

#### 蓝牙翻页

1. 踩住延音踏板开机，听到蜂鸣提示 (Mi→Sol→Si) 表示蓝牙已开启
2. 使用平板或手机连接名为 `翻页器` 的蓝牙设备
3. 短踩持音踏板：下一页
4. 长踩持音踏板 (≥500ms)：上一页

> 蓝牙连接时，持音踏板的 DAC 输出将被禁用。

### 引脚定义

| 功能 | GPIO | 说明 |
|-----|------|------|
| 延音踏板 DAC | 25 | DAC 通道 1 |
| 持音踏板 DAC | 26 | DAC 通道 2 |
| 弱音踏板开关 | 17 | 数字输出 |
| 延音霍尔 ADC | 35 | ADC1_CH7 |
| 持音霍尔 ADC | 32 | ADC1_CH4 |
| 弱音霍尔 ADC | 33 | ADC1_CH5 |
| 蜂鸣器 | 16 | PWM 输出 |

### 配置参数

#### 半踏兼容模式配置

通过 OTA Web 界面可配置：

| 参数 | 默认值 | 说明 |
|-----|-------|------|
| 半踏兼容模式 | 关闭 | 开启后延音踏板输出三档电压 |
| 半踏范围下限 | 1500 mV | 触发半踏的最低电压 |
| 半踏范围上限 | 2500 mV | 触发半踏的最高电压 |
| 半踏输出电压 | 1.7 V | 半踏时输出的固定电压 |
| 最大输出电压 | 1.9 V | 踩到底时输出的最大电压 |

**三档输出逻辑**：
- 不踩 ~ 半踏下限：输出 0V
- 半踏范围内：输出半踏电压
- 半踏上限 ~ 踩到底：输出最大电压

#### 蜂鸣提示编码

| 场景 | 音调 | 音符 |
|-----|------|------|
| 进入校准 | Do → Sol | C4 → G4 |
| 校准完成 | Do (长音) | C4 |
| 校准取消 | Sol → Do | G4 → C4 |
| 进入 OTA | Do→Sol→Si | C4→G4→B4 |
| 蓝牙开启 | Mi→Sol→Si | E4→G4→B4 |

### 自动构建

项目使用 GitHub Action 自动构建和发布：

- **触发条件**：`arduino_src/` 目录变更推送到 main/master 分支
- **手动触发**：在 Actions 页面点击 "Run workflow"
- **输出产物**：`esp_pedal_vX.X.X.bin`

#### 版本管理

固件版本定义在 `arduino_src/platformio.ini` 中：

```ini
build_flags = -DFW_VERSION=\"1.2.0\"
```

当版本号对应的 git tag 不存在时，Action 会自动创建 tag 并发布 Release。

### 技术规格

| 参数 | 值 |
|-----|-----|
| DAC 输出范围 | 0 - 1.9V |
| ADC 采样范围 | 0 - 3300mV |
| ADC 分辨率 | 12-bit |
| 主循环周期 | 5ms (200Hz) |
| 蓝牙协议 | BLE Keyboard |

### 依赖库

| 库 | 版本 | 用途 |
|---|------|------|
| ESP32 BLE Keyboard | 0.3.2 | 蓝牙键盘协议 |

### 许可证

本项目采用 MIT 许可证，详见 [LICENSE](LICENSE) 文件。

### 相关文档

- [业务功能逻辑文档](Documents/业务功能逻辑文档.md) - 详细的功能说明和架构文档
- [PlatformIO 文档](https://docs.platformio.org/)
- [ESP32 Arduino 框架](https://docs.espressif.com/projects/arduino-esp32/)

### 贡献

欢迎提交 Issue 和 Pull Request！

### 更新日志

### v1.3.0
- 半踏功能改为"半踏兼容模式"，三档输出（0V/半踏电压/最大电压）
- 新增"最大输出电压"设置（0.1V-3.3V可调）
- 优化OTA Web界面布局

### v1.2.0
- 新增半踏功能，支持可配置的半踏范围和电压
- 新增 OTA Web 界面配置功能
- 优化 ADC 采样和信号处理算法

### v1.1.0
- 新增蓝牙翻页功能
- 优化功耗管理

### v1.0.0
- 基础踏板输出功能
- 霍尔传感器校准
- OTA 固件更新

---

## English Documentation

An ESP32-based smart piano pedal controller that converts traditional piano's three pedals (Sustain, Sostenuto, Soft) into electrical signal outputs, supporting Bluetooth page turning and OTA firmware updates.

### Features

- 🎹 **Triple Pedal Support**: Sustain, Sostenuto, Soft
- 📡 **Analog Voltage Output**: Continuous voltage output via DAC, compatible with traditional piano pedal interfaces
- 🔄 **Half-Pedal Compatibility Mode**: Sustain pedal supports three-level output (0V/half-pedal voltage/max voltage), compatible with devices that don't support continuous pedal signals
- 📱 **Bluetooth Page Turning**: Sheet music page turning via BLE keyboard protocol
- 📡 **OTA Updates**: WiFi hotspot web interface for firmware upgrades
- ⚙️ **Hall Sensor Calibration**: Auto-adapts to different pedal travel ranges
- 🔋 **Low Power Design**: Dynamic power management with light sleep mode support

### Hardware Requirements

| Component | Specification |
|-----|------|
| MCU | ESP32-WROOM-32 (4MB Flash, 520KB SRAM) |
| Sensors | Hall sensors x3 |
| Crystal | 40MHz |
| Operating Voltage | 3.3V |

### Project Structure

```
ESP32_Piano_Pedal/
├── arduino_src/              # Arduino framework source code
│   ├── src/
│   │   ├── main.cpp          # Main program
│   │   └── ota_portal.cpp    # OTA update module
│   ├── include/
│   │   └── ota_portal.h
│   ├── platformio.ini        # PlatformIO configuration
│   └── partition.csv         # Partition table
├── espressif_src/            # ESP-IDF framework source code (backup)
├── Documents/
│   └── 业务功能逻辑文档.md    # Detailed functional documentation
├── PCB/                      # PCB design files
└── .github/
    └── workflows/
        └── build-release.yml # GitHub Action auto-build
```

### Quick Start

#### Environment Setup

1. Install [PlatformIO](https://platformio.org/)
2. Clone the project:
   ```bash
   git clone https://github.com/ningjx/ESP32_Piano_Pedal.git
   cd ESP32_Piano_Pedal
   ```

#### Build Firmware

```bash
cd arduino_src
pio run -e esp32dev
```

#### Flash Firmware

```bash
pio run -e esp32dev -t upload
```

#### Serial Monitor

```bash
pio device monitor -b 115200
```

### Usage

#### Boot Modes

| Action | Mode | Description |
|-----|------|------|
| Normal boot | Normal Mode | Pedal signal output |
| Hold Sostenuto pedal on boot | Calibration Mode | Calibrate Hall sensor range |
| Hold Soft pedal on boot | OTA Mode | Start WiFi hotspot for firmware update |
| Hold Sustain pedal on boot | Bluetooth Toggle | Enable/disable Bluetooth page turning |

#### Calibration Process

1. Hold Sostenuto pedal on boot, hear beep prompt (Do → Sol)
2. Press and release all three pedals fully to record min/max values
3. Hold Sostenuto pedal for 2 seconds to complete calibration, hear beep prompt (Do long tone)
4. Device automatically reboots

> Calibration timeout is 20 seconds, after which calibration is cancelled without saving.

#### OTA Firmware Update

1. Hold Soft pedal on boot, hear beep prompt (Do→Sol→Si)
2. Connect to WiFi hotspot with phone or computer: `钢琴踏板固件更新`
3. Browser auto-opens or visit `http://192.168.4.1`
4. Upload `.bin` firmware file
5. Wait for upload to complete, device auto-reboots

#### Bluetooth Page Turning

1. Hold Sustain pedal on boot, hear beep prompt (Mi→Sol→Si) indicating Bluetooth is enabled
2. Connect tablet or phone to Bluetooth device named `翻页器`
3. Short press Sostenuto pedal: Next page
4. Long press Sostenuto pedal (≥500ms): Previous page

> When Bluetooth is connected, Sostenuto pedal DAC output is disabled.

### Pin Definitions

| Function | GPIO | Description |
|-----|------|------|
| Sustain DAC | 25 | DAC Channel 1 |
| Sostenuto DAC | 26 | DAC Channel 2 |
| Soft Switch | 17 | Digital output |
| Sustain Hall ADC | 35 | ADC1_CH7 |
| Sostenuto Hall ADC | 32 | ADC1_CH4 |
| Soft Hall ADC | 33 | ADC1_CH5 |
| Buzzer | 16 | PWM output |

### Configuration Parameters

#### Half-Pedal Compatibility Mode Configuration

Configurable via OTA Web interface:

| Parameter | Default | Description |
|-----|-------|------|
| Half-Pedal Mode | Off | When enabled, sustain pedal outputs three-level voltage |
| Half-Pedal Range Lower | 1500 mV | Minimum voltage to trigger half-pedal |
| Half-Pedal Range Upper | 2500 mV | Maximum voltage to trigger half-pedal |
| Half-Pedal Output Voltage | 1.7 V | Fixed voltage output at half-pedal |
| Max Output Voltage | 1.9 V | Maximum voltage when fully pressed |

**Three-Level Output Logic**:
- Not pressed ~ Half-pedal lower: Output 0V
- Within half-pedal range: Output half-pedal voltage
- Half-pedal upper ~ Fully pressed: Output max voltage

#### Beep Codes

| Scenario | Tone | Notes |
|-----|------|------|
| Enter Calibration | Do → Sol | C4 → G4 |
| Calibration Complete | Do (long) | C4 |
| Calibration Cancelled | Sol → Do | G4 → C4 |
| Enter OTA | Do→Sol→Si | C4→G4→B4 |
| Bluetooth Enabled | Mi→Sol→Si | E4→G4→B4 |

### Auto Build

The project uses GitHub Action for automatic build and release:

- **Trigger**: Changes pushed to `arduino_src/` directory on main/master branch
- **Manual Trigger**: Click "Run workflow" on Actions page
- **Output Artifact**: `esp_pedal_vX.X.X.bin`

#### Version Management

Firmware version is defined in `arduino_src/platformio.ini`:

```ini
build_flags = -DFW_VERSION=\"1.2.0\"
```

When a git tag for the version doesn't exist, Action automatically creates the tag and publishes a Release.

### Technical Specifications

| Parameter | Value |
|-----|-----|
| DAC Output Range | 0 - 1.9V |
| ADC Sampling Range | 0 - 3300mV |
| ADC Resolution | 12-bit |
| Main Loop Period | 5ms (200Hz) |
| Bluetooth Protocol | BLE Keyboard |

### Dependencies

| Library | Version | Purpose |
|---|------|------|
| ESP32 BLE Keyboard | 0.3.2 | Bluetooth keyboard protocol |

### License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

### Related Documentation

- [Functional Logic Documentation](Documents/业务功能逻辑文档.md) - Detailed functional specifications and architecture
- [PlatformIO Documentation](https://docs.platformio.org/)
- [ESP32 Arduino Framework](https://docs.espressif.com/projects/arduino-esp32/)

### Contributing

Issues and Pull Requests are welcome!

### Changelog

#### v1.3.0
- Changed half-pedal feature to "Half-Pedal Compatibility Mode" with three-level output (0V/half-pedal voltage/max voltage)
- Added "Max Output Voltage" setting (0.1V-3.3V adjustable)
- Optimized OTA Web interface layout

#### v1.2.0
- Added half-pedal feature with configurable half-pedal range and voltage
- Added OTA Web interface configuration
- Optimized ADC sampling and signal processing algorithms

#### v1.1.0
- Added Bluetooth page turning feature
- Optimized power management

#### v1.0.0
- Basic pedal output functionality
- Hall sensor calibration
- OTA firmware update