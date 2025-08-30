/**
 *          Особенности реализации:
 * 1. Коррекция при записи:
 *    - Значение корректируется сразу при получении
 *    - Обновление регистра происходит ДО попытки записи в NVS
 *    - Гарантирует актуальность данных в регистрах
 *
 * 2. Коррекция при чтении:
 *    - Защита от некорректных значений в NVS
 *    - Автоматическое исправление при загрузке
 *    - Работает как для значений из NVS, так и для значений по умолчанию
 *
 * 3. Безопасность:
 *    - Проверка индексов во всех операциях
 *    - Защита от переполнения буфера ключей
 *    - Повторные попытки при сбоях NVS
 *
 *          Рекомендации по использованию:
 * 1. При инициализации устройства вызывайте `read_parameter_from_nvs()` для всех параметров
 * 2. При изменении параметров через Modbus используйте `write_parameter_to_nvs()`
 * 3. Для добавления новых параметров:
 *    - Увеличьте `PARAMS_COUNT`
 *    - Добавьте новый элемент в `param_meta`
 *    - Обновите обработчики Modbus при необходимости
 *
 * Данная реализация гарантирует, что значения параметров всегда будут находиться в допустимых пределах,
 * независимо от источника данных (NVS или прямое изменение через регистры).
 *
 * Версия от 29 июня 2025г. Проверено
 */

#include "gw_nvs.h"
#include "project_config.h"
#include "board.h"
#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "nvs_flash.h"
#include "driver/uart.h"

/* Теги для логов */
static const char *TAG = "NVS";

/* Таблица скоростей (индекс 0-9 соответствует стандартным скоростям) */
static const uint32_t baud_table[10] = {300, 600, 1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200};

/* Массив регистров Modbus */
uint16_t regs[MAX_REGS];

uint8_t dad; // адрес приёмника запроса (задаётся в настройках шлюза и целевого прибора)
uint8_t sad; // адрес источника запроса (задаётся в настройках шлюза)

/* Инициализация NVS */
void nvs_init()
{
    // Инициализация регистров
    memset(regs, 0, sizeof(regs));
    // REG_WIFI_MODE = 1; // Старт в режиме STA

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
}

/* Корректировка значения параметра по границам */
static uint16_t clamp_parameter_value(int index, uint16_t value)
{
    if (index < 0 || index >= PARAMS_COUNT)
    {
        return value; // Некорректный индекс - возвращаем как есть
    }

    const ParamMeta *meta = &param_meta[index];
    if (value < meta->min)
    {
        ESP_LOGW(TAG, "Param %d clamped: %d -> min=%d", index, value, meta->min);
        return meta->min;
    }
    else if (value > meta->max)
    {
        ESP_LOGW(TAG, "Param %d clamped: %d -> max=%d", index, value, meta->max);
        return meta->max;
    }
    return value;
}

/* Чтение параметра из NVS с автоматической коррекцией */
esp_err_t read_parameter_from_nvs(int i)
{
    // Проверка валидности индекса
    if (i < 0 || i >= MAX_PARAM_INDEX)
    {
        ESP_LOGE(TAG, "Invalid parameter index: 0x%02X", i);
        return ESP_ERR_INVALID_ARG;
    }

    // Генерация ключа
    char key[NVS_KEY_BUFFER_SIZE];
    int key_len = snprintf(key, sizeof(key), "param_%d", i);
    if (key_len < 0 || key_len >= sizeof(key))
    {
        ESP_LOGE(TAG, "Key generation failed for index: %d", i);
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t handle;
    esp_err_t err;
    int retry_count = 0;
    bool value_loaded = false;
    uint16_t stored_value = 0;

    // Повторные попытки чтения
    while (retry_count++ < MAX_RETRY_ATTEMPTS)
    {
        err = nvs_open("storage", NVS_READONLY, &handle);
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "NVS open failed: %s (0x%x)", esp_err_to_name(err), err);
            continue;
        }

        err = nvs_get_u16(handle, key, &stored_value);
        nvs_close(handle);

        if (err == ESP_OK)
        {
            value_loaded = true;
            break;
        }

        ESP_LOGW(TAG, "Read failed for %s: %s (retry %d/%d)",
                 key, esp_err_to_name(err), retry_count, MAX_RETRY_ATTEMPTS);
    }

    // Обработка результата чтения
    if (value_loaded)
    {
        // Корректировка прочитанного значения
        stored_value = clamp_parameter_value(i, stored_value);
        regs[i] = stored_value;
    }
    else
    {
        // Использование значения по умолчанию с коррекцией
        ESP_LOGW(TAG, "Using default value for param %d", i);
        regs[i] = clamp_parameter_value(i, param_meta[i].def);
        err = (err == ESP_ERR_NVS_NOT_FOUND) ? ESP_ERR_NOT_FOUND : err;
    }

    return err;
}

