/**
 * Версия 42*96 с поддержкой трех разделов:
 *      request,  data, 0x40, 0x310000, 0x1000, encrypted
 *      response, data, 0x41, 0x311000, 0x1000, encrypted
 *      config,   data, 0x42, 0x312000, 0x1000, encrypted
 *
 * Раздел config хранит:
 *   - WiFi конфигурацию (STA_SSID, STA_PASSWORD, AP_SSID, AP_PASSWORD)
 *   - Серийный номер
 *   - Версию прошивки
 *   - Дату последнего обновления
 *   - Системные параметры
 *
 * Промышленный контроллер ESP32. ESP-IDF v5.4
 * 12 июля 2025г.
 */

#include "sp_storage.h"
#include "project_config.h"
#include "board.h"
#include "esp_partition.h"
#include "esp_log.h"
#include "spi_flash_mmap.h"
#include <string.h>
#include "inttypes.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "time.h"

#define SPI_FLASH_SEC_SIZE 4096     // Размер сектора SPI flash
#define SPI_FLASH_PAGE_SIZE 256     // Размер страницы SPI flash
#define CONFIG_SIGNATURE 0x55AAC3D9 // Сигнатура валидной конфигурации

extern uint16_t regs[];
extern uint8_t actual_bytes; // Актуальное количество байт данных

static const char *TAG = "STORAGE"; // Общий тег для логирования
static const esp_partition_t *request_partition = NULL;
static const esp_partition_t *response_partition = NULL;
static const esp_partition_t *config_partition = NULL;
SemaphoreHandle_t reg_mutex;

// Глобальная конфигурация
static system_config_t current_config;

// Регистры конфигурации
#define REG_CONFIG_OPERATION regs[0x1A] // Операция: [15] R/W, [14:8] тип, [7:0] индекс
#define REG_CONFIG_INDEX regs[0x1B]     // Индекс конфигурации
#define HLD_CONFIG_DATA_REG 0x20        // Регистры данных конфигурации

// Флаги операций
#define CONFIG_OP_READ 0x8000
#define CONFIG_OP_WRITE 0x0000

// Типы конфигурации
#define CONFIG_TYPE_STA0 0x01
#define CONFIG_TYPE_STA1 0x02
#define CONFIG_TYPE_STA2 0x03
#define CONFIG_TYPE_AP 0x04
#define CONFIG_TYPE_SN 0x05
#define CONFIG_TYPE_FW 0x06

// Инициализация разделов хранилища
esp_err_t storage_init(void)
{
    // Поиск разделов
    request_partition = esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA,
        ESP_PARTITION_SUBTYPE_ANY,
        "request");

    response_partition = esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA,
        ESP_PARTITION_SUBTYPE_ANY,
        "response");

    config_partition = esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA,
        ESP_PARTITION_SUBTYPE_ANY,
        "config");

    // Проверка наличия разделов
    if (!request_partition)
        ESP_LOGE(TAG, "Раздел 'request' не найден!");
    if (!response_partition)
        ESP_LOGE(TAG, "Раздел 'response' не найден!");
    if (!config_partition)
        ESP_LOGE(TAG, "Раздел 'config' не найден!");

    if (!request_partition || !response_partition || !config_partition)
    {
        return ESP_ERR_NOT_FOUND;
    }

    // Проверка размеров разделов
    if (request_partition->size < SP_STORAGE_FILE_COUNT * SP_STORAGE_FILE_SIZE)
    {
        ESP_LOGE(TAG, "Недостаточный размер 'request'! Требуется: %ld, доступно: %ld",
                 (long)SP_STORAGE_FILE_COUNT * SP_STORAGE_FILE_SIZE,
                 (long)request_partition->size);
        return ESP_ERR_INVALID_SIZE;
    }

    if (response_partition->size < SP_STORAGE_FILE_COUNT * SP_STORAGE_FILE_SIZE)
    {
        ESP_LOGE(TAG, "Недостаточный размер 'response'! Требуется: %ld, доступно: %ld",
                 (long)SP_STORAGE_FILE_COUNT * SP_STORAGE_FILE_SIZE,
                 (long)response_partition->size);
        return ESP_ERR_INVALID_SIZE;
    }

    if (config_partition->size < sizeof(system_config_t))
    {
        ESP_LOGE(TAG, "Недостаточный размер 'config'! Требуется: %d, доступно: %ld",
                 sizeof(system_config_t),
                 config_partition->size);
        return ESP_ERR_INVALID_SIZE;
    }

    // Логирование адресов
    ESP_LOGI(TAG, "'request': адрес 0x%lx, размер %ld",
             request_partition->address, request_partition->size);
    ESP_LOGI(TAG, "'response': адрес 0x%lx, размер %ld",
             response_partition->address, response_partition->size);
    ESP_LOGI(TAG, "'config': адрес 0x%lx, размер %ld",
             config_partition->address, config_partition->size);

    return ESP_OK;
}

