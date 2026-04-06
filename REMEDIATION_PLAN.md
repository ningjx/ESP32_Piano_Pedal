# ESP-IDF 项目功能对齐修复计划

## 优先级 P1：启动模式检测 (关键路径)

### 需要修改的文件
- [main/app_main.c](../../main/app_main.c)

### 修复内容

#### 现状代码
```c
void app_main(void) {
    ESP_LOGI(TAG, "Initializing...");
    nvs_config_init();
    pedal_init();
    pedal_config_t *config = pedal_config_get();
    if (nvs_config_load(config) != ESP_OK) {
        ESP_LOGW(TAG, "No saved config");
    }
    pedal_start_processing();
    xTaskCreate(monitor_task, "monitor", 2048, NULL, 3, NULL);
}
```

#### 需要添加的逻辑
```c
// 在 pedal_start_processing() 之前添加：

// 延迟 50ms 让 ADC 初始化完成
vTaskDelay(pdMS_TO_TICKS(50));

// 读取踏板值
uint16_t soft_mv = adc_read_pedal_mv(PEDAL_SOFT);
uint16_t sustain_mv = adc_read_pedal_mv(PEDAL_SUSTAIN);

// OTA 模式：弱音踏板被踩下（> 1500mV）
if (soft_mv > 1500) {
    ESP_LOGW(TAG, "OTA mode detected - Soft pedal pressed");
    // 关闭蓝牙以节省内存
    // ble_shutdown();
    // 启动 WiFi OTA 服务器
    wifi_manager_start_ota_mode();
    web_server_start();
    return;  // 不继续初始化正常踏板处理
}

// 蓝牙模式：延音踏板被踩下（> 1500mV）
if (sustain_mv > 1500) {
    ESP_LOGW(TAG, "Bluetooth mode detected - Sustain pedal pressed");
    config->bluetooth_active = true;
    nvs_config_save(config);
}
```

### 关键依赖
- [ ] 需要实现 `wifi_manager_start_ota_mode()` 函数
- [ ] 需要在 ADC 初始化后立即可用
- [ ] 需要 `web_server_start()` 可用

---

## 优先级 P2：Web API 实时数据链接

### 需要修改的文件
- [include/network/web/web_server.c](../../include/network/web/web_server.c)
- [include/pedal/pedal_core.h](../../include/pedal/pedal_core.h)

### 修复内容

#### 当前问题代码
```c
static void get_pedal_status(uint8_t pedal_id, pedal_status_t *status) {
    // ❌ 返回硬编码数据
    status->mv = 1500 + pedal_id * 100;
    status->min = 100;
    status->max = 3200;
}
```

#### 修复代码
```c
static void get_pedal_status(uint8_t pedal_id, pedal_status_t *status) {
    if (!status) return;
    
    // 获取实时踏板状态
    pedal_state_t state = pedal_get_state();
    pedal_config_t *config = pedal_config_get();
    
    switch (pedal_id) {
        case 0:  // Soft pedal
            status->mv = state.soft_mv;
            status->min = config->soft_min_mv;
            status->max = config->soft_max_mv;
            break;
        case 1:  // Sostenuto pedal
            status->mv = state.sostenuto_mv;
            status->min = config->sostenuto_min_mv;
            status->max = config->sostenuto_max_mv;
            break;
        case 2:  // Sustain pedal
            status->mv = state.sustain_mv;
            status->min = config->sustain_min_mv;
            status->max = config->sustain_max_mv;
            break;
    }
    
    // 计算映射值 (0-255)
    uint16_t range = status->max - status->min;
    if (range > 0) {
        status->mapped = (status->mv - status->min) * 255 / range;
    } else {
        status->mapped = 0;
    }
}
```