/* Запись параметра в NVS с проверкой границ */
esp_err_t write_parameter_to_nvs(int i, uint16_t value)
{

    // Проверка валидности индекса
    if (i < 0 || i >= MAX_PARAM_INDEX)
    {
        ESP_LOGE(TAG, "Invalid parameter index: 0x%02X", i);
        return ESP_ERR_INVALID_ARG;
    }

    // Корректировка значения перед записью
    uint16_t corrected_value = clamp_parameter_value(i, value);
    if (corrected_value != value)
    {
        ESP_LOGW(TAG, "Param %d corrected during write: %d -> %d",
                 i, value, corrected_value);
    }

    // Обновление регистра
    regs[i] = corrected_value;

    // Генерация ключа
    char key[NVS_KEY_BUFFER_SIZE];
    int key_len = snprintf(key, sizeof(key), "param_%d", i);
    if (key_len < 0 || key_len >= sizeof(key))
    {
        ESP_LOGE(TAG, "Key generation failed for index: %d", i);
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t handle;
    esp_err_t err;
    int retry_count = 0;

    // Повторные попытки записи
    while (retry_count++ < MAX_RETRY_ATTEMPTS)
    {
        err = nvs_open("storage", NVS_READWRITE, &handle);
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "NVS open failed: %s (0x%x)", esp_err_to_name(err), err);
            continue;
        }

        err = nvs_set_u16(handle, key, corrected_value);
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "NVS set failed for %s: %s", key, esp_err_to_name(err));
            nvs_close(handle);
            continue;
        }

        err = nvs_commit(handle);
        nvs_close(handle);

        if (err == ESP_OK)
        {

            return ESP_OK; // Успешная запись
        }

        ESP_LOGE(TAG, "NVS commit failed: %s (retry %d/%d)",
                 esp_err_to_name(err), retry_count, MAX_RETRY_ATTEMPTS);
    }

    return err;
}

/* Чтение параметров из NVS */
void update_parameters_from_nvs()
{
    uint16_t stored_version = 0;
    nvs_handle_t handle;

    /* Открываем NVS пространство */
    if (nvs_open("storage", NVS_READONLY, &handle) != ESP_OK)
    {
        ESP_LOGE(TAG, "Error opening NVS");
        goto USE_DEFAULTS;
    }

    /* Читаем версию */
    if (nvs_get_u16(handle, "version", &stored_version) != ESP_OK)
    {
        ESP_LOGW(TAG, "Version not found");
        nvs_close(handle);
        goto USE_DEFAULTS;
    }

    /* Проверяем версию */
    if (stored_version != CURRENT_VERSION)
    {
        ESP_LOGI(TAG, "New version detected");
        nvs_close(handle);

        write_defaults_to_nvs();
        return;
    }

    // Новая версия не обнаружена - готовность к работе
    ledsBlue();

    /* Читаем параметры */
    for (int i = 0; i < PARAMS_COUNT; i++)
    {
        char key[10];
        snprintf(key, sizeof(key), "param_%d", i);

        if (nvs_get_u16(handle, key, &regs[i]) != ESP_OK)
        {
            ESP_LOGW(TAG, "Param %d not found", i);
            regs[i] = param_meta[i].def;
        }
    }
    nvs_close(handle);
    return;

USE_DEFAULTS:
    /* Записываем дефолтные значения */
    write_defaults_to_nvs();
}

