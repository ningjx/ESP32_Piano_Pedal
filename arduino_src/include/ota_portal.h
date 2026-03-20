// ota_portal.h
#pragma once
#include <Arduino.h>

void otaPortalBegin();
void otaPortalHandle();
void otaPortalStop();
bool otaPortalActive();
// 更新 OTA 页面上踏板状态：index 0=soft,1=sostenuto,2=sustain
extern "C" void otaPortalSetPedalStatus(int index, int mv, int minv, int maxv, int mapped);
// 半踏范围相关API（使用mV值）
int otaPortalGetHalfPedalLower_mV();
int otaPortalGetHalfPedalUpper_mV();
void otaPortalSetHalfPedalRange_mV(int lower_mV, int upper_mV);
float otaPortalGetHalfPedalVoltage();
void otaPortalSetHalfPedalVoltage(float voltage);
