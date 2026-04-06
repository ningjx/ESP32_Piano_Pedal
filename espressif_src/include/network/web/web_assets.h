#ifndef WEB_ASSETS_H
#define WEB_ASSETS_H

// 内嵌的 HTML 页面 - 与 Arduino 项目完全一致
static const char WEB_PAGE_HTML[] = R"rawliteral(
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
    .vprogress{width:60px;height:140px;background:#eee;border-radius:8px;position:relative;margin:8px auto;overflow:visible}
    .vprogress>i{position:absolute;left:0;bottom:0;width:100%;height:0;background:linear-gradient(180deg,#4caf50,#8bc34a);transition:height 120ms;border-radius:0 0 8px 8px}
    .pedal-row{display:flex;gap:12px;justify-content:space-between}
    .pedal-label{font-weight:600;margin-bottom:6px}
    .vprogress .vmax, .vprogress .vmin{position:absolute;left:50%;transform:translateX(-50%);color:#444;font-size:12px;font-weight:600}
    .vprogress .vmax{top:6px}
    .vprogress .vmin{bottom:6px}
    .copy-btn{display:inline-block;margin-left:6px;padding:2px 6px;border:1px solid #ccc;border-radius:3px;background:#f8f9fa;color:#666;font-size:11px;cursor:pointer;transition:all 0.2s}
    .copy-btn:hover{background:#e9ecef;border-color:#999}
    .copy-btn:active{background:#dee2e6;transform:scale(0.95)}
    .half-marker{position:absolute;left:-8px;width:76px;height:4px;background:#ff9800;cursor:ns-resize;z-index:10;opacity:0.8;border-radius:2px}
    .half-marker:hover{opacity:1;background:#f57c00}
    .half-marker::after{content:attr(data-val);position:absolute;right:-4px;top:-18px;font-size:10px;color:#ff9800;font-weight:600}
    .half-marker.upper{background:#2196f3}
    .half-marker.upper:hover{background:#1976d2}
    .half-marker.upper::after{color:#2196f3}
    .half-zone{position:absolute;left:0;width:100%;background:rgba(255,152,0,0.15);pointer-events:none}
    .half-label{font-size:11px;color:#ff9800;margin-top:4px}
    .settings-section{margin-top:20px;padding-top:16px;border-top:1px solid #eee}
    .settings-row{display:flex;align-items:center;gap:12px;margin:8px 0}
    .settings-label{font-size:13px;color:#444;min-width:100px}
    .settings-select{padding:6px 10px;border:1px solid #ccc;border-radius:4px;font-size:13px;background:#fff;cursor:pointer;min-width:80px}
  </style>
</head>
<body>
  <div class="card">
    <h1>延音踏板 固件在线更新</h1>
    <p class="note small">当前固件版本：v0.2 (ESP-IDF)</p>
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
          <div class="small" id="v0_txt">0 mV</div>
        </div>
        <div style="flex:1;text-align:center">
          <div class="pedal-label" id="v1_label">持音踏板</div>
          <div class="vprogress" id="v1"><div class="vmax">0</div><i></i><div class="vmin">0</div></div>
          <div class="small" id="v1_txt">0 mV</div>
        </div>
        <div style="flex:1;text-align:center">
          <div class="pedal-label" id="v2_label">延音踏板</div>
          <div class="vprogress" id="v2">
            <div class="vmax">0</div>
            <i></i>
            <div class="vmin">0</div>
            <div class="half-zone" id="halfZone"></div>
            <div class="half-marker lower" id="halfLower" data-val="1500"></div>
            <div class="half-marker upper" id="halfUpper" data-val="2500"></div>
          </div>
          <div class="small" id="v2_txt">0 mV</div>
          <div class="half-label" id="halfLabel">半踏范围: 1500 - 2500 mV</div>
        </div>
      </div>
    </div>

    <!-- 设置区域 -->
    <div class="settings-section">
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

    let halfLower = 1500;
    let halfUpper = 2500;
    let halfVoltage = 1.7;
    const v2Progress = document.getElementById('v2');
    const halfLowerEl = document.getElementById('halfLower');
    const halfUpperEl = document.getElementById('halfUpper');
    const halfZoneEl = document.getElementById('halfZone');
    const halfLabelEl = document.getElementById('halfLabel');
    const voltageSelectEl = document.getElementById('voltageSelect');
    const minMv = 0;
    const maxMv = 3300;

    let sustainMin = 0;
    let sustainMax = 3300;

    function updateHalfMarkers() {
      const range = sustainMax - sustainMin;
      if (range <= 0) return;
      
      const lowerPct = (halfLower - sustainMin) / range * 100;
      const upperPct = (halfUpper - sustainMin) / range * 100;
      
      halfLowerEl.style.bottom = Math.max(0, Math.min(100, lowerPct)) + '%';
      halfUpperEl.style.bottom = Math.max(0, Math.min(100, upperPct)) + '%';
      halfLowerEl.setAttribute('data-val', halfLower);
      halfUpperEl.setAttribute('data-val', halfUpper);
      
      halfZoneEl.style.bottom = Math.max(0, lowerPct) + '%';
      halfZoneEl.style.height = Math.max(0, upperPct - lowerPct) + '%';
      halfLabelEl.textContent = '半踏范围: ' + halfLower + ' - ' + halfUpper + ' mV';
      voltageSelectEl.value = halfVoltage.toFixed(1);
    }

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
      let pct = (rect.bottom - clientY) / rect.height * 100;
      pct = Math.max(0, Math.min(100, pct));
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

    voltageSelectEl.addEventListener('change', function() {
      halfVoltage = parseFloat(voltageSelectEl.value);
      fetch('/setHalfPedal?voltage=' + halfVoltage);
    });

    function loadHalfPedalSettings() {
      fetch('/status').then(r=>r.json()).then(j=>{
        if (j.halfPedal) {
          halfLower = j.halfPedal.lower;
          halfUpper = j.halfPedal.upper;
          if (j.halfPedal.voltage !== undefined) {
            halfVoltage = j.halfPedal.voltage;
          }
          updateHalfMarkers();
        }
      });
    }
    loadHalfPedalSettings();

    function updatePedals(){
      fetch('/status').then(r=>r.json()).then(j=>{
        for(let i=0;i<3;i++){
          const p = j['p'+i];
          if(!p) continue;
          const pct = Math.round(p.mapped / 255 * 100);
          const h = Math.max(0, Math.min(100, pct));
          document.querySelector('#v'+i+' > i').style.height = h+'%';
          document.getElementById('v'+i+'_txt').textContent = p.mv + ' mV';
          const vmaxEl = document.querySelector('#v'+i+' .vmax');
          const vminEl = document.querySelector('#v'+i+' .vmin');
          if (vmaxEl) vmaxEl.textContent = p.max;
          if (vminEl) vminEl.textContent = p.min;
          if (i === 2) {
            sustainMin = p.min;
            sustainMax = p.max;
            updateHalfMarkers();
          }
        }
      }).catch(e=>{ });
    }
    setInterval(updatePedals, 100);

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

#endif // WEB_ASSETS_H
