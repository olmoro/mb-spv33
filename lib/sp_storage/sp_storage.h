#ifndef SP_STORAGE_H
#define SP_STORAGE_H

#include <stdint.h>
#include "esp_err.h"
#include "time.h"

// Структура конфигурации
typedef struct
{
    uint32_t signature;        // Сигнатура валидации
    char sta_ssid[3][32];      // SSID для STA режима (3 сети)
    char sta_password[3][64];  // Пароли для STA
    char ap_ssid[32];          // SSID для AP режима
    char ap_password[64];      // Пароль для AP
    char serial_number[24];    // Серийный номер
    char firmware_version[16]; // Версия прошивки
    time_t last_update;        // Время последнего обновления
    uint32_t flags;            // Флаги конфигурации
} system_config_t;

void start_storage_task(void);

esp_err_t sp_storage_config_init(system_config_t *config);

esp_err_t request_read_file(uint8_t file_id, uint8_t *data);

esp_err_t request_write_file(uint8_t file_id, const uint8_t *data);

esp_err_t response_read_file(uint8_t file_id, uint8_t *data);

esp_err_t response_write_file(uint8_t file_id, const uint8_t *data);

#endif // SP_STORAGE_H
