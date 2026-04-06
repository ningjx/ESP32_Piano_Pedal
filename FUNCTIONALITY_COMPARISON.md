# ESP-IDF vs Arduino 功能一致性对比

## 发现日期：2026-04-06

---

## 📋 总体评估

| 模块 | Arduino 实现 | ESP-IDF 实现 | 一致性 | 优先级 |
|------|------------|-----------|--------|--------|
| **踏板读取与处理** | ✅ 完整 | ✅ 完整 | 🟢 一致 | 🟢 OK |
| **半踏功能** | ✅ 完整 | ✅ 基础 | 🟡 部分缺陷 | 🔴 高 |
| **NVS 存储** | ✅ Preferences | ✅ NVS | 🟢 兼容 | 🟢 OK |
| **校准功能** | ✅ 完整（启动检测+流程） | ⚠️ 仅基础函数 | 🔴 缺失启动逻辑 | 🔴 高 |
| **OTA 启动模式** | ✅ 检测弱音踏板启动 | ❌ 未实现 | 🔴 缺失 | 🔴 高 |
| **蓝牙启动模式** | ✅ 检测延音踏板启动 | ❌ 未实现 | 🔴 缺失 | 🔴 高 |
| **蓝牙翻页功能** | ✅ 完整实现 | ❌ 仅占位 | 🔴 缺失 | 🔴 中 |
| **Web 服务器** | ✅ OTA 门户网页 | ✅ 页面重做 | 🟢 一致 | 🟢 OK |
| **Web API /status** | ✅ 返回 p0/p1/p2/halfPedal | ⚠️ 返回硬编码数据 | 🟡 需链接 | 🔴 高 |
| **Web API /setHalfPedal** | ✅ 解析参数并保存 | ⚠️ 解析但未保存 | 🔴 无法持久化 | 🔴 高 |
| **OTA 固件更新** | ✅ 完整实现 | ⚠️ 仅占位 | 🔴 缺失实现 | 🔴 中 |
| **功耗优化** | ✅ pm_configure + WDT | ❌ 完全缺失 | 🔴 缺失 | 🟡 低 |
| **日志系统** | ✅ 条件 DEBUG 宏 | ✅ ESP_LOGI | 🟢 兼容 | 🟢 OK |

---

## 🔴 关键缺陷详解

### 1. **启动模式检测缺失** (优先级: 🔴 **严重**)

#### Arduino 的实现：
```cpp
// app_main.c setup() 中：

// OTA 更新功能 - 检查弱音踏板
int softValue = AdcRemap(ADC_Soft_PIN, Soft_Pedal_MIN, Soft_Pedal_MAX);
if (softValue > 127) {
    ShutdownBluetooth();
    otaPortalBegin();  // 启动 WiFi OTA 门户
}

// 蓝牙翻页功能 - 检查延音踏板
int sustainValue = AdcRemap(ADC_Sustain_PIN, Sustain_Pedal_MIN, Sustain_Pedal_MAX);
if (sustainValue > 127) {
    Bluetooth_Active = !Bluetooth_Active;
    SaveBluetoothActive();
}
```

#### ESP-IDF 的实现：
```c
// app_main.c：
// ❌ 完全缺失这两个启动模式检测逻辑
// app_main() 直接启动踏板处理，没有条件分支
```

**影响范围：** 
- 用户无法通过踩踏板切换功能模式
- OTA 更新只能通过重写固件或其他方式触发
- 蓝牙功能无法启用

**修复方案：**
需要在 app_main() 中添加：
```c
// 开机时快速检测踏板状态（在初始化完成后）
// 1. 如果弱音踏板被踩下 → 启动 WiFi OTA 模式
// 2. 如果延音踏板被踩下 → 启用蓝牙翻页模式
```

---

### 2. **校准启动逻辑缺失** (优先级: 🔴 **严重**)

#### Arduino 的实现：
```cpp
void setup() {
    // 在 setup() 最早阶段检查持音踏板电压
    int sV = esp_adc_cal_raw_to_voltage(analogRead(ADC_Sostenuto_PIN), &adc_chars);
    if (sV > 1500) {  // 踩住持音踏板开机
        StartCalibration();
        return;  // 进入校准模式，不继续初始化其他功能
    }
}
```

#### ESP-IDF 的实现：
```c
void app_main() {
    // ❌ 缺失校准启动检测
    // 直接初始化所有功能
}
```

**影响范围：**
- 无法进入校准模式
- 用户无法重新校准踏板范围

---

### 3. **/setHalfPedal 缺少 NVS 持久化** (优先级: 🔴 **高**)

#### Arduino 的实现（ota_portal.cpp）：
```cpp
// 服务器接收 GET /setHalfPedal?lower=1500&upper=2500&voltage=1.7
// 调用 SetSustainHalfPedalRange_mV() 和 SetHalfPedalVoltage()
// 这两个函数内部自动调用 SaveHalfPedalRange() 保存到 NVS
```

#### ESP-IDF 的实现（web_server.c）：
```c
static esp_err_t set_half_pedal_handler(httpd_req_t *req) {
    // 解析参数
    // ✅ 成功解析 lower, upper, voltage
    // ❌ TODO: Save to NVS  <- 这里没有实现！
    
    httpd_resp_send(req, "{\"success\":true}", 16);
    return ESP_OK;
}
```

**影响范围：**
- 用户在网页上调整半踏范围后，重启设备配置会丢失
- 半踏功能无法个性化定制

---

### 4. **Web API /status 返回硬编码数据** (优先级: 🔴 **高**)