/* Запись дефолтных значений в NVS */
void write_defaults_to_nvs()
{

    nvs_handle_t handle;
    ESP_ERROR_CHECK(nvs_open("storage", NVS_READWRITE, &handle));

    /* Записываем все параметры */
    for (int i = 0; i < PARAMS_COUNT; i++)
    {
        char key[10];
        snprintf(key, sizeof(key), "param_%d", i);
        ESP_ERROR_CHECK(nvs_set_u16(handle, key, param_meta[i].def));

        /* Обновляем регистры */
        regs[i] = param_meta[i].def;

        /**
         *  Бесконечное мигание синим после загрузки дефолтных значений
         *  в NVS и регистры. Это тоже готовность к работе, далее при
         *  записи или чтении индицация готовности сменится на немигающий
         *  синий
         */
        led_blink(RGB_BLUE_GPIO, 200, 400, -1);
    }

    /* Устанавливаем текущую версию */
    ESP_ERROR_CHECK(nvs_set_u16(handle, "version", CURRENT_VERSION));

    /* Коммитим изменения */
    ESP_ERROR_CHECK(nvs_commit(handle));
    nvs_close(handle);

    ESP_LOGI(TAG, "Default values written");
}

// /* Примерный вид функции save_parameters_to_nvs - заглушка*/
// void save_parameters_to_nvs()
// {
//     // Открываем пространство имен в NVS
//     nvs_handle_t nvs_handle;
//     esp_err_t err = nvs_open("storage", NVS_READWRITE, &nvs_handle);

//     if (err != ESP_OK)
//     {
//         // Обработка ошибки
//         ESP_LOGE(TAG, "Error opening NVS");
//         return;
//     }

//     // Записываем параметры
// /**  nvs_set_blob устанавливает двоичное значение переменной длины для ключа.
//  * Функция использует 2 служебных байта и 1 запись для каждых 32 байт новых данных из пула
//  * доступных записей. См. \c nvs_get_stats .
//  * В случае обновления значения для существующего ключа пространство, занимаемое существующим
//  * значением, и 2 служебных байта возвращаются в пул доступных записей.
//  * Обратите внимание, что базовое хранилище не будет обновлено до вызова \c nvs_commit.
//  */
//     err = nvs_set_blob(nvs_handle, "parameters", &param_meta, sizeof(param_meta));

//     if (err != ESP_OK)
//     {
//     // Обработка ошибки
//         ESP_LOGE(TAG, "Error writing NVS");
//         return;
//     }

//     // Фиксируем запись
//     nvs_commit(nvs_handle);
//     nvs_close(nvs_handle);
// }

// /* Пользовательский обработчик перезагрузки */
// void custom_shutdown_handler(void)
// {
//     /*
//      * Критически важно сохранять параметры перед перезагрузкой в промышленных системах:
//      * - Гарантирует сохранение последних корректных настроек
//      * - Предотвращает потерю критичных данных при сбоях
//      * - Обеспечивает предсказуемое поведение системы после перезапуска
//      */
//     save_parameters_to_nvs();

//     /*
//      * Примечание: здесь можно добавить:
//      * - Запись причины перезагрузки (в RTC память или NVS)
//      * - Безопасное отключение периферии
//      * - Сигнализация о перезагрузке внешним системам
//      */
// }