// Общая функция чтения файла из раздела
static esp_err_t read_from_partition(const esp_partition_t *part, uint8_t file_id, uint8_t *data)
{
    if (file_id >= SP_STORAGE_FILE_COUNT || !part)
        return ESP_ERR_INVALID_ARG;

    size_t offset = file_id * SP_STORAGE_FILE_SIZE;
    return esp_partition_read(part, offset, data, SP_STORAGE_FILE_SIZE);
}

// Общая функция записи файла в раздел
static esp_err_t write_to_partition(const esp_partition_t *part, uint8_t file_id, const uint8_t *data)
{
    if (file_id >= SP_STORAGE_FILE_COUNT || !part)
        return ESP_ERR_INVALID_ARG;

    size_t offset = file_id * SP_STORAGE_FILE_SIZE;
    uint32_t sector_offset = (offset / SPI_FLASH_SEC_SIZE) * SPI_FLASH_SEC_SIZE;

    // Буфер для сектора
    uint8_t *sector_buf = malloc(SPI_FLASH_SEC_SIZE);
    if (!sector_buf)
        return ESP_ERR_NO_MEM;

    // Чтение сектора
    esp_err_t err = esp_partition_read(part, sector_offset, sector_buf, SPI_FLASH_SEC_SIZE);
    if (err != ESP_OK)
    {
        free(sector_buf);
        return err;
    }

    // Обновление данных
    size_t internal_offset = offset - sector_offset;
    memcpy(sector_buf + internal_offset, data, SP_STORAGE_FILE_SIZE);

    // Стирание сектора
    err = esp_partition_erase_range(part, sector_offset, SPI_FLASH_SEC_SIZE);
    if (err != ESP_OK)
    {
        free(sector_buf);
        return err;
    }

    // Постраничная запись
    for (int i = 0; i < SPI_FLASH_SEC_SIZE; i += SPI_FLASH_PAGE_SIZE)
    {
        err = esp_partition_write(part, sector_offset + i,
                                  sector_buf + i, SPI_FLASH_PAGE_SIZE);
        if (err != ESP_OK)
            break;
    }

    free(sector_buf);
    return err;
}

// =======================================================
// Функции для работы с конфигурацией
// =======================================================

// Сохранение конфигурации
esp_err_t config_save(const system_config_t *config)
{
    if (!config_partition)
        return ESP_ERR_INVALID_STATE;

    // Буфер для сектора
    uint8_t *sector_buf = malloc(SPI_FLASH_SEC_SIZE);
    if (!sector_buf)
        return ESP_ERR_NO_MEM;

    // Чтение сектора
    esp_err_t err = esp_partition_read(config_partition, 0,
                                       sector_buf, SPI_FLASH_SEC_SIZE);
    if (err != ESP_OK)
    {
        free(sector_buf);
        return err;
    }

    // Обновление данных конфигурации
    memcpy(sector_buf, config, sizeof(system_config_t));

    // Стирание сектора
    err = esp_partition_erase_range(config_partition, 0, SPI_FLASH_SEC_SIZE);
    if (err != ESP_OK)
    {
        free(sector_buf);
        return err;
    }

    // Запись
    err = esp_partition_write(config_partition, 0, sector_buf, SPI_FLASH_SEC_SIZE);
    free(sector_buf);
    return err;
}

