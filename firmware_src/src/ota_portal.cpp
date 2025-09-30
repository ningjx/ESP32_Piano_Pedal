#include "ota_portal.h"
#include <WiFi.h>
#include <WebServer.h>
#include <Update.h>
#include <DNSServer.h>
#include <Preferences.h>

//#define DEBUG

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
  json += "}";
  server.send(200, "application/json", json);
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
    .vprogress{width:60px;height:140px;background:#eee;border-radius:8px;position:relative;margin:8px auto}
    .vprogress>i{position:absolute;left:0;bottom:0;width:100%;height:0;background:linear-gradient(180deg,#4caf50,#8bc34a);transition:height 120ms}
    .pedal-row{display:flex;gap:12px;justify-content:space-between}
    .pedal-label{font-weight:600;margin-bottom:6px}
    .vprogress .vmax, .vprogress .vmin{position:absolute;left:50%;transform:translateX(-50%);color:#444;font-size:12px;font-weight:600}
    .vprogress .vmax{top:6px}
    .vprogress .vmin{bottom:6px}
  </style>
</head>
<body>
  <div class="card">
    <h1>延音踏板 固件在线更新</h1>
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
      <div class="small">提示：若浏览器未自动打开本页，请在地址栏输入 <strong>http://192.168.4.1</strong></div>
    </div>

    <!-- 三个竖向进度条显示踏板实时状态 -->
    <div class="row">
      <div class="pedal-row">
        <div style="flex:1;text-align:center">
          <div class="pedal-label" id="v0_label">弱音踏板</div>
          <div class="vprogress" id="v0"><div class="vmax">0</div><i></i><div class="vmin">0</div></div>
          <div class="small" id="v0_txt">0 mV (min:0 max:0) → 0</div>
        </div>
        <div style="flex:1;text-align:center">
          <div class="pedal-label" id="v1_label">持音踏板</div>
          <div class="vprogress" id="v1"><div class="vmax">0</div><i></i><div class="vmin">0</div></div>
          <div class="small" id="v1_txt">0 mV (min:0 max:0) → 0</div>
        </div>
        <div style="flex:1;text-align:center">
          <div class="pedal-label" id="v2_label">延音踏板</div>
          <div class="vprogress" id="v2"><div class="vmax">0</div><i></i><div class="vmin">0</div></div>
          <div class="small" id="v2_txt">0 mV (min:0 max:0) → 0</div>
        </div>
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

    // 轮询 /status 更新三个踏板的竖向进度条
    function updatePedals(){
      fetch('/status').then(r=>r.json()).then(j=>{
        for(let i=0;i<3;i++){
          const p = j['p'+i];
          if(!p) continue;
          const pct = Math.round(p.mapped / 255 * 100);
          const h = Math.max(0, Math.min(100, pct));
          document.querySelector('#v'+i+' > i').style.height = h+'%';
          document.getElementById('v'+i+'_txt').textContent = `${p.mv}`;
          // 将 min/max 显示在进度条顶部/底部
          const vmaxEl = document.querySelector('#v'+i+' .vmax');
          const vminEl = document.querySelector('#v'+i+' .vmin');
          if (vmaxEl) vmaxEl.textContent = p.max;
          if (vminEl) vminEl.textContent = p.min;
        }
      }).catch(e=>{ /* ignore network errors while uploading */ });
    }
    setInterval(updatePedals, 100);
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
    DBG_PRINTLN("/update 返回 200：更新成功或无错误");
  }
  delay(100);
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
      DBG_PRINTLN("执行重启...");
      delay(100);
      ESP.restart();
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