/* Инициализация интерфейса UART1 */
void mb_uart1_init()
{
    uint8_t mb_slave_addr = param_meta[1].def;
    uint32_t mb_baud_rate = baud_table[param_meta[2].def];

    /* Применяем настройки адреса modbus-slave */
    // mb_slave_addr = regs[1];
    mb_slave_addr = REG_MODBUS_SLAVE_ADDR;
    ESP_LOGI(TAG, "Modbus configured for %d slave addr", (unsigned int)mb_slave_addr);

    /* Применяем настройки скорости */
    mb_baud_rate = baud_table[REG_MODBUS_BAUD_INDEX];
    ESP_LOGI(TAG, "Modbus configured for %d baud", (unsigned int)mb_baud_rate);

    /* Configure parameters of an UART driver, communication pins and install the driver */
    uart_config_t uart_mb_config =
        {
            .baud_rate = mb_baud_rate, // MB_BAUD_RATE,
            .data_bits = UART_DATA_8_BITS,
            .parity = UART_PARITY_DISABLE,
            .stop_bits = UART_STOP_BITS_1,
            .flow_ctrl = UART_HW_FLOWCTRL_DISABLE, // RTS для управления направлением DE/RE !!
            .rx_flow_ctrl_thresh = 122,
        };

    ESP_ERROR_CHECK(uart_driver_install(MB_PORT_NUM, UART_BUF_SIZE, UART_BUF_SIZE, MB_QUEUE_SIZE, NULL, 0));

    /* IO33 свободен (трюк) */
    ESP_ERROR_CHECK(uart_set_pin(MB_PORT_NUM, CONFIG_MB_UART_TXD, CONFIG_MB_UART_RXD, CONFIG_MB_UART_RTS, CONFIG_MB_UART_DTS));

    /* activate RS485 half duplex in the driver */
    ESP_ERROR_CHECK(uart_set_mode(MB_PORT_NUM, UART_MODE_RS485_HALF_DUPLEX));

    ESP_ERROR_CHECK(uart_param_config(MB_PORT_NUM, &uart_mb_config));
    ESP_LOGI(TAG, "slave_uart initialized.");
}

/* Инициализация интерфейса UART2 */
void sp_uart2_init()
{
    uint32_t sp_baud_rate = baud_table[param_meta[6].def];

    /* Применяем настройки магистрального адреса приёмника */
    dad = REG_SP_DAD_ADDR;
    ESP_LOGI(TAG, "Sp configured for 0x%02X dad", (unsigned int)dad);

    /* Применяем настройки магистрального адреса источника */
    sad = REG_SP_SAD_ADDR;
    ESP_LOGI(TAG, "Sp configured for 0x%02X sad", (unsigned int)sad);

    /* Применяем настройки скорости */
    sp_baud_rate = baud_table[REG_SP_BAUD_INDEX];
    ESP_LOGI(TAG, "Sp configured for %d baud", (unsigned int)sp_baud_rate);

    /* Configure parameters of an UART driver, communication pins and install the driver */
    uart_config_t uart_sp_config =
        {
            .baud_rate = sp_baud_rate,
            .data_bits = UART_DATA_8_BITS,
            .parity = UART_PARITY_DISABLE,
            .stop_bits = UART_STOP_BITS_1,
            .flow_ctrl = UART_HW_FLOWCTRL_DISABLE, // RTS для управления направлением DE/RE !!
            .rx_flow_ctrl_thresh = 122,
        };

    ESP_ERROR_CHECK(uart_driver_install(SP_PORT_NUM, UART_BUF_SIZE, UART_BUF_SIZE, SP_QUEUE_SIZE, NULL, 0));

    /* IO17 свободен (трюк) */
    ESP_ERROR_CHECK(uart_set_pin(SP_PORT_NUM, CONFIG_SP_UART_TXD, CONFIG_SP_UART_RXD, CONFIG_SP_UART_RTS, CONFIG_SP_UART_DTS));

    /* activate RS485 half duplex in the driver */
    ESP_ERROR_CHECK(uart_set_mode(SP_PORT_NUM, UART_MODE_RS485_HALF_DUPLEX));

    ESP_ERROR_CHECK(uart_param_config(SP_PORT_NUM, &uart_sp_config));
    ESP_LOGI(TAG, "sp_uart initialized.");
}
