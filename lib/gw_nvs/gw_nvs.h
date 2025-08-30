#ifndef _GW_NVS_H_
#define _GW_NVS_H_

#include <stdint.h>
#include <stdio.h>
#include "nvs_flash.h"

#ifdef __cplusplus
extern "C"
{
#endif

    void nvs_init();
    esp_err_t read_parameter_from_nvs(int i);
    esp_err_t write_parameter_to_nvs(int i, uint16_t value);

    void update_parameters_from_nvs();
    void write_defaults_to_nvs();
    // void custom_shutdown_handler();
    // void save_parameters_to_nvs();

    void mb_uart1_init();
    void sp_uart2_init();

#ifdef __cplusplus
}
#endif

#endif // !_GW_NVS_H_
