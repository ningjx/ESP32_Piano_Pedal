#include <Arduino.h>
#include <Preferences.h>
#include <BleKeyboard.h>
#include "ota_portal.h"
#include <WiFi.h>
#include <esp_wifi.h>
#include "esp_adc_cal.h"
#include "esp_bt.h"
#include "esp_bt_main.h"

//#define DEBUG

// 调试宏
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

/**
  三个按钮的上拉电阻（R7，R8，R9）可以不焊，用内部上拉，
  按键只焊接一个持音踏板的按钮（SW9）就够了，
  用来触发校准功能，一旦校准完成，
  就可以用霍尔传感器检测三个踏板的位置，
  并且将三个踏板作为开关触发其他功能
**/

// 参数持久化
Preferences prefs;
// DAC配置
#define DAC_Sustain_PIN 25   // 延音踏板电压输出
#define DAC_Sostenuto_PIN 26 // 持音踏板电压输出
#define Switch_Soft_PIN 17   // 开关型踏板输出（因为只有俩DAC，所以还有一个踏板只能用开关了）

// ADC配置
#define ADC_Sustain_PIN 35   // ADC1_CH4 (GPIO32)延音踏板检测霍尔
#define ADC_Sostenuto_PIN 32 // ADC1_CH5 (GPIO33)持音踏板检测霍尔
#define ADC_Soft_PIN 33      // ADC1_CH7 (GPIO35)弱音踏板检测霍尔

// 按钮配置（低电平有效）
#define Sustain_BUTTON_PIN 27   // 延音踏板检测按钮
#define Sostenuto_BUTTON_PIN 14 // 持音踏板检测按钮
#define Soft_BUTTON_PIN 13      // 弱音踏板检测按钮

// 功能按键绑定
#define Calibrate_Button Sostenuto_BUTTON_PIN // 持音踏板按钮触发校准功能

// 蜂鸣器PWM配置
#define BUZZER_PIN 16
const int PWM_CHANNEL = 0;    // LEDC通道0
const int PWM_FREQ = 2000;    // 2KHz频率
const int PWM_RESOLUTION = 8; // 8位分辨率 (0-255)

// 霍尔范围校准参数
int Sustain_Pedal_MIN;
int Sustain_Pedal_MAX;
int Sostenuto_Pedal_MIN;
int Sostenuto_Pedal_MAX;
int Soft_Pedal_MIN;
int Soft_Pedal_MAX;

// 校准功能相关参数
bool InCalibration = false;
unsigned long calibrationStartMs = 0;
const unsigned long calibrationTimeoutMs = 20000; // 20 秒超时
bool calibrationCanceled = false;

// 翻页功能踩下计时
#define LongPressTimeMs 1000

// 蓝牙模式
int Bluetooth_Mode; // 0:关闭 1:蓝牙MIDI 2:蓝牙键盘

// 蓝牙键盘
BleKeyboard bleKeyboard("翻页器", "Ning", 100);

// ADC 校准结构
static esp_adc_cal_characteristics_t adc_chars;

bool CheckButton(int pin);
void SaveCalibration();
void ReadCalibration();
void StartCalibration();
void FinishCalibration();
bool CheckButtonLong(int pin, unsigned long holdMs);
int AdcRemap(int pin, int minV, int maxV, float deadZonePct = 0.05f);
void BeepTone(int degree, int duration_ms);
unsigned long GetPageturnerContinueTime(bool isDown);

