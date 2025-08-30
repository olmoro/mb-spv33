#include "staff.h"
#include "project_config.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "esp_system.h"
#include "esp_err.h"
#include "esp_log.h"


// static const char *TAG = "STAFF";

int staff(const uint8_t *input, size_t input_len, uint8_t *output, size_t output_max_len)
{
    size_t j = 0;
    for (size_t i = 0; i < input_len; i++)
    {
        // Определяем, сколько байт нужно записать
        size_t required = (input[i] == SOH || input[i] == ISI || input[i] == STX || input[i] == ETX) ? 2 : 1;

        // Проверяем, достаточно ли места в целевом буфере
        if (j + required > output_max_len)
        {
            return 0;
        }

        // Вставляем байты согласно условию
        switch (input[i])
        {
        case SOH:
            output[j++] = DLE;
            output[j++] = SOH;
            break;
        case ISI:
            output[j++] = DLE;
            output[j++] = ISI;
            break;
        case STX:
            output[j++] = DLE;
            output[j++] = STX;
            break;
        case ETX:
            output[j++] = DLE;
            output[j++] = ETX;
            break;
        default:
            output[j++] = input[i];
        }
    }

    // ESP_LOGI(TAG, "Staffing (%d bytes):", j);
    // for (int i = 0; i < j; i++)
    // {
    //     printf("%02X ", output[i]);
    // }
    // printf("\n");

    return j;
}
