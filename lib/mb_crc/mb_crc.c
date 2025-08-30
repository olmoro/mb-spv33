/**                 Расчёт контрольной суммы Modbus RTU.
 * Ключевые моменты:
 * Порядок байтов: В пакете сначала идет младший байт (LSB), затем старший (MSB).
 * Например, для CRC 0x1234, в пакет записывается 0x34 0x12.
 * 
 * Полином: Используется полином 0xA001 (отраженная версия 0x8005).
 * 
 * Проверка: Для тестирования можно использовать онлайн-калькуляторы, например
 * Modbus CRC Calculator https://crccalc.com/?crc=123456789&method=&datatype=ascii&outtype=hex.
 * 
 * Примечания:
 * Алгоритм корректен для всех версий ESP-IDF.
 * Убедитесь, что в пакет не включен CRC до его расчета.
 * Для работы с UART в Modbus RTU требуется коррекция порядка байтов и битов (обычно 8N1).
 */

#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "project_config.h"
#include "esp_err.h"
#include "esp_log.h"

uint16_t mb_crc16(const uint8_t *buffer, size_t length)
{
    uint16_t crc = 0xFFFF; // Инициализация CRC

    for (size_t i = 0; i < length; i++)
    {
        crc ^= (uint16_t)buffer[i]; // XOR с текущим байтом

        for (uint8_t j = 0; j < 8; j++)
        {
            if (crc & 0x0001) // Если младший бит равен 1
            {
                crc = (crc >> 1) ^ 0xA001; // Полином 0x8005 (отраженный)
            }
            else
            {
                crc >>= 1;
            }
        }
    }
    return crc;
}