void setup()
{
  setCpuFrequencyMhz(80);
  
  DBG_BEGIN(115200);

  // 读取配置
  ReadCalibration();

  // 配置按钮引脚（启用内部上拉）
  pinMode(Sustain_BUTTON_PIN, INPUT_PULLUP);
  pinMode(Sostenuto_BUTTON_PIN, INPUT_PULLUP);
  pinMode(Soft_BUTTON_PIN, INPUT_PULLUP);

  // 配置弱音踏板信号输出引脚
  pinMode(Switch_Soft_PIN, OUTPUT);
  digitalWrite(Switch_Soft_PIN, LOW);

  // 配置蜂鸣器PWM
  ledcSetup(PWM_CHANNEL, PWM_FREQ, PWM_RESOLUTION);
  ledcAttachPin(BUZZER_PIN, PWM_CHANNEL);
  ledcWrite(PWM_CHANNEL, 0);

  // ADC初始化
  analogSetPinAttenuation(ADC_Sustain_PIN, ADC_11db);
  analogSetPinAttenuation(ADC_Sostenuto_PIN, ADC_11db);
  analogSetPinAttenuation(ADC_Soft_PIN, ADC_11db);
  analogSetWidth(12);
  esp_adc_cal_characterize(ADC_UNIT_1, (adc_atten_t)ADC_11db, ADC_WIDTH_BIT_12, 1100, &adc_chars);

  /**
  校准功能
  开机时踩住[持音踏板]，进入校准模式并蜂鸣(Do So)提示开始校准
  将三个踏板分别踩到底和松开，记录最大最小值
  踩住[持音踏板]2秒完成校准并保存，蜂鸣(Do长音)提示
  如果没有主动结束校准，则校准模式会在20秒后自动关闭，蜂鸣(So Do)提示，并且不保存本次校准结果
  **/
  if (digitalRead(Calibrate_Button) == LOW)
  {
    StartCalibration();
  }

  // OTA更新功能
  // 开机时踩住[弱音踏板]，则启动 OTA 上传固件网页
  int sustainValue = AdcRemap(ADC_Soft_PIN, Sustain_Pedal_MIN, Sustain_Pedal_MAX);
  if (sustainValue > 127) //(1)
  {
    BeepTone(1, 120);
    BeepTone(3, 120);
    BeepTone(5, 120);
    // 禁用蓝牙堆栈以避免 WiFi OTA 时与 BLE 冲突导致卡死
    // 尝试安全地停用蓝牙控制器和蓝牙守护进程
    esp_bluedroid_disable();
    esp_bluedroid_deinit();
    esp_bt_controller_disable();
    esp_bt_controller_deinit();
    delay(100);
    otaPortalBegin();
  }
  else
  {
    // 关闭WIFI节约功耗，保留 BLE
    // 先断开并关闭 WiFi
    WiFi.disconnect(true);
    delay(50);
    WiFi.mode(WIFI_OFF);
    // 停止并反初始化底层 wifi 驱动（确保无线子系统彻底关闭）
    esp_wifi_stop();
    esp_wifi_deinit();
  }

  /**
  蓝牙翻页功能(开启OTA模式时，请勿启动此功能，避免内存溢出死机)
  使用平板或手机等设备连接名为[翻页器]的蓝牙设备
  短踩持音踏板下一页，长踩踏板上一页
  当连接蓝牙之后，踏板的持音功能将不可用，断开蓝牙后恢复正常
  **/
  if (!otaPortalActive()) // WIFI更新固件和蓝牙翻页不能同时使用
    bleKeyboard.begin();
}

