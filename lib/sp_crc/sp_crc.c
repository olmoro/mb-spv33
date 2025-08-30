#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "project_config.h"
#include "esp_err.h"
#include "esp_log.h"


/* Контрольные коды насчитывается с байта, следующего за SOH, поскольку два первых байта DLE
    и SOH проверяются явно при выделении начала сообщения. Контрольные коды охватывают все байты,
    включая ETX и все стаффинг символы в этом промежутке.
*/
uint16_t sp_crc16(const uint8_t *msg, size_t len)
{
    int j, crc = 0;
    while (len-- > 0)
    {
        crc = crc ^ (int)*msg++ << 8;
        for (j = 0; j < 8; j++)
        {
            if (crc & 0x8000)
                crc = (crc << 1) ^ 0x1021;
            else
                crc <<= 1;
        }
    }
    return crc;
}
