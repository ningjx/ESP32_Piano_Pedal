#include "ota_portal.h"
#include <WiFi.h>
#include <WebServer.h>
#include <Update.h>
#include <DNSServer.h>
#include <Preferences.h>

// 若未通过 build_flags 提供 FW_VERSION，则使用默认值
#ifndef FW_VERSION
#define FW_VERSION "unknown"
#endif

// #define DEBUG

// 调试宏（与 main.cpp 保持一致）：定义 DEBUG 时启用，否则为空操作
#ifdef DEBUG
#define DBG_BEGIN(baud) Serial.begin(baud)
#define DBG_PRINT(...) Serial.print(__VA_ARGS__)
#define DBG_PRINTLN(...) Serial.println(__VA_ARGS__)
#define DBG_PRINTF(...) Serial.printf(__VA_ARGS__)
#else
#define DBG_BEGIN(baud) ((void)0)
#define DBG_PRINT(...) ((void)0)
#define DBG_PRINTLN(...) ((void)0)
#define DBG_PRINTF(...) ((void)0)
#endif

static WebServer server(80);
static DNSServer dnsServer;
static bool active = false;

// 三个踏板的状态结构（单位：mV，mapped：0-255）
struct PedalStatus
{
  int mv;
  int minv;
  int maxv;
  int mapped;
};

static PedalStatus pedals[3] = {{0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}};

// 半踏范围（延音踏板，mV值）
static int halfPedalLower_mV = 1500;
static int halfPedalUpper_mV = 2500;
static float halfPedalVoltage = 1.7f;
static bool halfPedalEnabled = false; // 半踏功能开关（默认关闭）
static float maxDACVoltage = 1.9f;    // 最大DAC输出电压

// 外部可调用的函数：用于更新每个踏板的实时状态
extern "C" void otaPortalSetPedalStatus(int index, int mv, int minv, int maxv, int mapped)
{
  if (index < 0 || index >= 3)
    return;
  pedals[index].mv = mv;
  pedals[index].minv = minv;
  pedals[index].maxv = maxv;
  pedals[index].mapped = mapped;
}

// 返回 JSON 状态的处理器
void handleStatus()
{
  String json = "{";
  for (int i = 0; i < 3; ++i)
  {
    json += "\"p" + String(i) + "\":{";
    json += "\"mv\":" + String(pedals[i].mv) + ",";
    json += "\"min\":" + String(pedals[i].minv) + ",";
    json += "\"max\":" + String(pedals[i].maxv) + ",";
    json += "\"mapped\":" + String(pedals[i].mapped);
    json += "}";
    if (i < 2)
      json += ",";
  }
  // 添加半踏范围（mV值）、电压和开关状态
  json += ",\"halfPedal\":{";
  json += "\"lower\":" + String(halfPedalLower_mV) + ",";
  json += "\"upper\":" + String(halfPedalUpper_mV) + ",";
  json += "\"voltage\":" + String(halfPedalVoltage, 2) + ",";
  json += "\"enabled\":" + String(halfPedalEnabled ? "true" : "false") + ",";
  json += "\"maxDACVoltage\":" + String(maxDACVoltage, 1);
  json += "}}";
  server.send(200, "application/json", json);
}

// 设置半踏范围的处理器
void handleSetHalfPedal()
{
  bool updated = false;
  if (server.hasArg("lower") && server.hasArg("upper"))
  {
    int lower = server.arg("lower").toInt();
    int upper = server.arg("upper").toInt();
    // 调用main.cpp中的设置函数
    otaPortalSetHalfPedalRange_mV(lower, upper);
    // 更新本地缓存
    halfPedalLower_mV = lower;
    halfPedalUpper_mV = upper;
    updated = true;
  }
  if (server.hasArg("voltage"))
  {
    float voltage = server.arg("voltage").toFloat();
    otaPortalSetHalfPedalVoltage(voltage);
    halfPedalVoltage = voltage;
    updated = true;
  }
  if (updated)
  {
    server.send(200, "application/json", "{\"success\":true}");
  }
  else
  {
    server.send(400, "application/json", "{\"success\":false,\"error\":\"missing parameters\"}");
  }
}

// 半踏范围API实现 - 调用main.cpp中的函数
int otaPortalGetHalfPedalLower_mV()
{
  extern int GetSustainHalfPedalLower_mV();
  return GetSustainHalfPedalLower_mV();
}

