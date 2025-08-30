#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "driver/uart.h"
#include "project_config.h"
#include "board.h"
#include "uart1_task.h"
#include "uart2_task.h"
#include "gw_nvs.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_system.h"
#include "reboot.h"
#include "sp_storage.h"
#include "wifi_manager.h"

static const char *TAG = "UART Gateway";


extern uint16_t *regs;

void app_main(void)
{
    /* Инициализация периферии */
    boardInit();
    vTaskDelay(pdMS_TO_TICKS(1));

    /* Инициализация NVS */
    nvs_init();
    vTaskDelay(pdMS_TO_TICKS(1));  // время на стабилизацию системы (лучше использовать явное преобразование)
    
    // /* Регистрация нашего обработчика перезагрузки */
    // esp_register_shutdown_handler(&custom_shutdown_handler);
    // vTaskDelay(pdMS_TO_TICKS(1));

    /* Загрузка параметров из NVS */
    update_parameters_from_nvs();
    vTaskDelay(pdMS_TO_TICKS(1));

    /* Инициализация асинхронных интерфейсов */
    mb_uart1_init();
    vTaskDelay(pdMS_TO_TICKS(1));

    sp_uart2_init();
    vTaskDelay(pdMS_TO_TICKS(1));

    /* Создание задач modbus и sp */
    xTaskCreate(uart1_task, "UART1 Task", 4096, NULL, 5, NULL);
    vTaskDelay(pdMS_TO_TICKS(1));

    xTaskCreate(uart2_task, "UART2 Task", 4096, NULL, 5, NULL);
    vTaskDelay(pdMS_TO_TICKS(1));

    /* Запуск менеджера sp_storage */
    start_storage_task();
    vTaskDelay(pdMS_TO_TICKS(1));

    /* Запуск менеджера WiFi */
//    start_wifi_manager_task();
    vTaskDelay(pdMS_TO_TICKS(1));

    ESP_LOGI(TAG, "System initialized");
}