#### /status 响应修复
```c
static esp_err_t status_handler(httpd_req_t *req) {
    pedal_status_t pedals[3];
    for (int i = 0; i < 3; i++) 
        get_pedal_status(i, &pedals[i]);
    
    pedal_config_t *config = pedal_config_get();
    
    // Manual JSON construction with real data
    char json_response[512];
    snprintf(json_response, sizeof(json_response),
        "{"
        "\"p0\":{\"mv\":%d,\"min\":%d,\"max\":%d,\"mapped\":%d},"
        "\"p1\":{\"mv\":%d,\"min\":%d,\"max\":%d,\"mapped\":%d},"
        "\"p2\":{\"mv\":%d,\"min\":%d,\"max\":%d,\"mapped\":%d},"
        "\"halfPedal\":{\"lower\":%d,\"upper\":%d,\"voltage\":%.1f}"
        "}",
        pedals[0].mv, pedals[0].min, pedals[0].max, pedals[0].mapped,
        pedals[1].mv, pedals[1].min, pedals[1].max, pedals[1].mapped,
        pedals[2].mv, pedals[2].min, pedals[2].max, pedals[2].mapped,
        config->half_pedal_lower_mv,
        config->half_pedal_upper_mv,
        (float)config->half_pedal_voltage / 100.0f  // 假设存储为 int16 百分比
    );
    
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json_response, strlen(json_response));
    return ESP_OK;
}
```

---

## 优先级 P3：/setHalfPedal 添加 NVS 持久化

### 需要修改的文件
- [include/network/web/web_server.c](../../include/network/web/web_server.c)

### 修复内容

#### 当前代码
```c
static esp_err_t set_half_pedal_handler(httpd_req_t *req) {
    // ... 参数解析 ...
    // TODO: Save to NVS  ❌
    httpd_resp_send(req, "{\"success\":true}", 16);
}
```

#### 修复代码
```c
static esp_err_t set_half_pedal_handler(httpd_req_t *req) {
    char buf[256] = {0};
    httpd_req_get_url_query_str(req, buf, sizeof(buf) - 1);
    
    char param[32];
    pedal_config_t *config = pedal_config_get();
    bool changed = false;
    
    if (httpd_query_key_value(buf, "lower", param, sizeof(param)) == ESP_OK) {
        int lower = atoi(param);
        if (lower >= 0 && lower <= 3300) {
            config->half_pedal_lower_mv = lower;
            ESP_LOGI(TAG, "Setting half-pedal lower: %d mV", lower);
            changed = true;
        }
    }
    
    if (httpd_query_key_value(buf, "upper", param, sizeof(param)) == ESP_OK) {
        int upper = atoi(param);
        if (upper >= 0 && upper <= 3300) {
            config->half_pedal_upper_mv = upper;
            ESP_LOGI(TAG, "Setting half-pedal upper: %d mV", upper);
            changed = true;
        }
    }
    
    if (httpd_query_key_value(buf, "voltage", param, sizeof(param)) == ESP_OK) {
        float voltage = atof(param);
        if (voltage >= 0.0f && voltage <= 3.3f) {
            // 存储为浮点数或转换为 uint16 百分比
            config->half_pedal_voltage = (uint16_t)(voltage * 100);  // 转为 0-330
            ESP_LOGI(TAG, "Setting half-pedal voltage: %.1f V", voltage);
            changed = true;
        }
    }
    
    // ✅ 保存到 NVS
    if (changed) {
        esp_err_t ret = nvs_config_save(config);
        if (ret == ESP_OK) {
            httpd_resp_set_type(req, "application/json");
            httpd_resp_send(req, "{\"success\":true}", 16);
        } else {
            httpd_resp_send_500(req);
        }
    } else {
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, "{\"success\":false,\"error\":\"no parameters changed\"}", 53);
    }
    
    return ESP_OK;
}
```

---

## 优先级 P4：校准启动检测

### 需要修改的文件
- [main/app_main.c](../../main/app_main.c)
- [include/pedal/pedal_core.h](../../include/pedal/pedal_core.h)

### 修复内容

在 app_main.c 中添加：