int otaPortalGetHalfPedalUpper_mV()
{
  extern int GetSustainHalfPedalUpper_mV();
  return GetSustainHalfPedalUpper_mV();
}

void otaPortalSetHalfPedalRange_mV(int lower_mV, int upper_mV)
{
  extern void SetSustainHalfPedalRange_mV(int lower_mV, int upper_mV);
  SetSustainHalfPedalRange_mV(lower_mV, upper_mV);
}

float otaPortalGetHalfPedalVoltage()
{
  extern float GetHalfPedalVoltage();
  return GetHalfPedalVoltage();
}

void otaPortalSetHalfPedalVoltage(float voltage)
{
  extern void SetHalfPedalVoltage(float voltage);
  SetHalfPedalVoltage(voltage);
}

// 设置半踏功能开关的处理器
void handleSetHalfPedalEnabled()
{
  if (server.hasArg("enabled"))
  {
    bool enabled = server.arg("enabled") == "1" || server.arg("enabled") == "true";
    otaPortalSetHalfPedalEnabled(enabled);
    halfPedalEnabled = enabled;
    server.send(200, "application/json", "{\"success\":true}");
  }
  else
  {
    server.send(400, "application/json", "{\"success\":false,\"error\":\"missing enabled parameter\"}");
  }
}

// 半踏功能开关API实现 - 调用main.cpp中的函数
bool otaPortalGetHalfPedalEnabled()
{
  extern bool GetHalfPedalEnabled();
  return GetHalfPedalEnabled();
}

void otaPortalSetHalfPedalEnabled(bool enabled)
{
  extern void SetHalfPedalEnabled(bool enabled);
  SetHalfPedalEnabled(enabled);
}

// 设置最大DAC输出电压的处理器
void handleSetMaxDACVoltage()
{
  if (server.hasArg("voltage"))
  {
    float voltage = server.arg("voltage").toFloat();
    otaPortalSetMaxDACVoltage(voltage);
    maxDACVoltage = voltage;
    server.send(200, "application/json", "{\"success\":true}");
  }
  else
  {
    server.send(400, "application/json", "{\"success\":false,\"error\":\"missing voltage parameter\"}");
  }
}

// 最大DAC输出电压API实现
float otaPortalGetMaxDACVoltage()
{
  extern float GetMaxDACVoltage();
  return GetMaxDACVoltage();
}

void otaPortalSetMaxDACVoltage(float voltage)
{
  extern void SetMaxDACVoltage(float voltage);
  SetMaxDACVoltage(voltage);
}