void loop()
{
#ifdef DEBUG
  unsigned long loopStartMs = millis();
#endif

  // 校准模式
  if (InCalibration)
  {
    int susVoltage = esp_adc_cal_raw_to_voltage(analogRead(ADC_Sustain_PIN), &adc_chars);
    int sosVoltage = esp_adc_cal_raw_to_voltage(analogRead(ADC_Sostenuto_PIN), &adc_chars);
    int sofVoltage = esp_adc_cal_raw_to_voltage(analogRead(ADC_Soft_PIN), &adc_chars);

    // 更新 min/max
    if (susVoltage < Sustain_Pedal_MIN)
      Sustain_Pedal_MIN = susVoltage;
    if (susVoltage > Sustain_Pedal_MAX)
      Sustain_Pedal_MAX = susVoltage;

    if (sosVoltage < Sostenuto_Pedal_MIN)
      Sostenuto_Pedal_MIN = sosVoltage;
    if (sosVoltage > Sostenuto_Pedal_MAX)
      Sostenuto_Pedal_MAX = sosVoltage;

    if (sofVoltage < Soft_Pedal_MIN)
      Soft_Pedal_MIN = sofVoltage;
    if (sofVoltage > Soft_Pedal_MAX)
      Soft_Pedal_MAX = sofVoltage;

    // 检查是否超时
    if (calibrationStartMs != 0 && (millis() - calibrationStartMs >= calibrationTimeoutMs))
    {
      // 超时：取消本次校准，恢复上次保存的参数
      InCalibration = false;
      calibrationCanceled = true;
      // 重新读取配置
      prefs.begin("config");
      Sustain_Pedal_MIN = prefs.getUInt("sustainmin", 3300);
      Sustain_Pedal_MAX = prefs.getUInt("sustainmax", 0);
      Sostenuto_Pedal_MIN = prefs.getUInt("sostenutomin", 3300);
      Sostenuto_Pedal_MAX = prefs.getUInt("sostenutomax", 0);
      Soft_Pedal_MIN = prefs.getUInt("softmin", 3300);
      Soft_Pedal_MAX = prefs.getUInt("softmax", 0);
      prefs.end();
      DBG_PRINTLN("校准超时：已取消本次校准并恢复上次参数");
      DBG_PRINTF("[重新读取配置] Sustain MIN=%dmV MAX=%dmV | Sostenuto MIN=%dmV MAX=%dmV | Soft MIN=%dmV MAX=%dmV\n",
                 Sustain_Pedal_MIN, Sustain_Pedal_MAX, Sostenuto_Pedal_MIN, Sostenuto_Pedal_MAX, Soft_Pedal_MIN, Soft_Pedal_MAX);
      // 给出蜂鸣提示
      BeepTone(5, 120);
      BeepTone(1, 120);
      // 清空起始时间
      calibrationStartMs = 0;
    }

    // 长按3秒完成校准
    if (CheckButtonLong(Calibrate_Button, 2000))
    {
      FinishCalibration();
    }
    return;
  }

  // OTA更新处理
  if (otaPortalActive())
  {
    otaPortalHandle();
    // return;
  }

  // 读取踏板数值 0 - 255
  int sustainValue = AdcRemap(ADC_Sustain_PIN, Sustain_Pedal_MIN, Sustain_Pedal_MAX);
  int sostenutoValue = AdcRemap(ADC_Sostenuto_PIN, Sostenuto_Pedal_MIN, Sostenuto_Pedal_MAX);
  int softValue = AdcRemap(ADC_Soft_PIN, Soft_Pedal_MIN, Soft_Pedal_MAX);

  // 输出延音信号
  dacWrite(DAC_Sustain_PIN, sustainValue * 2 / 3);

  // 输出持音信号
  // 如果连接蓝牙翻页，就不再输出持音踏板信号
  if (!bleKeyboard.isConnected())
    dacWrite(DAC_Sostenuto_PIN, sostenutoValue * 2 / 3);

  // 输出弱音开关信号
  // 控制 Switch_Soft: 若 Soft_BUTTON 按下则高电平，否则低
  digitalWrite(Switch_Soft_PIN, softValue > 127 ? HIGH : LOW);

  // 翻页功能
  if (bleKeyboard.isConnected())
  {
    bool pageTurnerDown = false;
    if (sostenutoValue > 100)
      pageTurnerDown = true;
    else if (sostenutoValue < 90)
      pageTurnerDown = false;

    unsigned long downTime = GetPageturnerContinueTime(pageTurnerDown);

    if (downTime == LongPressTimeMs)
    {
      bleKeyboard.write(KEY_PAGE_UP);
      // DBG_PRINTF("[状态] 踩下时间%d\n", downTime);
    }
    else if (downTime > 0 && downTime < LongPressTimeMs)
    {
      bleKeyboard.write(KEY_PAGE_DOWN);
      // DBG_PRINTF("[状态] 踩下时间%d\n", downTime);
    }
  }

  // 日志打印（前三项固定三位宽，前导零）
  DBG_PRINTF("[状态] 延音输入:%03d | 持音输入:%03d | 弱音输入:%03d | 开销:%dms\n", sustainValue, sostenutoValue, softValue, millis() - loopStartMs);
  delay(10);
}

// 带防抖的按钮检测函数
bool CheckButton(int pin)
{
  if (digitalRead(pin) == LOW)
  {
    delay(10);
    if (digitalRead(pin) == LOW)
    {
      return true;
    }
  }
  return false;
}

