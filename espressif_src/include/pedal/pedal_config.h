#ifndef PEDAL_CONFIG_H
#define PEDAL_CONFIG_H

#include "pedal_core.h"

// 配置管理接口
void pedal_config_init(void);
pedal_config_t* pedal_config_get(void);
void pedal_config_set(const pedal_config_t *config);

#endif // PEDAL_CONFIG_H