const char index_html[] PROGMEM = R"rawliteral(
<!doctype html>
<html lang="zh-CN">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width,initial-scale=1">
  <title>固件更新</title>
  <style>
    body{font-family:Segoe UI,Roboto,Arial;background:#f5f7fb;color:#222;margin:0;padding:20px}
    .card{max-width:720px;margin:30px auto;padding:20px;background:#fff;border-radius:8px;box-shadow:0 6px 18px rgba(0,0,0,0.08)}
    h1{font-size:20px;margin:0 0 10px}
    p.note{color:#666;font-size:13px}
    .row{margin:12px 0}
    input[type=file]{width:100%}
    .btn{display:inline-block;padding:10px 16px;border-radius:6px;background:#0078d4;color:#fff;text-decoration:none;border:none;cursor:pointer}
    .btn:disabled{opacity:0.5}
    .progress{width:100%;height:14px;background:#eee;border-radius:8px;overflow:hidden}
    .progress > i{display:block;height:100%;width:0;background:linear-gradient(90deg,#4caf50,#8bc34a);transition:width 150ms}
    .status{margin-top:8px;font-size:13px}
    .small{font-size:12px;color:#888}
    .vprogress{width:60px;height:140px;background:#eee;border-radius:8px;position:relative;margin:8px auto;overflow:hidden}
    .vprogress>i{position:absolute;left:0;bottom:0;width:100%;height:0;background:linear-gradient(180deg,#4caf50,#8bc34a);transition:height 120ms;border-radius:0 0 8px 8px}
    .pedal-row{display:flex;gap:12px;justify-content:space-between}
    .pedal-label{font-weight:600;margin-bottom:6px}
    .vprogress .vmax, .vprogress .vmin{position:absolute;left:50%;transform:translateX(-50%);color:#444;font-size:12px;font-weight:600}
    .vprogress .vmax{top:6px}
    .vprogress .vmin{bottom:6px}
    .copy-btn{display:inline-block;margin-left:6px;padding:2px 6px;border:1px solid #ccc;border-radius:3px;background:#f8f9fa;color:#666;font-size:11px;cursor:pointer;transition:all 0.2s}
    .copy-btn:hover{background:#e9ecef;border-color:#999}
    .copy-btn:active{background:#dee2e6;transform:scale(0.95)}
    /* 半踏范围滑动标志样式 */
    .half-marker{position:absolute;left:0;width:60px;height:4px;background:#ff9800;cursor:ns-resize;z-index:10;opacity:0.8;border-radius:2px}
    .half-marker:hover{opacity:1;background:#f57c00}
    .half-marker.upper{background:#2196f3}
    .half-marker.upper:hover{background:#1976d2}
    .half-zone{position:absolute;left:0;width:100%;background:rgba(255,152,0,0.15);pointer-events:none}
    .half-label{font-size:11px;color:#ff9800;margin-top:4px}
    /* 延音踏板容器（为半踏数字留出空间） */
    .sustain-container{position:relative}
    .sustain-container .vprogress{margin-left:45px}
    /* 半踏数字显示（放在容器中，避免被overflow:hidden裁剪） */
    .half-marker-val{position:absolute;left:3px;width:38px;text-align:right;font-size:10px;color:#ff9800;font-weight:600;pointer-events:none;z-index:5}
    .half-marker-val.upper{color:#2196f3}
    /* 设置区域样式 */
    .settings-section{margin-top:20px;padding-top:16px;border-top:1px solid #eee}
    .settings-row{display:flex;align-items:center;gap:12px;margin:8px 0}
    .settings-label{font-size:13px;color:#444;min-width:100px}
    .settings-select{padding:6px 10px;border:1px solid #ccc;border-radius:4px;font-size:13px;background:#fff;cursor:pointer;min-width:80px}
    /* 开关样式 */
    .toggle-switch{position:relative;width:50px;height:26px}
    .toggle-switch input{opacity:0;width:0;height:0}
    .toggle-slider{position:absolute;cursor:pointer;top:0;left:0;right:0;bottom:0;background-color:#ccc;transition:0.3s;border-radius:26px}
    .toggle-slider:before{position:absolute;content:"";height:20px;width:20px;left:3px;bottom:3px;background-color:white;transition:0.3s;border-radius:50%}
    .toggle-switch input:checked + .toggle-slider{background-color:#4caf50}
    .toggle-switch input:checked + .toggle-slider:before{transform:translateX(24px)}
    .toggle-status{font-size:12px;color:#666;margin-left:8px}
  </style>
</head>
<body>
  <div class="card">
    <h1>延音踏板 固件在线更新</h1>
    <p class="note small">当前固件版本：)rawliteral" FW_VERSION R"rawliteral(</p>
    <p class="note" style="color:#d32f2f;font-weight:bold;">注意：使用在线更新功能时无法使用蓝牙翻页</p>
    <p class="note">在此页面上传编译生成的固件（.bin）。上传完成设备将自动重启。</p>

    <div class="row">
      <label>选择固件文件（.bin）</label>
      <input id="file" type="file" accept=".bin" />
    </div>

    <div class="row">
      <button id="uploadBtn" class="btn">开始上传</button>
      <button id="cancelBtn" class="btn" style="background:#999;margin-left:8px;">取消</button>
    </div>

    <div class="row">
      <div class="progress"><i id="bar"></i></div>
      <div class="status" id="status">准备就绪</div>
      <div class="small">提示：若浏览器未自动打开本页，请在地址栏输入 <strong style="color:#0078d4;">http://192.168.4.1</strong> <button id="copyBtn" class="copy-btn" onclick="copyToClipboard()" title="复制地址">📋</button></div>
    </div>

    <!-- 三个竖向进度条显示踏板实时状态 -->
    <div class="row">
      <div class="pedal-row">
        <div style="flex:1;text-align:center">
          <div class="pedal-label" id="v0_label">弱音踏板</div>
          <div class="vprogress" id="v0"><div class="vmax">0</div><i></i><div class="vmin">0</div></div>
          <div class="small" id="v0_txt">输出: 0 mV</div>
        </div>
        <div style="flex:1;text-align:center">
          <div class="pedal-label" id="v1_label">持音踏板</div>
          <div class="vprogress" id="v1"><div class="vmax">0</div><i></i><div class="vmin">0</div></div>
          <div class="small" id="v1_txt">输出: 0 mV</div>
        </div>
        <div style="flex:1;text-align:center">
          <div class="pedal-label" id="v2_label">延音踏板</div>
          <div class="sustain-container">
            <!-- 半踏数字显示（放在容器中，避免被overflow:hidden裁剪） -->
            <div class="half-marker-val" id="halfLowerVal">1500</div>
            <div class="half-marker-val upper" id="halfUpperVal">2500</div>
            <div class="vprogress" id="v2">
              <div class="vmax">0</div>
              <i></i>
              <div class="vmin">0</div>
              <!-- 半踏范围滑动标志 -->
              <div class="half-zone" id="halfZone"></div>
              <div class="half-marker lower" id="halfLower"></div>
              <div class="half-marker upper" id="halfUpper"></div>
            </div>
          </div>
          <div class="small" id="v2_txt">输出: 0 mV</div>
          <div class="half-label" id="halfLabel">半踏范围: 1500 - 2500 mV</div>
        </div>
      </div>
    </div>

    <!-- 设置区域 -->
    <div class="settings-section">
      <div class="settings-row">
        <span class="settings-label">最大输出:</span>
        <select id="maxDACVoltageSelect" class="settings-select">
          <option value="0.1">0.1V</option>
          <option value="0.2">0.2V</option>
          <option value="0.3">0.3V</option>
          <option value="0.4">0.4V</option>
          <option value="0.5">0.5V</option>
          <option value="0.6">0.6V</option>
          <option value="0.7">0.7V</option>
          <option value="0.8">0.8V</option>
          <option value="0.9">0.9V</option>
          <option value="1.0">1.0V</option>
          <option value="1.1">1.1V</option>
          <option value="1.2">1.2V</option>
          <option value="1.3">1.3V</option>
          <option value="1.4">1.4V</option>
          <option value="1.5">1.5V</option>
          <option value="1.6">1.6V</option>
          <option value="1.7">1.7V</option>
          <option value="1.8">1.8V</option>
          <option value="1.9" selected>1.9V</option>
          <option value="2.0">2.0V</option>
          <option value="2.1">2.1V</option>
          <option value="2.2">2.2V</option>
          <option value="2.3">2.3V</option>
          <option value="2.4">2.4V</option>
          <option value="2.5">2.5V</option>
          <option value="2.6">2.6V</option>
          <option value="2.7">2.7V</option>
          <option value="2.8">2.8V</option>
          <option value="2.9">2.9V</option>
          <option value="3.0">3.0V</option>
          <option value="3.1">3.1V</option>
          <option value="3.2">3.2V</option>
          <option value="3.3">3.3V</option>
        </select>
      </div>
      <div class="settings-row">
        <span class="settings-label">半踏兼容模式:</span>
        <label class="toggle-switch">
          <input type="checkbox" id="halfPedalEnabled">
          <span class="toggle-slider"></span>
        </label>
        <span class="toggle-status" id="halfPedalStatus">关闭</span>
      </div>
      <div class="settings-row">
        <span class="settings-label">半踏电压:</span>
        <select id="voltageSelect" class="settings-select">
          <option value="0.1">0.1V</option>
          <option value="0.2">0.2V</option>
          <option value="0.3">0.3V</option>
          <option value="0.4">0.4V</option>
          <option value="0.5">0.5V</option>
          <option value="0.6">0.6V</option>
          <option value="0.7">0.7V</option>
          <option value="0.8">0.8V</option>
          <option value="0.9">0.9V</option>
          <option value="1.0">1.0V</option>
          <option value="1.1">1.1V</option>
          <option value="1.2">1.2V</option>
          <option value="1.3">1.3V</option>
          <option value="1.4">1.4V</option>
          <option value="1.5">1.5V</option>
          <option value="1.6">1.6V</option>
          <option value="1.7" selected>1.7V</option>
          <option value="1.8">1.8V</option>
          <option value="1.9">1.9V</option>
          <option value="2.0">2.0V</option>
          <option value="2.1">2.1V</option>
          <option value="2.2">2.2V</option>
          <option value="2.3">2.3V</option>
          <option value="2.4">2.4V</option>
          <option value="2.5">2.5V</option>
          <option value="2.6">2.6V</option>
          <option value="2.7">2.7V</option>
          <option value="2.8">2.8V</option>
          <option value="2.9">2.9V</option>
          <option value="3.0">3.0V</option>
          <option value="3.1">3.1V</option>
          <option value="3.2">3.2V</option>
        </select>
      </div>
    </div>
  </div>

  <script>
    const fileEl = document.getElementById('file');
    const uploadBtn = document.getElementById('uploadBtn');
    const cancelBtn = document.getElementById('cancelBtn');
    const bar = document.getElementById('bar');
    const status = document.getElementById('status');
    let xhr = null;

    function setStatus(s){ status.textContent = s; }
    function setProgress(p){ bar.style.width = p + '%'; }

    uploadBtn.addEventListener('click', function(){
      const f = fileEl.files[0];
      if(!f){ setStatus('请先选择一个 .bin 文件'); return; }
      uploadBtn.disabled = true;
      setStatus('开始上传...');
      setProgress(0);
      const fd = new FormData();
      fd.append('update', f);

      xhr = new XMLHttpRequest();
      xhr.open('POST', '/update', true);
      xhr.upload.onprogress = function(e){
        if(e.lengthComputable){
          const pct = Math.round(e.loaded / e.total * 100);
          setProgress(pct);
          setStatus('上传中：' + pct + '%');
        }
      };
      xhr.onload = function(){
        if(xhr.status===200){
          setProgress(100);
          setStatus('上传完成，设备将重启并应用新固件');
        } else {
          setStatus('上传失败：HTTP ' + xhr.status);
        }
        uploadBtn.disabled = false;
      };
      xhr.onerror = function(){ setStatus('上传发生错误'); uploadBtn.disabled = false; };
      xhr.send(fd);
    });

    cancelBtn.addEventListener('click', function(){
      if(xhr){ xhr.abort(); setStatus('已取消'); setProgress(0); uploadBtn.disabled=false; }
    });

    // 半踏范围变量（mV值）
    let halfLower = 1500;
    let halfUpper = 2500;
    let halfVoltage = 1.7;
    let halfEnabled = false;
    let maxDACVoltage = 1.9;
    const v2Progress = document.getElementById('v2');
    const halfLowerEl = document.getElementById('halfLower');
    const halfUpperEl = document.getElementById('halfUpper');
    const halfZoneEl = document.getElementById('halfZone');
    const halfLabelEl = document.getElementById('halfLabel');
    const voltageSelectEl = document.getElementById('voltageSelect');
    const halfEnabledEl = document.getElementById('halfPedalEnabled');
    const halfStatusEl = document.getElementById('halfPedalStatus');
    const maxDACVoltageSelectEl = document.getElementById('maxDACVoltageSelect');
    const halfLowerValEl = document.getElementById('halfLowerVal');
    const halfUpperValEl = document.getElementById('halfUpperVal');
    const minMv = 0;
    const maxMv = 3300;

    // mV值转换为进度条百分比（延音踏板的min/max范围）
    let sustainMin = 0;
    let sustainMax = 3300;

    // 更新半踏范围显示
    function updateHalfMarkers() {
      // 使用延音踏板的实际范围计算位置
      const range = sustainMax - sustainMin;
      if (range <= 0) return;
      
      // 计算百分比位置
      const lowerPct = (halfLower - sustainMin) / range * 100;
      const upperPct = (halfUpper - sustainMin) / range * 100;
      
      // lower标志在下方，upper标志在上方
      halfLowerEl.style.bottom = Math.max(0, Math.min(100, lowerPct)) + '%';
      halfUpperEl.style.bottom = Math.max(0, Math.min(100, upperPct)) + '%';
      
      // 更新数字显示位置（放在容器中，使用bottom定位与滑条对齐）
      // 进度条高度140px，margin-top 8px
      // 数字中心需要与指示条中心对齐
      const vprogressHeight = 140;
      const vprogressMarginTop = 8;
      const markerHeight = 4;  // 指示条高度
      const valHeight = 12;    // 数字高度约12px（font-size:10px + line-height）
      
      // 计算指示条中心位置（相对于容器底部）
      // 指示条bottom是百分比，中心位置 = bottom + markerHeight/2
      const lowerMarkerCenter = vprogressMarginTop + vprogressHeight * (lowerPct / 100) + markerHeight / 2;
      const upperMarkerCenter = vprogressMarginTop + vprogressHeight * (upperPct / 100) + markerHeight / 2;
      
      // 数字底部位置 = 指示条中心 - 数字高度/2（让数字中心与指示条中心对齐）
      const lowerBottom = lowerMarkerCenter - valHeight / 2;
      const upperBottom = upperMarkerCenter - valHeight / 2;
      halfLowerValEl.style.bottom = lowerBottom + 'px';
      halfUpperValEl.style.bottom = upperBottom + 'px';
      halfLowerValEl.textContent = halfLower;
      halfUpperValEl.textContent = halfUpper;
      
      // 更新半踏区域显示
      halfZoneEl.style.bottom = Math.max(0, lowerPct) + '%';
      halfZoneEl.style.height = Math.max(0, upperPct - lowerPct) + '%';
      halfLabelEl.textContent = '半踏范围: ' + halfLower + ' - ' + halfUpper + ' mV';
      // 设置下拉框选中对应电压
      voltageSelectEl.value = halfVoltage.toFixed(1);
      maxDACVoltageSelectEl.value = maxDACVoltage.toFixed(1);
      // 更新开关状态
      halfEnabledEl.checked = halfEnabled;
      halfStatusEl.textContent = halfEnabled ? '开启' : '关闭';
      // 根据开关状态调整半踏区域显示
      halfZoneEl.style.opacity = halfEnabled ? 1 : 0.3;
      halfLowerEl.style.opacity = halfEnabled ? 0.8 : 0.3;
      halfUpperEl.style.opacity = halfEnabled ? 0.8 : 0.3;
    }

    // 拖动处理
    let dragging = null;
    function onDragStart(e, marker) {
      e.preventDefault();
      dragging = marker;
      document.addEventListener('mousemove', onDragMove);
      document.addEventListener('mouseup', onDragEnd);
      document.addEventListener('touchmove', onDragMove, {passive:false});
      document.addEventListener('touchend', onDragEnd);
    }
    function onDragMove(e) {
      if (!dragging) return;
      e.preventDefault();
      const rect = v2Progress.getBoundingClientRect();
      const clientY = e.touches ? e.touches[0].clientY : e.clientY;
      // 计算相对位置（从底部开始）
      let pct = (rect.bottom - clientY) / rect.height * 100;
      pct = Math.max(0, Math.min(100, pct));
      // 转换为mV值
      let mV = Math.round(sustainMin + pct / 100 * (sustainMax - sustainMin));
      mV = Math.max(minMv, Math.min(maxMv, mV));
      
      if (dragging === 'lower') {
        halfLower = Math.min(mV, halfUpper);
      } else {
        halfUpper = Math.max(mV, halfLower);
      }
      updateHalfMarkers();
    }
    function onDragEnd() {
      if (dragging) {
        // 拖动结束，保存设置到设备
        fetch('/setHalfPedal?lower=' + halfLower + '&upper=' + halfUpper);
      }
      dragging = null;
      document.removeEventListener('mousemove', onDragMove);
      document.removeEventListener('mouseup', onDragEnd);
      document.removeEventListener('touchmove', onDragMove);
      document.removeEventListener('touchend', onDragEnd);
    }
    halfLowerEl.addEventListener('mousedown', e => onDragStart(e, 'lower'));
    halfUpperEl.addEventListener('mousedown', e => onDragStart(e, 'upper'));
    halfLowerEl.addEventListener('touchstart', e => onDragStart(e, 'lower'), {passive:false});
    halfUpperEl.addEventListener('touchstart', e => onDragStart(e, 'upper'), {passive:false});

    // 电压选择处理
    voltageSelectEl.addEventListener('change', function() {
      halfVoltage = parseFloat(voltageSelectEl.value);
      fetch('/setHalfPedal?voltage=' + halfVoltage);
    });

    // 最大DAC输出电压选择处理
    maxDACVoltageSelectEl.addEventListener('change', function() {
      maxDACVoltage = parseFloat(maxDACVoltageSelectEl.value);
      fetch('/setMaxDACVoltage?voltage=' + maxDACVoltage);
    });

    // 半踏功能开关处理
    halfEnabledEl.addEventListener('change', function() {
      halfEnabled = halfEnabledEl.checked;
      halfStatusEl.textContent = halfEnabled ? '开启' : '关闭';
      fetch('/setHalfPedalEnabled?enabled=' + (halfEnabled ? '1' : '0'));
      updateHalfMarkers();
    });

    // 网页加载时获取一次半踏设置
    function loadHalfPedalSettings() {
      fetch('/status').then(r=>r.json()).then(j=>{
        if (j.halfPedal) {
          halfLower = j.halfPedal.lower;
          halfUpper = j.halfPedal.upper;
          if (j.halfPedal.voltage !== undefined) {
            halfVoltage = j.halfPedal.voltage;
          }
          if (j.halfPedal.enabled !== undefined) {
            halfEnabled = j.halfPedal.enabled === true;
          }
          if (j.halfPedal.maxDACVoltage !== undefined) {
            maxDACVoltage = j.halfPedal.maxDACVoltage;
          }
          updateHalfMarkers();
        }
      });
    }
    loadHalfPedalSettings();

    // 轮询 /status 更新三个踏板的竖向进度条
    function updatePedals(){
      fetch('/status').then(r=>r.json()).then(j=>{
        for(let i=0;i<3;i++){
          const p = j['p'+i];
          if(!p) continue;
          const pct = Math.round(p.mapped / 255 * 100);
          const h = Math.max(0, Math.min(100, pct));
          document.querySelector('#v'+i+' > i').style.height = h+'%';
          document.getElementById('v'+i+'_txt').textContent = '输出: ' + p.mv + ' mV';
          // 将 min/max 显示在进度条顶部/底部
          const vmaxEl = document.querySelector('#v'+i+' .vmax');
          const vminEl = document.querySelector('#v'+i+' .vmin');
          if (vmaxEl) vmaxEl.textContent = p.max;
          if (vminEl) vminEl.textContent = p.min;
          // 更新延音踏板的范围
          if (i === 2) {
            sustainMin = p.min;
            sustainMax = p.max;
            // 更新滑动标志位置（使用本地值）
            updateHalfMarkers();
          }
        }
      }).catch(e=>{ /* ignore network errors while uploading */ });
    }
    setInterval(updatePedals, 100);

    // 复制地址到剪贴板功能
    function copyToClipboard() {
      const url = 'http://192.168.4.1';
      const btn = document.getElementById('copyBtn');
      
      if (navigator.clipboard && window.isSecureContext) {
        navigator.clipboard.writeText(url).then(() => {
          showCopyFeedback(btn, '✓');
        }).catch(() => {
          fallbackCopy(url, btn);
        });
      } else {
        fallbackCopy(url, btn);
      }
    }

    // 降级复制方案
    function fallbackCopy(text, btn) {
      const textArea = document.createElement('textarea');
      textArea.value = text;
      textArea.style.position = 'fixed';
      textArea.style.left = '-999999px';
      textArea.style.top = '-999999px';
      document.body.appendChild(textArea);
      textArea.focus();
      textArea.select();
      
      try {
        document.execCommand('copy');
        showCopyFeedback(btn, '✓');
      } catch (err) {
        showCopyFeedback(btn, '✗');
      }
      
      document.body.removeChild(textArea);
    }

    // 显示复制反馈
    function showCopyFeedback(btn, icon) {
      const originalText = btn.innerHTML;
      btn.innerHTML = icon;
      btn.style.background = icon === '✓' ? '#d4edda' : '#f8d7da';
      btn.style.borderColor = icon === '✓' ? '#c3e6cb' : '#f5c6cb';
      
      setTimeout(() => {
        btn.innerHTML = originalText;
        btn.style.background = '#f8f9fa';
        btn.style.borderColor = '#ccc';
      }, 1500);
    }
  </script>
</body>
</html>
)rawliteral";

void handleRoot()
{
  server.send_P(200, "text/html", index_html);
}

void handleUpdate()
{
  server.sendHeader("Connection", "close");
  if (Update.hasError())
  {
    server.send(500, "text/plain", "FAIL");
    DBG_PRINTLN("/update 返回 500：更新期间发生错误");
  }
  else
  {
    server.send(200, "text/plain", "OK");
    DBG_PRINTLN("/update 返回 200：更新成功，即将重启...");
    // 发送响应后延迟重启
    delay(500);
    ESP.restart();
  }
}

void handleUpload()
{
  HTTPUpload &upload = server.upload();

  // 在主循环的 otaPortalHandle 中会检查并触发重启
  static unsigned long restartAt = 0;
  if (upload.status == UPLOAD_FILE_START)
  {
    DBG_PRINTF("开始更新固件: %s\n", upload.filename.c_str());
    if (!Update.begin(UPDATE_SIZE_UNKNOWN))
    { // 以最大可用大小开始
      DBG_PRINTLN("出出出出错了");
      Update.printError(Serial);
    }
  }
  else if (upload.status == UPLOAD_FILE_WRITE)
  {
    // 将接收到的数据写入Update对象
    if (Update.write(upload.buf, upload.currentSize) != upload.currentSize)
    {
      Update.printError(Serial);
    }
  }
  else if (upload.status == UPLOAD_FILE_END)
  {
    if (Update.end(true))
    { // 设置大小为当前大小
      DBG_PRINTF("更新成功: %u bytes\n", upload.totalSize);
      DBG_PRINTLN("等待响应发送后重启...");
      // 不立即重启，等待handleUpdate发送响应
    }
    else
    {
      Update.printError(Serial);
    }
  }

  if (restartAt != 0 && millis() >= restartAt)
  {
  }
}

void otaPortalBegin()
{
  if (active)
    return;
  active = true;
  
  // 从main.cpp加载存储的半踏配置
  halfPedalLower_mV = otaPortalGetHalfPedalLower_mV();
  halfPedalUpper_mV = otaPortalGetHalfPedalUpper_mV();
  halfPedalVoltage = otaPortalGetHalfPedalVoltage();
  halfPedalEnabled = otaPortalGetHalfPedalEnabled();
  maxDACVoltage = otaPortalGetMaxDACVoltage();
  
  WiFi.mode(WIFI_AP);
  const char *ssid = "钢琴踏板固件更新";
  // 使用无密码开放热点，并限制最大连接数为 1（避免多人同时连接）
  // 参数：ssid, password (NULL 表示无密码), channel(1), ssid_hidden(0), max_connection(1)
  WiFi.softAP(ssid, NULL, 1, 0, 1);
  delay(200);
  // 尝试设置固定IP（确保 AP IP 可知）
  IPAddress apIP = WiFi.softAPIP();
  DBG_PRINT("SoftAP 地址：");
  DBG_PRINTLN(apIP);
  // 如果需要，可强制设置为 192.168.4.1
  IPAddress desiredIP(192, 168, 4, 1);
  if (apIP != desiredIP)
  {
    DBG_PRINTLN("尝试将 SoftAP IP 设置为 192.168.4.1");
    if (WiFi.softAPConfig(desiredIP, desiredIP, IPAddress(255, 255, 255, 0)))
    {
      DBG_PRINTLN("softAPConfig 成功");
      apIP = WiFi.softAPIP();
      DBG_PRINT("新的 SoftAP 地址：");
      DBG_PRINTLN(apIP);
    }
    else
    {
      DBG_PRINTLN("softAPConfig 失败");
    }
  }
  dnsServer.start(53, "*", apIP);
  server.on("/", HTTP_GET, handleRoot);
  server.on("/status", HTTP_GET, handleStatus);
  server.on("/setHalfPedal", HTTP_GET, handleSetHalfPedal);
  server.on("/setHalfPedalEnabled", HTTP_GET, handleSetHalfPedalEnabled);
  server.on("/setMaxDACVoltage", HTTP_GET, handleSetMaxDACVoltage);
  server.on("/update", HTTP_POST, handleUpdate, handleUpload);
  // 捕获所有未命中的请求并重定向到根页面，配合 DNS 劫持可以实现 captive-portal 风格自动弹出
  server.onNotFound([]() {
    // 指定完整 URL 以便某些客户端正确打开
    server.sendHeader("Location", String("http://") + WiFi.softAPIP().toString() + "/");
    server.send(302, "text/plain", "");
  });
  server.begin();
  DBG_PRINT("OTA 门户已启动，地址：");
  DBG_PRINTLN(WiFi.softAPIP().toString());
}

void otaPortalHandle()
{
  if (!active)
    return;
  dnsServer.processNextRequest();
  server.handleClient();
  static unsigned long lastStatus = 0;
  if (millis() - lastStatus > 2000)
  {
    lastStatus = millis();
    String ssid = WiFi.softAPSSID();
    IPAddress ip = WiFi.softAPIP();
    wifi_mode_t mode = WiFi.getMode();
    int clients = WiFi.softAPgetStationNum();
    DBG_PRINTF("[OTA] AP='%s' IP=%s 模式=%d 连接数=%d\n", ssid.c_str(), ip.toString().c_str(), (int)mode, clients);
  }
}

void otaPortalStop()
{
  if (!active)
    return;
  server.stop();
  dnsServer.stop();
  WiFi.softAPdisconnect(true);
  active = false;
}

bool otaPortalActive() { return active; }
