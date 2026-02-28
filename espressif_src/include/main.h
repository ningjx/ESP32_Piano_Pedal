/*
 * Piano Pedal Application Header
 * Function declarations for main pedal control application
 */
#pragma once

#include "config.h"

/* Function Declarations */
bool check_button(int pin);
bool check_button_long(int pin, unsigned long hold_ms);
void save_calibration(void);
void read_calibration(void);
void start_calibration(void);
void finish_calibration(void);
int adc_remap(int pin, int min_v, int max_v, float dead_zone_pct);
void beep_tone(int degree, int duration_ms);
unsigned long get_pageturner_continue_time(bool is_down);
void read_bluetooth_active(void);
void save_bluetooth_active(void);
void shutdown_bluetooth(void);
void init_hardware(void);
void pedal_loop(void);

#endif /* MAIN_H */