// Инициализация конфигурации
// esp_err_t config_init(system_config_t *config)
esp_err_t sp_storage_config_init(system_config_t *config)
{
    if (!config_partition)
        return ESP_ERR_INVALID_STATE;

    // Чтение конфигурации
    esp_err_t err = esp_partition_read(config_partition, 0,
                                       config, sizeof(system_config_t));

    // Проверка валидности
    /** Обратите внимание: при первом запуске, если раздела `config` нет или он пуст, мы прочитаем
     * случайные данные. Поэтому нужно проверять на валидность (например, по какому-то полю).
     * Или инициализировать нулями и потом копировать из project_config.h.
     */
    if (err != ESP_OK || config->signature != CONFIG_SIGNATURE)
    {
        // Сброс к заводским настройкам
        memset(config, 0, sizeof(system_config_t));
        config->signature = CONFIG_SIGNATURE;

// Заполнение стандартными значениями
#ifdef STA_SSID1
        strlcpy(config->sta_ssid[0], STA_SSID1, sizeof(config->sta_ssid[0]));
#endif
#ifdef STA_PASSWORD1
        strlcpy(config->sta_password[0], STA_PASSWORD1, sizeof(config->sta_password[0]));
#endif

#ifdef STA_SSID2
        strlcpy(config->sta_ssid[1], STA_SSID2, sizeof(config->sta_ssid[1]));
#endif
#ifdef STA_PASSWORD2
        strlcpy(config->sta_password[1], STA_PASSWORD2, sizeof(config->sta_password[1]));
#endif

#ifdef STA_SSID3
        strlcpy(config->sta_ssid[2], STA_SSID3, sizeof(config->sta_ssid[2]));
#endif
#ifdef STA_PASSWORD3
        strlcpy(config->sta_password[2], STA_PASSWORD3, sizeof(config->sta_password[2]));
#endif

#ifdef AP_SSID
        strlcpy(config->ap_ssid, AP_SSID, sizeof(config->ap_ssid));
#endif
#ifdef AP_PASSWORD
        strlcpy(config->ap_password, AP_PASSWORD, sizeof(config->ap_password));
#endif

#ifdef SERIAL_NUMBER
        strlcpy(config->serial_number, SERIAL_NUMBER, sizeof(config->serial_number));
#endif

#ifdef FIRMWARE_VERSION
        strlcpy(config->firmware_version, FIRMWARE_VERSION, sizeof(config->firmware_version));
#endif

        config->last_update = time(NULL);
        config->flags = 0;

        // Сохранение конфигурации
        return config_save(config);
    }

    return ESP_OK;
}

// =======================================================
// Функции для обратной совместимости
// =======================================================

esp_err_t request_init(void) { return storage_init(); }

esp_err_t request_read_file(uint8_t file_id, uint8_t *data)
{
    return read_from_partition(request_partition, file_id, data);
}

esp_err_t request_write_file(uint8_t file_id, const uint8_t *data)
{
    return write_to_partition(request_partition, file_id, data);
}

esp_err_t response_read_file(uint8_t file_id, uint8_t *data)
{
    return read_from_partition(response_partition, file_id, data);
}

esp_err_t response_write_file(uint8_t file_id, const uint8_t *data)
{
    return write_to_partition(response_partition, file_id, data);
}