void SaveCalibration()
{
  prefs.begin("config", false);
  prefs.putInt("sustainmin", Sustain_Pedal_MIN);
  prefs.putInt("sustainmax", Sustain_Pedal_MAX);
  prefs.putInt("sostenutomin", Sostenuto_Pedal_MIN);
  prefs.putInt("sostenutomax", Sostenuto_Pedal_MAX);
  prefs.putInt("softmin", Soft_Pedal_MIN);
  prefs.putInt("softmax", Soft_Pedal_MAX);
  prefs.end();
  DBG_PRINTF("[保存参数] Sustain MIN=%dmV MAX=%dmV | Sostenuto MIN=%dmV MAX=%dmV | Soft MIN=%dmV MAX=%dmV\n",
             Sustain_Pedal_MIN, Sustain_Pedal_MAX, Sostenuto_Pedal_MIN, Sostenuto_Pedal_MAX, Soft_Pedal_MIN, Soft_Pedal_MAX);
}

void ReadCalibration()
{
  prefs.begin("config", false);
  Sustain_Pedal_MIN = prefs.getInt("sustainmin", 5000);
  Sustain_Pedal_MAX = prefs.getInt("sustainmax", 0);
  Sostenuto_Pedal_MIN = prefs.getInt("sostenutomin", 5000);
  Sostenuto_Pedal_MAX = prefs.getInt("sostenutomax", 0);
  Soft_Pedal_MIN = prefs.getInt("softmin", 5000);
  Soft_Pedal_MAX = prefs.getInt("softmax", 0);
  prefs.end();
  DBG_PRINTF("[读取参数] Sustain MIN=%dmV MAX=%dmV | Sostenuto MIN=%dmV MAX=%dmV | Soft MIN=%dmV MAX=%dmV\n",
             Sustain_Pedal_MIN, Sustain_Pedal_MAX, Sostenuto_Pedal_MIN, Sostenuto_Pedal_MAX, Soft_Pedal_MIN, Soft_Pedal_MAX);
}

// 霍尔范围校准
void StartCalibration()
{
  DBG_PRINTLN("开始校准...");
  InCalibration = true;
  calibrationCanceled = false;
  calibrationStartMs = millis();
  // 初始化 min/max 确保后续采样能正确更新范围
  // ADC 电压单位为 mV，设置初始 min 为较大值，max 为 0
  Sustain_Pedal_MIN = 5000;
  Sustain_Pedal_MAX = 0;
  Sostenuto_Pedal_MIN = 5000;
  Sostenuto_Pedal_MAX = 0;
  Soft_Pedal_MIN = 5000;
  Soft_Pedal_MAX = 0;
  // 蜂鸣提示
  BeepTone(1, 120);
  BeepTone(5, 120);
}

// 完成校准
void FinishCalibration()
{
  // 如果之前超时被取消，则不保存
  InCalibration = false;
  if (!calibrationCanceled)
  {
    SaveCalibration();
    // 蜂鸣提示
    BeepTone(5, 240);
    DBG_PRINTLN("校准完成");
  }
  else
  {
    DBG_PRINTLN("校准已被取消，未保存本次参数");
  }
  calibrationStartMs = 0;
  calibrationCanceled = false;
}

// 检测长按
bool CheckButtonLong(int pin, unsigned long holdMs)
{
  static unsigned long pinStartTimes[40] = {0};
  int idx = pin % 40;
  if (digitalRead(pin) == LOW)
  {
    if (pinStartTimes[idx] == 0)
      pinStartTimes[idx] = millis();
    else if (millis() - pinStartTimes[idx] >= holdMs)
    {
      // 一次性触发：重置时间戳，等待松开再可触发下一次
      pinStartTimes[idx] = 0;
      return true;
    }
  }
  else
  {
    pinStartTimes[idx] = 0;
  }
  return false;
}

unsigned long GetPageturnerContinueTime(bool isDown)
{
  static unsigned long downStartMs = 0;
  static bool downing = false;
  static bool checked = false;

  if (isDown && !downing)
  {
    downStartMs = millis();
    downing = true;
    checked = false;
  }

  if (!isDown && downing)
  {
    downing = false;
    if (!checked)
      return millis() - downStartMs;
  }

  if ((millis() - downStartMs) >= LongPressTimeMs && downing && !checked)
  {
    checked = true;
    return LongPressTimeMs;
  }

  return 0;
}

