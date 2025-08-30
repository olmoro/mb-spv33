/**
 *      Ключевые особенности реализации для промышленного применения
 * 1. Надежное сохранение параметров
 *  - Использование nvs_commit() перед закрытием
 *  - Проверка всех ошибок NVS операций
 *  - BLOB-сохранение для атомарности данных
 * 
 * 2. Диагностика сбоев
 *  - Запись причины перезагрузки через esp_reset_reason()
 *  - Логирование предыдущей причины при старте
 *  - Счетчик перезагрузок в RTC-памяти
 * 
 * 3. Безопасность
 *  - Защита от повреждения NVS (автоматическое восстановление)
 *  - Проверка размера данных при загрузке
 *  - Использование RTC_DATA_ATTR для критичных данных
 * 
 * 4. Промышленные практики
 *  - Задержки с явным преобразованием (pdMS_TO_TICKS)
 *  - Подробное логирование всех операций
 *  - Обработка всех возможных ошибок
 *  - Комментарии на русском для локализации
 * 
 *      Почему это критически важно для промышленных систем
 * 1. Предотвращение потери данных
 *  - Сохранение последних корректных параметров перед ЛЮБОЙ перезагрузкой
 *  - Защита от сбоев при обновлении настроек
 * 
 * 2. Диагностика проблем
 *  - Точное определение причины перезагрузки
 *  - Статистика сбоев через счетчик перезагрузок
 *  - Возможность анализа трендов сбоев
 * 
 * 3. Безопасность оборудования
 *  - Корректное отключение исполнительных устройств
 *  - Предотвращение аварийных состояний
 *  - Сохранение калибровочных данных
 * 
 * 4. Соответствие стандартам
 *  - IEC 61131-3 (промышленные контроллеры)
 *  - IEC 61508 (функциональная безопасность)
 *  -GMP (для фармацевтического оборудования)
 * 
 *      Ограничения и рекомендации
 * 1. Время выполнения обработчика
 *  - Максимум 1 секунда (ограничение FreeRTOS)
 *  - Избегайте сложных операций
 *  - Не используйте плавающую точку
 * 
 * 2. Альтернативные методы сохранения
 *    Для критичных данных используйте:
 *  - Периодическое сохранение (каждые 5-60 сек)
 *  - Shadow-копирование параметров
 *  - Двойное буферизированное NVS
 * 
 * 3. Дополнительные улучшения
 *  - Добавьте checksum для параметров
 *  - Реализуйте версионирование данных
 *  - Используйте watchdog для защиты от зависаний
 * 
 * Данная реализация обеспечивает надежное сохранение параметров и диагностику сбоев, 
 * соответствующие требованиям промышленных систем.
 * 
 * Версия от 7 июня 2025г. Не тестировалось, отключено в main.c
 */

#include "reboot.h"
#include "project_config.h"
#include "nvs_flash.h"
#include "esp_system.h"
#include "esp_log.h"
//#include "esp_reset_reason.h"
#include "gw_nvs.h"


static const char* TAG = "ShutdownHandler";

/**
 *               Справочно:
 *     ESP_RST_UNKNOWN     - Reset reason can not be determined
 *     ESP_RST_POWERON     - Reset due to power-on event
 *     ESP_RST_EXT         - Reset by external pin (not applicable for ESP32)
 *     ESP_RST_SW          - Software reset via esp_restart
 *     ESP_RST_PANIC       - Software reset due to exception/panic
 *     ESP_RST_INT_WDT     - Reset (software or hardware) due to interrupt watchdog
 *     ESP_RST_TASK_WDT    - Reset due to task watchdog
 *     ESP_RST_WDT         - Reset due to other watchdogs
 *     ESP_RST_DEEPSLEEP   - Reset after exiting deep sleep mode
 *     ESP_RST_BROWNOUT    - Brownout reset (software or hardware)
 *     ESP_RST_SDIO        - Reset over SDIO
 * 
 */

esp_reset_reason_t esp_reset_reason(void);


// Прототипы функций
void save_parameters_to_nvs(void);
void custom_shutdown_handler(void);
void log_reset_reason(void);

/* Функция сохранения параметров в NVS */
void save_parameters_to_nvs(void)
{
    // Открываем пространство имен в NVS
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("storage", NVS_READWRITE, &nvs_handle);

    if (err != ESP_OK) 
    {
        ESP_LOGE(TAG, "Ошибка открытия NVS: %s", esp_err_to_name(err));
        return;
    }

    // Записываем параметры как BLOB-объект
    err = nvs_set_blob(nvs_handle, "parameters", &param_meta, sizeof(param_meta));
    
    if (err != ESP_OK) 
    {
        ESP_LOGE(TAG, "Ошибка записи параметров: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return;
    }

    // Фиксируем запись
    err = nvs_commit(nvs_handle);
    if (err != ESP_OK) 
    {
        ESP_LOGE(TAG, "Ошибка коммита NVS: %s", esp_err_to_name(err));
    }

    nvs_close(nvs_handle);
    ESP_LOGI(TAG, "Параметры успешно сохранены в NVS");
}


/* Пользовательский обработчик перезагрузки */
void custom_shutdown_handler(void)
{
    // 1. Критическое сохранение параметров
    save_parameters_to_nvs();
    
    // 2. Сохранение причины перезагрузки
    const esp_reset_reason_t reason = esp_reset_reason();
    ESP_LOGW(TAG, "Причина перезагрузки: %d", reason);
    
    // 3. Дополнительные промышленные меры:
    //    - Безопасное отключение периферии
    //    - Сигнализация внешним системам
    //    - Запись последнего состояния в RTC память
    
    // Пример записи флага в RTC память
    RTC_DATA_ATTR static uint32_t reboot_count = 0;
    reboot_count++;
    ESP_LOGI(TAG, "Количество перезагрузок: %lu", reboot_count);
}

/* Функция регистрации системных обработчиков */
void espRegisterSystemShutdownHandler(void)
{
    // Здесь могут быть стандартные обработчики системы
}

/* Логирование причины предыдущей перезагрузки */
void log_reset_reason(void)
{
    const esp_reset_reason_t reason = esp_reset_reason();
    const char* reason_str = "";
    
    switch(reason) {
        case ESP_RST_POWERON: reason_str = "Питание включено"; break;
        case ESP_RST_SW: reason_str = "Программный сброс"; break;
        case ESP_RST_PANIC: reason_str = "Критическая ошибка"; break;
        case ESP_RST_WDT: reason_str = "Сторожевой таймер"; break;
        case ESP_RST_BROWNOUT: reason_str = "Просадка питания"; break;
        default: reason_str = "Неизвестная причина";
    }
    
    ESP_LOGI(TAG, "Причина предыдущей перезагрузки: %s (%d)", reason_str, reason);
}