// Обработчик операций с хранилищем
void storage_handler_task(void *arg)
{
    // Инициализация конфигурации
    if (sp_storage_config_init(&current_config) != ESP_OK)
    {
        ESP_LOGE(TAG, "Ошибка инициализации конфигурации");
    }

    while (1)
    {
        if (xSemaphoreTake(reg_mutex, pdMS_TO_TICKS(100)))
        {
            // --- Обработка конфигурационных операций ---
            if (REG_CONFIG_OPERATION != 0xFFFF)
            {
                uint8_t config_idx = REG_CONFIG_INDEX & 0xFF;
                uint8_t config_type = (REG_CONFIG_OPERATION >> 8) & 0x7F;
                uint8_t is_read = (REG_CONFIG_OPERATION >> 15) & 1;

                ESP_LOGI(TAG, "Config op: %s type: %d idx: %d",
                         is_read ? "READ" : "WRITE", config_type, config_idx);

                if (is_read)
                {
                    // Операция чтения конфигурации
                    switch (config_type)
                    {
                    case CONFIG_TYPE_STA0:
                    case CONFIG_TYPE_STA1:
                    case CONFIG_TYPE_STA2:
                    {
                        int sta_idx = config_type - CONFIG_TYPE_STA0;
                        if (sta_idx >= 0 && sta_idx < 3)
                        {
                            // Чтение SSID
                            for (int i = 0; i < 16; i++)
                            {
                                uint16_t val = (current_config.sta_ssid[sta_idx][i * 2] << 8) |
                                               current_config.sta_ssid[sta_idx][i * 2 + 1];
                                regs[HLD_CONFIG_DATA_REG + i] = val;
                            }

                            // Чтение пароля (возвращаем звездочки для безопасности)
                            for (int i = 0; i < 32; i++)
                            {
                                regs[HLD_CONFIG_DATA_REG + 16 + i] = 0x2A2A; // "**"
                            }
                        }
                        break;
                    }

                    case CONFIG_TYPE_AP:
                    {
                        // Чтение AP SSID
                        for (int i = 0; i < 16; i++)
                        {
                            uint16_t val = (current_config.ap_ssid[i * 2] << 8) |
                                           current_config.ap_ssid[i * 2 + 1];
                            regs[HLD_CONFIG_DATA_REG + i] = val;
                        }

                        // Чтение пароля (звездочки)
                        for (int i = 0; i < 32; i++)
                        {
                            regs[HLD_CONFIG_DATA_REG + 16 + i] = 0x2A2A;
                        }
                        break;
                    }

                    case CONFIG_TYPE_SN:
                    {
                        // Чтение серийного номера
                        for (int i = 0; i < 12; i++)
                        {
                            uint16_t val = (current_config.serial_number[i * 2] << 8) |
                                           current_config.serial_number[i * 2 + 1];
                            regs[HLD_CONFIG_DATA_REG + i] = val;
                        }
                        break;
                    }

                    case CONFIG_TYPE_FW:
                    {
                        // Чтение версии прошивки
                        for (int i = 0; i < 8; i++)
                        {
                            uint16_t val = (current_config.firmware_version[i * 2] << 8) |
                                           current_config.firmware_version[i * 2 + 1];
                            regs[HLD_CONFIG_DATA_REG + i] = val;
                        }
                        break;
                    }
                    }
                }
                else
                {
                    // Операция записи конфигурации
                    switch (config_type)
                    {
                    case CONFIG_TYPE_STA0:
                    case CONFIG_TYPE_STA1:
                    case CONFIG_TYPE_STA2:
                    {
                        int sta_idx = config_type - CONFIG_TYPE_STA0;
                        if (sta_idx >= 0 && sta_idx < 3)
                        {
                            char ssid[32] = {0};
                            char password[64] = {0};

                            // Чтение SSID из регистров
                            for (int i = 0; i < 16; i++)
                            {
                                uint16_t val = regs[HLD_CONFIG_DATA_REG + i];
                                ssid[i * 2] = val >> 8;
                                ssid[i * 2 + 1] = val & 0xFF;
                            }

                            // Чтение пароля из регистров
                            for (int i = 0; i < 32; i++)
                            {
                                uint16_t val = regs[HLD_CONFIG_DATA_REG + 16 + i];
                                password[i * 2] = val >> 8;
                                password[i * 2 + 1] = val & 0xFF;
                            }

                            // Обновление конфигурации
                            strlcpy(current_config.sta_ssid[sta_idx], ssid,
                                    sizeof(current_config.sta_ssid[0]));
                            strlcpy(current_config.sta_password[sta_idx], password,
                                    sizeof(current_config.sta_password[0]));

                            ESP_LOGI(TAG, "Updated STA%d: SSID=%s", sta_idx, ssid);
                        }
                        break;
                    }

                    case CONFIG_TYPE_AP:
                    {
                        char ssid[32] = {0};
                        char password[64] = {0};

                        // Чтение SSID
                        for (int i = 0; i < 16; i++)
                        {
                            uint16_t val = regs[HLD_CONFIG_DATA_REG + i];
                            ssid[i * 2] = val >> 8;
                            ssid[i * 2 + 1] = val & 0xFF;
                        }

                        // Чтение пароля
                        for (int i = 0; i < 32; i++)
                        {
                            uint16_t val = regs[HLD_CONFIG_DATA_REG + 16 + i];
                            password[i * 2] = val >> 8;
                            password[i * 2 + 1] = val & 0xFF;
                        }

                        strlcpy(current_config.ap_ssid, ssid,
                                sizeof(current_config.ap_ssid));
                        strlcpy(current_config.ap_password, password,
                                sizeof(current_config.ap_password));

                        ESP_LOGI(TAG, "Updated AP: SSID=%s", ssid);
                        break;
                    }

                    case CONFIG_TYPE_SN:
                    {
                        char serial[24] = {0};
                        for (int i = 0; i < 12; i++)
                        {
                            uint16_t val = regs[HLD_CONFIG_DATA_REG + i];
                            serial[i * 2] = val >> 8;
                            serial[i * 2 + 1] = val & 0xFF;
                        }
                        strlcpy(current_config.serial_number, serial,
                                sizeof(current_config.serial_number));
                        ESP_LOGI(TAG, "Updated SN: %s", serial);
                        break;
                    }
                    }

                    // Обновление метки времени
                    current_config.last_update = time(NULL);

                    // Сохранение конфигурации
                    if (config_save(&current_config) == ESP_OK)
                    {
                        ESP_LOGI(TAG, "Configuration saved");
                    }
                    else
                    {
                        ESP_LOGE(TAG, "Error saving config");
                    }
                }

                // Сброс флагов операции
                REG_CONFIG_OPERATION = 0xFFFF;
                REG_CONFIG_INDEX = 0xFFFF;
            }

            // --- Обработка разделов request/response ---
            // Обработка чтения REQUEST
            if (REG_SP_READ_REQ != 0xFFFF)
            {
                uint8_t file_id = REG_SP_READ_REQ & 0xFF;
                uint8_t file_buf[SP_STORAGE_FILE_SIZE];

                if (file_id < SP_STORAGE_FILE_COUNT)
                {
                    if (request_read_file(file_id, file_buf) == ESP_OK)
                    {
                        // Если первый байт 0xFF - файл не существует
                        if (file_buf[0] == 0xFF)
                        {
                            memset(file_buf, 0, SP_STORAGE_FILE_SIZE);
                        }
                        else
                        {
                            // Коррекция длины данных
                            uint8_t length = file_buf[0];
                            if (length > SP_STORAGE_FILE_SIZE - 1)
                                length = SP_STORAGE_FILE_SIZE - 1;

                            // Обнуляем только заголовок для несуществующих файлов
                            if (length == 0)
                            {
                                memset(file_buf, 0, SP_STORAGE_FILE_SIZE);
                            }
                        }

                        // Упаковка в регистры
                        for (int i = 0; i < SP_STORAGE_FILE_SIZE / 2; i++)
                        {
                            regs[HLD_READ_REG + i] = (file_buf[i * 2] << 8) | file_buf[i * 2 + 1];
                        }
                    }
                }
                REG_SP_READ_REQ = 0xFFFF; // Сброс флага
            }

            // Обработка записи REQUEST
            if (REG_SP_WRITE_REQ != 0xFFFF)
            {
                uint8_t file_id = REG_SP_WRITE_REQ & 0xFF;
                uint8_t file_buf[SP_STORAGE_FILE_SIZE] = {0}; // Временный буфер

                if (file_id < SP_STORAGE_FILE_COUNT)
                {
                    // Распаковка данных из регистров
                    for (int i = 0; i < SP_STORAGE_FILE_SIZE / 2; i++)
                    {
                        file_buf[i * 2] = regs[HLD_WRITE_REG + i] >> 8;
                        file_buf[i * 2 + 1] = regs[HLD_WRITE_REG + i] & 0xFF;
                    }

                    // Форматирование данных: [длина][данные]
                    // Не обнуляем хвост - сохраняем как есть (0xFF после стирания)
                    uint8_t *write_buf = malloc(SP_STORAGE_FILE_SIZE);
                    if (write_buf)
                    {
                        memset(write_buf, 0xFF, SP_STORAGE_FILE_SIZE); // Заполняем 0xFF
                        write_buf[0] = actual_bytes;                   // Байт длины

                        // Копируем только актуальные данные
                        if (actual_bytes > 0 && actual_bytes <= SP_STORAGE_FILE_SIZE - 1)
                        {
                            memcpy(write_buf + 1, file_buf, actual_bytes);
                        }

                        request_write_file(file_id, write_buf);
                        free(write_buf);
                    }
                }
                REG_SP_WRITE_REQ = 0xFFFF; // Сброс флага
            }

            // Обработка чтения RESPONSE
            if (REG_SP_READ_RESP != 0xFFFF)
            {
                uint8_t file_id = REG_SP_READ_RESP & 0xFF;
                uint8_t file_buf[SP_STORAGE_FILE_SIZE];

                if (file_id < SP_STORAGE_FILE_COUNT)
                {
                    if (response_read_file(file_id, file_buf) == ESP_OK)
                    {
                        // Если первый байт 0xFF - файл не существует
                        if (file_buf[0] == 0xFF)
                        {
                            memset(file_buf, 0, SP_STORAGE_FILE_SIZE);
                        }
                        else
                        {
                            // Коррекция длины данных
                            uint8_t length = file_buf[0];
                            if (length > SP_STORAGE_FILE_SIZE - 1)
                                length = SP_STORAGE_FILE_SIZE - 1;

                            // Обнуляем только заголовок для несуществующих файлов
                            if (length == 0)
                            {
                                memset(file_buf, 0, SP_STORAGE_FILE_SIZE);
                            }
                        }

                        // Упаковка в регистры
                        for (int i = 0; i < SP_STORAGE_FILE_SIZE / 2; i++)
                        {
                            regs[HLD_READ_RESP_REG + i] = (file_buf[i * 2] << 8) | file_buf[i * 2 + 1];
                        }
                    }
                }
                REG_SP_READ_RESP = 0xFFFF; // Сброс флага
            }

            // Обработка записи RESPONSE
            if (REG_SP_WRITE_RESP != 0xFFFF)
            {
                uint8_t file_id = REG_SP_WRITE_RESP & 0xFF;
                uint8_t file_buf[SP_STORAGE_FILE_SIZE] = {0}; // Временный буфер

                if (file_id < SP_STORAGE_FILE_COUNT)
                {
                    // Распаковка данных из регистров
                    for (int i = 0; i < SP_STORAGE_FILE_SIZE / 2; i++)
                    {
                        file_buf[i * 2] = regs[HLD_WRITE_RESP_REG + i] >> 8;
                        file_buf[i * 2 + 1] = regs[HLD_WRITE_RESP_REG + i] & 0xFF;
                    }

                    // Форматирование данных: [длина][данные]
                    // Не обнуляем хвост - сохраняем как есть (0xFF после стирания)
                    uint8_t *write_buf = malloc(SP_STORAGE_FILE_SIZE);
                    if (write_buf)
                    {
                        memset(write_buf, 0xFF, SP_STORAGE_FILE_SIZE); // Заполняем 0xFF
                        write_buf[0] = actual_bytes;                   // Байт длины

                        // Копируем только актуальные данные
                        if (actual_bytes > 0 && actual_bytes <= SP_STORAGE_FILE_SIZE - 1)
                        {
                            memcpy(write_buf + 1, file_buf, actual_bytes);
                        }

                        response_write_file(file_id, write_buf);
                        free(write_buf);
                    }
                }
                REG_SP_WRITE_RESP = 0xFFFF; // Сброс флага
            }

            xSemaphoreGive(reg_mutex);
        }
        vTaskDelay(50 / portTICK_PERIOD_MS);
    }
}

