/*
 *  Вариант со сдвигами в одном буфере
 *  Версия 8 июля 2025г.
 */
#include "destaff.h"
#include "project_config.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "esp_system.h"
#include "esp_err.h"
#include "esp_log.h"

static const char *TAG = "DESTAFF";

// Позиции STX и ETX (Глобальные переменные)
int stx_position = -1;
int etx_position = -1;

int deStaff(uint8_t *input, size_t len) // input одновременно и output
{
    // Проверка валидности входных аргументов
    if (input == NULL || len == 0)
        return 0;
    if (len < BUF_MIN_SIZE || len > UART_BUF_SIZE * 2)
        return 0;

    size_t read_idx = 0;
    size_t write_idx = 0;

    while (read_idx < len)
    {
        // Проверяем текущий и следующий байт, если есть
        if (input[read_idx] == DLE && (read_idx + 1 < len))
        {
            uint8_t next_byte = input[read_idx + 1];

            // Проверяем следующий байт на совпадение с целевыми значениями
            bool should_remove = (next_byte == SOH || next_byte == STX ||
                                  next_byte == ETX || next_byte == ISI);

            if (should_remove)
            {
                if (next_byte == STX)
                    stx_position = (int)(write_idx);
                if (next_byte == ETX)
                    etx_position = (int)(write_idx);

                // Пропускаем запись текущего байта (DLE)
                read_idx++; // Переходим к следующему байту после DLE

                continue;
            }
        }

        // Копируем текущий байт в новую позицию
        input[write_idx] = input[read_idx];
        write_idx++;
        read_idx++;
    }

    /* Проверка условий корректности:
    - STX должен существовать (stx_position != -1)
    - ETX должен существовать (etx_position != -1)
    - STX должен находиться строго перед ETX (stx_position < etx_position)
    */
    if (stx_position == -1 || etx_position == -1 || stx_position >= etx_position)
    {
        ESP_LOGE(TAG, "Ошибка формата: STX/ETX не найдены или нарушен порядок");
        // Возвращаем специальный код ошибки
        return -1;
    }

    ESP_LOGI(TAG, "Позиции STX: %d byte: 0x%02X", stx_position, input[stx_position]);
    ESP_LOGI(TAG, "Позиции ETX: %d byte: 0x%02X", etx_position, input[etx_position]);
    // Возвращаем новую длину сообщения
    return write_idx;
}