// 将 ADC（基于校准范围）映射到 0 -255
int AdcRemap(int pin, int minV, int maxV, float deadZonePct)
{
  // 快速多次采样，降低量化与瞬时噪声（低延迟：无额外delay）
  int raw0 = analogRead(pin);
  int raw1 = analogRead(pin);
  int raw2 = analogRead(pin);
  int adcValue = (raw0 + raw1 + raw2) / 3;
  int adcVoltage = esp_adc_cal_raw_to_voltage(adcValue, &adc_chars);
  if (maxV <= minV)
    return 0;

  // 应用死区 (deadZonePct 例如 0.05 表示 5%)
  float dz = constrain(deadZonePct, 0.0f, 0.45f); // 防止死区重叠

  int reminV = minV + (maxV - minV) * dz;
  int remaxV = maxV - (maxV - minV) * dz;
  int adcVol = constrain(adcVoltage, reminV, remaxV);
  // 计算百分比
  float pct = (float)(adcVol - reminV) / (float)(remaxV - reminV);
  int valueRaw = 255 * pct;

  // 低延迟平滑与消抖：自适应EMA + 步进限幅 + 微抖动死区
  int idx = pin % 40;
  static bool s_inited[40] = {false};
  static float s_ema[40] = {0};
  static int s_lastOut[40] = {0};

  if (!s_inited[idx])
  {
    s_inited[idx] = true;
    s_ema[idx] = (float)valueRaw;
    s_lastOut[idx] = valueRaw;
  }
  else
  {
    float delta = (float)valueRaw - s_ema[idx];
    float alpha = (abs((int)delta) > 15) ? 0.7f : 0.2f; // 大幅变化快速跟随，小抖动更稳
    s_ema[idx] = s_ema[idx] + alpha * delta;

    int emaInt = (int)(s_ema[idx] + (s_ema[idx] >= 0 ? 0.5f : -0.5f));

    // 微抖动死区：差值≤1 不更新，避免1级跳动
    if (abs(emaInt - s_lastOut[idx]) <= 1)
    {
      // 保持上次输出
    }
    else
    {
      // 限制单次步进，避免过快跳变但保持低延迟响应
      const int maxStep = 12; // 0..255 空间下单次最大变化
      int step = emaInt - s_lastOut[idx];
      if (step > maxStep)
        step = maxStep;
      else if (step < -maxStep)
        step = -maxStep;
      s_lastOut[idx] = s_lastOut[idx] + step;
    }
  }

  int value = constrain(s_lastOut[idx], 0, 255);
  // if (pin == ADC_Sostenuto_PIN)
  //   DBG_PRINTF("[状态] adc%d | 电压:%d | 死区:%f | 范围:%d-%d | 重设电压:%d | 百分比:%f | 映射:%d\n", adcValue, adcVoltage, value, dz, reminV, remaxV, adcVol, pct);

  // 更新网页上的踏板实时数据
  if (otaPortalActive())
  {
    // 传递：index, mv, min, max, mapped
    if (pin == ADC_Sustain_PIN)
    {
      otaPortalSetPedalStatus(2, adcVoltage, minV, maxV, value);
    }
    else if (pin == ADC_Sostenuto_PIN)
    {
      otaPortalSetPedalStatus(1, adcVoltage, minV, maxV, value);
    }
    else if (pin == ADC_Soft_PIN)
    {
      otaPortalSetPedalStatus(0, adcVoltage, minV, maxV, value);
    }
  }

  return value;
}

// 蜂鸣器音调控制：degree 1-7 对应 C 大调音阶（C D E F G A B）
// duration_ms 为持续时间（毫秒），若 duration_ms<=0 则持续播放直到再次调用停止
void BeepTone(int degree, int duration_ms)
{
  // 基础音阶（C4..B4）频率，单位 Hz
  const uint16_t freqs[7] = {262, 294, 330, 349, 392, 440, 494};
  if (degree < 1 || degree > 7)
    return;

  uint16_t freq = freqs[degree - 1];
  // 使用 ledcWriteTone 产生指定频率
  ledcWriteTone(PWM_CHANNEL, freq);
  if (duration_ms > 0)
  {
    delay(duration_ms);
    // 停止蜂鸣
    ledcWrite(PWM_CHANNEL, 0);
  }
}