/* Основная функция инициализации хранилищ */
void start_storage_task()
{
    if (storage_init() != ESP_OK)
    {
        ESP_LOGE(TAG, "Критическая ошибка инициализации хранилищ");
        return;
    }

    reg_mutex = xSemaphoreCreateMutex();
    if (!reg_mutex)
    {
        ESP_LOGE(TAG, "Ошибка создания мьютекса");
        return;
    }

    // Инициализация регистров управления
    if (xSemaphoreTake(reg_mutex, portMAX_DELAY))
    {
        REG_SP_READ_REQ = 0xFFFF;
        REG_SP_WRITE_REQ = 0xFFFF;
        REG_SP_READ_RESP = 0xFFFF;
        REG_SP_WRITE_RESP = 0xFFFF;
        REG_CONFIG_UPDATE = 0; // Новый регистр для обновления конфигурации
        xSemaphoreGive(reg_mutex);
    }

    // Создание задачи
    xTaskCreate(storage_handler_task,
                "storage_manager",
                5120,
                NULL,
                configMAX_PRIORITIES - 2,
                NULL);
}

/**
 *      Ключевые особенности реализации:
 * 1. Унифицированная система работы с конфигурацией:
 *    - Используются два регистра управления:
 *      - `REG_CONFIG_OPERATION` (0x1A):
 *        - Бит 15: 1 - чтение, 0 - запись
 *        - Бит 14-8: тип конфигурации
 *        - Бит 7-0: зарезервировано
 *      - `REG_CONFIG_INDEX` (0x1B): индекс конфигурации (STA)
 *
 * 2. Типы конфигураций:
 * #define CONFIG_TYPE_STA0  0x01  // STA сеть 0
 * #define CONFIG_TYPE_STA1  0x02  // STA сеть 1
 * #define CONFIG_TYPE_STA2  0x03  // STA сеть 2
 * #define CONFIG_TYPE_AP    0x04  // AP сеть
 * #define CONFIG_TYPE_SN    0x05  // Серийный номер
 * #define CONFIG_TYPE_FW    0x06  // Версия прошивки
 *
 *          Протокол работы:
 *
 *      Чтение конфигурации:
 * 1. Установить `REG_CONFIG_OPERATION = (0x8000 | (CONFIG_TYPE_* << 8))`
 *    regs[0x1A]  Операция: [15] R/W, [14:8] тип, [7:0] индекс
 * 2. Установить `REG_CONFIG_INDEX` (для STA сетей)
 *                          regs[0x1B]  Индекс конфигурации (STA = 0 1 2)
 * 3. После обработки данные будут в регистрах `HLD_CONFIG_DATA_REG` (0x20+)
 * 4. Флаги автоматически сбросятся в 0xFFFF
 *
 *    Прочитать SSID0:
 *      1. Записать в регистр 0x1A 0x8100
 *      2. Записать в регистр 0x1B 0x0000
 *      3. Прочитать 20 регистров, начиная от адреса 0x20
 *    Прочитать SSID2:
 *      1. Записать в регистр 0x1A 0x8300
 *      2. Прочитать 20 регистров, начиная от адреса 0x20
 *    Прочитать AP_SSID:
 *      1. Записать в регистр 0x1A 0x8400
 *      2. Прочитать 20 регистров, начиная от адреса 0x20 -> `45 53 50 33 32 5F 41 50` "ESP32_AP"
 *    Прочитать SER_NUM:
 *      1. Записать в регистр 0x1A 0x8500
 *      2. Прочитать 10 регистров, начиная от адреса 0x20 -> `` ""
 *
 *  !? Последний байт не записывается или не читается ?! - уточнить
 *
 *      Запись конфигурации:
 * 1. Записать данные в регистры `HLD_CONFIG_DATA_REG`
 * 2. Установить `REG_CONFIG_OPERATION = (CONFIG_TYPE_* << 8)`
 * 3. Установить `REG_CONFIG_INDEX` (для STA сетей)
 * 4. После обработки флаги сбросятся в 0xFFFF
 *
 *     Запись серийного номера:
 *   1. Записать серийный номер (HEX) в регистры 0x20+ (до 12 регистров)
 *         `3031 3233 3435 3637 3839 2020 3938 3736 3534 3332 3130 4040`
 *   2. Записать в регистр 0x1A   0x0500
 *   3. Читать в терминале: "Updated SN: 0123456789  9876543210@@"
 *
 *      Безопасность паролей:
 *   - При чтении пароли возвращаются как `0x2A2A` (символы '**')
 *   - Реальные пароли доступны только при записи
 *   - Это предотвращает несанкционированное чтение паролей
 *
 * 5. Структура данных в регистрах:
 *    - Для STA сетей:
 *      - Регистры 0x20-0x2F: SSID (до 32 байтов)
 *      - Регистры 0x30-0x4F: пароль (до 64 байтов)
 *    - Для AP:
 *      - Регистры 0x20-0x2F: SSID (до 32 байтов)
 *      - Регистры 0x30-0x4F: пароль (до 64 байтов)
 *    - Для серийного номера:
 *      - Регистры 0x20-0x2B: серийный номер (до 24 байтов)
 *    - Для версии прошивки:
 *      - Регистры 0x20-0x27: версия прошивки (до 16 байт)
 *
 * 6. Автоматическое сохранение:
 *    - После записи конфигурация автоматически сохраняется в раздел `config`
 *    - Обновляется метка времени последнего изменения
 *    - Обеспечивается целостность данных
 *
 * 7. Особенности работы с данными:
 *    - Все строковые поля (SSID, пароли, серийный номер) хранятся как фиксированные буферы
 *    - При копировании данных используется strlcpy(), что гарантирует нуль-терминацию
 *    - Если данные занимают весь буфер, нуль-терминатор не добавляется (буфер заполнен
 *      полностью)
 *    - При чтении таких данных через Modbus:
 *        - Последний байт будет корректно прочитан
 *        - При выводе в терминал система может ожидать нуль-терминатор,
 *          поэтому рекомендуется использовать вывод с явным указанием длины:
 *          ESP_LOGI(TAG, "%.*s", (int)sizeof(data), data);
 *
 *      Преимущества решения:
 * 1. Удобство обслуживания - единый интерфейс для всех типов конфигураций
 * 2. Безопасность - пароли скрыты при чтении
 * 3. Гибкость - поддержка нескольких сетей STA
 * 4. Расширяемость - легко добавить новые типы конфигураций
 * 5. Интеграция - использование существующих регистров данных
 * 6. Логирование - детальное протоколирование операций
 *
 * Для использования в проекте:
 * 1. Добавьте раздел `config` в `partitions.csv`
 * 2. Обновите `sp_storage.h` с новыми определениями
 * 3. Используйте в коде WiFi-менеджера:
 *    ```c
 *    // При инициализации WiFi
 *    config_init(&current_config);
 *    strcpy(wifi_config.sta_ssid, current_config.sta_ssid[0]);
 *    strcpy(wifi_config.sta_password, current_config.sta_password[0]);
 *    ```
 * 4. Для обновления конфигурации через Modbus:
 *    - Запишите новые данные в регистры 0x20+
 *    - Установите `REG_CONFIG_OPERATION` и `REG_CONFIG_INDEX`
 *    - Дождитесь сброса флагов (0xFFFF)
 *
 */