#### Arduino 的实现：
```cpp
// /status 返回实时踏板数据
{
  "p0": {"mv":X, "min":MIN, "max":MAX, "mapped":0-255},
  "p1": {"mv":Y, "min":MIN, "max":MAX, "mapped":0-255},
  "p2": {"mv":Z, "min":MIN, "max":MAX, "mapped":0-255},
  "halfPedal": {"lower":1500, "upper":2500, "voltage":1.7}
}
```

#### ESP-IDF 的实现：
```c
static void get_pedal_status(uint8_t pedal_id, pedal_status_t *status) {
    if (!status) return;
    // ❌ 返回硬编码测试数据
    status->mv = 1500 + pedal_id * 100;
    status->min = 100;
    status->max = 3200;
    // TODO: Link to actual pedal_core.c implementation
}
```

**影响范围：**
- 网页页面显示的进度条无法显示实际踏板状态
- 用户无法在 Web 界面看到半踏范围的实时调整

---

### 5. **蓝牙翻页功能完全缺失** (优先级: 🟡 **中**)

#### Arduino 的实现（main.cpp loop()）：
```cpp
// 翻页功能
if (bleKeyboard.isConnected()) {
    bool pageTurnerDown = false;
    if (sostenutoValue > 100)
        pageTurnerDown = true;
    else if (sostenutoValue < 90)
        pageTurnerDown = false;

    unsigned long downTime = GetPageturnerContinueTime(pageTurnerDown);

    if (downTime == LongPressTimeMs) {
        bleKeyboard.write(KEY_PAGE_UP);  // 长按上一页
    } else if (downTime > 0 && downTime < LongPressTimeMs) {
        bleKeyboard.write(KEY_PAGE_DOWN);  // 短按下一页
    }
}
```

#### ESP-IDF 的实现：
```c
// pedal_core.c
// ❌ 只有 bluetooth_active 标志，无翻页逻辑
// ❌ 缺少 BLE 键盘驱动初始化
// ❌ 缺少按键事件检测和发送
```

**影响范围：**
- 蓝牙连接时，持音踏板无法翻页

---

### 6. **功耗优化和看门狗缺失** (优先级: 🟡 **低**)

#### Arduino 的实现：
```cpp
void setup() {
    // 动态电源管理
    esp_pm_config_esp32_t pm_config = {
        .max_freq_mhz = 80,
        .min_freq_mhz = 10,
        .light_sleep_enable = true
    };
    esp_pm_configure(&pm_config);

    // 看门狗定时器
    esp_task_wdt_init(30, true);  // 30秒超时
    esp_task_wdt_add(NULL);
    
    // 主循环中定期调用
    esp_task_wdt_reset();
}
```

#### ESP-IDF 的实现：
```c
// app_main.c
// ❌ 完全缺失 esp_pm_configure()
// ❌ 完全缺失 esp_task_wdt_init()
```

**影响范围：**
- 功耗可能比 Arduino 版本高
- 系统无保护机制，若代码卡死无法自动重启

---

## ✅ 正确实现的功能

### 1. **踏板读取与处理**
- ✅ ADC 驱动实现（pedal_core.c）
- ✅ 5ms 周期处理
- ✅ 三踏板读取（弱音、持音、延音）
- ✅ DAC 输出（延音和持音）
- ✅ 开关输出（弱音）

### 2. **半踏功能（基础）**
- ✅ 半踏范围检测
- ✅ 半踏电压输出（与 Arduino 逻辑一致）
```c
if (sustain_mv >= config->half_pedal_lower_mv &&
    sustain_mv <= config->half_pedal_upper_mv) {
    uint8_t dac_value = (uint8_t)(config->half_pedal_voltage / 3.3f * 255);
    dac_output_sustain(dac_value);
}
```

### 3. **NVS 存储框架**
- ✅ 校准数据保存/读取
- ✅ 蓝牙活动状态保存/读取
- ✅ 半踏范围保存接口存在（但 Web API 未调用）

### 4. **Web 页面 UI**
- ✅ 完全重做为 Arduino 风格
- ✅ 竖向进度条、拖拽标记所有功能实现

---

## 📝 修复优先级建议

| 优先级 | 任务 | 工作量 | 重要性 |
|--------|------|--------|--------|
| 🔴 P1 | 添加启动模式检测（OTA/蓝牙） | 中 | 影响系统可用性 |
| 🔴 P2 | 链接 /status API 到实时踏板数据 | 低 | 网页反馈 |
| 🔴 P3 | /setHalfPedal 添加 NVS 持久化 | 低 | 用户设置保存 |
| 🔴 P4 | 添加校准启动检测逻辑 | 中 | 校准功能完整性 |
| 🟡 P5 | 实现蓝牙翻页功能 | 高 | BLE 功能完整 |
| 🟡 P6 | 实现 OTA 固件更新 | 高 | 更新功能完整 |
| 🟡 P7 | 添加功耗优化和 WDT | 低 | 系统鲁棒性 |

---

## 🔧 下一步建议

### 立即修复（关键路径）：
1. **app_main.c** - 添加启动模式检测
2. **web_server.c** - 链接实时踏板数据 + 半踏持久化
3. **pedal_core.h** - 导出踏板状态查询接口

### 中期完善：
4. **ble_keyboard.c** - 实现翻页逻辑 
5. **ota_update.c** - 实现固件更新
6. **app_main.c** - 添加功耗和 WDT 配置

---

## 📊 统计

- **总功能数**：13 个核心模块
- **完整实现**：6 个 (46%)
- **部分实现**：3 个 (23%)
- **缺失实现**：4 个 (31%)

**总体完成度：~ 70%** ⚠️ 需要补充关键启动逻辑