```c
// 在 pedal_config_load() 之后、pedal_start_processing() 之前

// 检测校准模式启动：持音踏板电压 > 1500mV
uint16_t sostenuto_mv = adc_read_pedal_mv(PEDAL_SOSTENUTO);
if (sostenuto_mv > 1500) {
    ESP_LOGW(TAG, "Calibration mode detected");
    pedal_start_calibration_mode();
    // 进入校准模式，阻塞等待完成或超时
    return;
}
```

需要实现 pedal_core.c 中的：
```c
void pedal_start_calibration_mode(void) {
    // 类似 Arduino 的 StartCalibration()
    // 1. 初始化 min=5000, max=0
    // 2. 蜂鸣提示
    // 3. 进入循环，实时记录三踏板 min/max
    // 4. 检测长按持音踏板按钮完成校准
    // 5. 或 20s 超时自动取消
}
```

---

## 优先级 P5：蓝牙翻页功能

### 需要修改的文件
- [include/pedal/pedal_core.c](../../include/pedal/pedal_core.c)
- [include/bluetooth/ble_keyboard.c](../../include/bluetooth/ble_keyboard.c)

### 修复内容

在 pedal_core.c 主循环中添加翻页逻辑：

```c
// 在 pedal_process_task() 中，处理踏板输出后

// 蓝牙翻页功能
if (config->bluetooth_active && ble_is_connected()) {
    static unsigned long page_down_start_ms = 0;
    bool sostenuto_pressed = (sostenuto_val > 100);
    
    if (sostenuto_pressed && page_down_start_ms == 0) {
        page_down_start_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;
    }
    
    if (!sostenuto_pressed && page_down_start_ms > 0) {
        unsigned long press_time = xTaskGetTickCount() * portTICK_PERIOD_MS - page_down_start_ms;
        
        if (press_time >= 500) {  // LongPressTimeMs = 500ms
            ble_send_key(BLE_KEY_PAGE_UP);
        } else if (press_time > 0) {
            ble_send_key(BLE_KEY_PAGE_DOWN);
        }
        page_down_start_ms = 0;
    }
}
```

---

## 优先级 P6：OTA 固件更新完整实现

### 需要修改的文件
- [include/network/ota/ota_update.c](../../include/network/ota/ota_update.c)

### 修复内容

完整实现固件更新处理...（按 Arduino 源码移植）

---

## 优先级 P7：功耗优化和 WDT

### 需要修改的文件
- [main/app_main.c](../../main/app_main.c)

### 修复内容

```c
void app_main(void) {
    // 动态电源管理 - 降低空闲功耗
    esp_pm_config_esp32_t pm_config = {
        .max_freq_mhz = 80,
        .min_freq_mhz = 10,        // 空闲时降频到 10MHz
        .light_sleep_enable = true
    };
    esp_pm_configure(&pm_config);

    // 看门狗定时器 - 防止系统卡死
    esp_task_wdt_init(30, true);   // 30秒超时
    esp_task_wdt_add(NULL);        // 添加当前任务
    
    // ... 其他初始化 ...
}

// 在每个长时间运行的任务中定期调用
esp_task_wdt_reset();
```

---

## 测试检查清单

- [ ] P1：开机时踩弱音踏板 → WiFi 软 AP 启动，可访问 192.168.4.1
- [ ] P1：开机时踩延音踏板 → 蓝牙设备"翻页器"可见
- [ ] P2：网页进度条实时显示踏板数据
- [ ] P3：网页拖拽半踏标记 → 数据保存 → 重启后配置仍存在
- [ ] P4：开机时踩住持音踏板 3 秒 → 进入校准模式
- [ ] P5：蓝牙连接后，短按持音踏板 → 下一页，长按 → 上一页
- [ ] P6：网页上传固件 → 进度条显示 → 自动重启
- [ ] P7：正常运行状态下，内存占用稳定

---

## 参考资料

- Arduino 源码：`arduino_src/src/main.cpp` - 启动模式、校准、翻页逻辑
- Arduino OTA 门户：`arduino_src/src/ota_portal.cpp` - Web API 实现
- ESP-IDF 组件：esp_http_server, esp_ota, BLE API 文档
