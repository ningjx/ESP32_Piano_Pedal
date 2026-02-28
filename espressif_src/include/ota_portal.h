/*
 * OTA Portal Header - Over-The-Air Firmware Update Web Interface
 * Ported to ESP-IDF
 */
#pragma once

#include "config.h"

/* Function Declarations */
void ota_portal_begin(void);
void ota_portal_handle(void);
void ota_portal_stop(void);
bool ota_portal_active(void);

/* Update pedal status for display on OTA web page
 * index: 0=soft, 1=sostenuto, 2=sustain
 * mv: voltage in mV
 * minv: minimum voltage calibration value
 * maxv: maximum voltage calibration value
 * mapped: mapped value 0-255
 */
void ota_portal_set_pedal_status(int index, int mv, int minv, int maxv, int mapped);

#endif /* OTA_PORTAL_H */
