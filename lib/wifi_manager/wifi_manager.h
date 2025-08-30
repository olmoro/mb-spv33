#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "project_config.h"


// Объявление функции для доступа к состоянию
wifi_condition_t get_wifi_mode(void);

void start_wifi_manager_task(void);

#ifdef __cplusplus
}
#endif

#endif // WIFI_MANAGER_H
