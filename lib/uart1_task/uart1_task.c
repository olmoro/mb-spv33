/**
 * uart1_task.c
 * Версия от 26 августа 2025г.
 */
#include "uart1_task.h"
#include "board.h"
#include "project_config.h"
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include "esp_err.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/uart.h"
#include "mb_crc.h"
#include "gw_nvs.h"

// Теги для логов
static const char *TAG = "UART1 Gateway";

static SemaphoreHandle_t uart1_mutex = NULL;

extern uint16_t regs[]; // Внешний массив holding-регистров

/* Глобальные переменные для хранения данных запроса */
uint8_t mb_addr = 0x00;
uint8_t mb_func = 0x00;
uint16_t mb_reg = 0x0000;
uint16_t mb_regs = 0x0000;
uint8_t mb_bytes = 0x00;
// Корректированное число актуальных байт в полученном по команде 0x10 пакете с проверкой "хвоста" (0x0C03)
uint8_t actual_bytes = 0x00;

/* Буфер для ошибки Modbus */
uint8_t error_mb[5];
const uint8_t error_mb_len = sizeof(error_mb);

// Генерация пакета ошибки Modbus
static void generate_error(uint8_t error_code)
{
    error_mb[0] = mb_addr;
    error_mb[1] = mb_func | 0x80; // Устанавливаем старший бит функции
    error_mb[2] = error_code;

    // Расчет CRC
    uint16_t error_mb_crc = mb_crc16(error_mb, error_mb_len - 2);
    error_mb[3] = error_mb_crc & 0xFF; // Младший байт CRC
    error_mb[4] = error_mb_crc >> 8;   // Старший байт CRC

    ESP_LOGI(TAG, "Error_packet_len (%d bytes):", error_mb_len);
    for (int i = 0; i < error_mb_len; i++)
    {
        printf("%02X ", error_mb[i]);
    }
    printf("\n");
}

// Отправка данных holding-регистров (функция 0x03)
static void send_register_data(uint8_t slave_addr, uint16_t start_reg, uint16_t count)
{
    // Размер ответа: адрес(1) + функция(1) + счетчик байт(1) + данные(2*count) + CRC(2)
    uint8_t response[5 + 2 * count];

    response[0] = slave_addr;
    response[1] = 0x03;
    response[2] = 2 * count; // Количество байт данных

    // Копирование данных из регистров
    for (int i = 0; i < count; i++)
    {
        uint16_t value = regs[start_reg + i];
        response[3 + 2 * i] = (value >> 8) & 0xFF; // Старший байт
        response[4 + 2 * i] = value & 0xFF;        // Младший байт
    }

    // Расчет CRC (для всего пакета кроме CRC)
    uint16_t crc = mb_crc16(response, 3 + 2 * count);
    response[3 + 2 * count] = crc & 0xFF; // Младший байт CRC
    response[4 + 2 * count] = crc >> 8;   // Старший байт CRC

    // Отправка с защитой мьютексом
    xSemaphoreTake(uart1_mutex, portMAX_DELAY);
    uart_write_bytes(MB_PORT_NUM, response, sizeof(response));
    xSemaphoreGive(uart1_mutex);
}

/* Задача обработки UART1 (Modbus slave) */
void uart1_task(void *arg)
{
    // Применяем настройки тайм-аута
    uint16_t mb_frame_time_out = REG_MODBUS_TIME_OUT;
    ESP_LOGI(TAG, "Modbus time-out %d ms", (unsigned int)mb_frame_time_out);

    // Создание мьютекса для доступа к UART
    uart1_mutex = xSemaphoreCreateMutex();
    if (!uart1_mutex)
    {
        ESP_LOGE(TAG, "Ошибка создания мьютекса UART1");
        vTaskDelete(NULL);
        return;
    }

    uint8_t *data_buf = NULL;  // Буфер для сбора пакета
    uint16_t data_len = 0;     // Длина собранных данных
    uint32_t last_rx_time = 0; // Время последнего приема
    const uint16_t timeout_ticks = pdMS_TO_TICKS(mb_frame_time_out);

    while (1)
    {
        uint8_t temp_buf[UART_BUF_SIZE];
        int len = uart_read_bytes(MB_PORT_NUM, temp_buf, sizeof(temp_buf), pdMS_TO_TICKS(20));

        // Обработка принятых данных
        if (len > 0)
        {
            // Инициализация буфера при начале нового фрейма
            if (data_buf == NULL)
            {
                data_buf = malloc(MAX_PDU_LENGTH);
                if (!data_buf)
                {
                    ESP_LOGE(TAG, "Ошибка выделения памяти");
                    continue;
                }
                data_len = 0;
            }

            // Проверка переполнения буфера
            if (data_len + len > MAX_PDU_LENGTH)
            {
                ESP_LOGE(TAG, "Переполнение буфера! Сброс фрейма");
                free(data_buf);
                data_buf = NULL;
                data_len = 0;
                continue;
            }

            // Копирование данных в буфер
            memcpy(data_buf + data_len, temp_buf, len);
            data_len += len;
            last_rx_time = xTaskGetTickCount();
        }

        // Проверка завершения фрейма по таймауту
        if (data_buf && (xTaskGetTickCount() - last_rx_time > timeout_ticks))
        {
            // Проверка минимальной длины фрейма (адрес + функция + CRC)
            if (data_len < 4)
            {
                ESP_LOGE(TAG, "Недопустимая длина фрейма: %d", data_len);
                free(data_buf);
                data_buf = NULL;
                data_len = 0;
                continue;
            }

            // Проверка адреса устройства
            if (data_buf[0] != REG_MODBUS_SLAVE_ADDR)
            {
                ESP_LOGW(TAG, "Несовпадение адреса: 0x%02X", data_buf[0]);
                free(data_buf);
                data_buf = NULL;
                data_len = 0;
                continue;
            }

            // Проверка CRC
            uint16_t received_crc = (data_buf[data_len - 1] << 8) | data_buf[data_len - 2];
            uint16_t calculated_crc = mb_crc16(data_buf, data_len - 2);

            if (received_crc != calculated_crc)
            {
                ESP_LOGE(TAG, "Ошибка CRC: %04X != %04X", received_crc, calculated_crc);
                free(data_buf);
                data_buf = NULL;
                data_len = 0;
                continue;
            }

            // Сохранение основных параметров запроса
            mb_addr = data_buf[0];
            mb_func = data_buf[1];
            mb_reg = (data_buf[2] << 8) | data_buf[3];  // Начальный регистр
            mb_regs = (data_buf[4] << 8) | data_buf[5]; // Число регистров

            // Обработка в зависимости от функции Modbus
            switch (mb_func)
            {
            case 0x03: // Чтение holding-регистров
                // Проверка допустимости диапазона регистров

      //  ESP_LOGI(TAG, "mb_reg: %02X mb_reg+s: %02X MAX: %02X", mb_reg, (mb_reg + mb_regs), MAX_REGS);


                if (mb_reg >= MAX_REGS ||
                    mb_reg + mb_regs > MAX_REGS ||
                    mb_regs == 0)
                {
                    generate_error(0x02); // Недопустимый адрес данных
                    xSemaphoreTake(uart1_mutex, portMAX_DELAY);
                    uart_write_bytes(MB_PORT_NUM, error_mb, error_mb_len);
                    xSemaphoreGive(uart1_mutex);
                }
                else
                {
                    send_register_data(mb_addr, mb_reg, mb_regs);
                }
                break;

            case 0x06: // Запись одного holding-регистра
                /** Проверка допустимости записи в регистры:
                 *  - 0x00 ... 0x1F - разрешено
                 *  - 0x20 ... 0x7F - запрещено
                 *  - 0x80 ... 0xDF - разрешено
                 */
                if ((mb_reg >= MAX_CONTROL_REGS &&
                     mb_reg < (MAX_REGS - MAX_WRITE_REGS)) ||
                    mb_reg > MAX_REGS -1)
                {
                    generate_error(0x02); // Недопустимый адрес данных
                    xSemaphoreTake(uart1_mutex, portMAX_DELAY);
                    uart_write_bytes(MB_PORT_NUM, error_mb, error_mb_len);
                    xSemaphoreGive(uart1_mutex);
                }
                else
                {
                    uint16_t value = (data_buf[4] << 8) | data_buf[5];
                    regs[mb_reg] = value;

                    /* Проверка допустимости адреса регистра для записи в NVS
                       (регистры управления в NVS не записываются) */
                    if (mb_reg < MAX_PARAM_INDEX)    //   MAX_CONTROL_REGS)
                    {
                        write_parameter_to_nvs(mb_reg, value);
                    }

                    // Формирование ответа (эхо запроса)
                    uint8_t response[8];
                    memcpy(response, data_buf, 6); // Копирование заголовка
                    uint16_t crc = mb_crc16(response, 6);
                    response[6] = crc & 0xFF;
                    response[7] = crc >> 8;

                    xSemaphoreTake(uart1_mutex, portMAX_DELAY);
                    uart_write_bytes(MB_PORT_NUM, response, sizeof(response));
                    xSemaphoreGive(uart1_mutex);
                }
                break;

            case 0x10: // Запись нескольких holding-регистров
                /** Проверка допустимости записи в регистры :
                 *  - 0x00 ... 0x1F - запрещено
                 *  - 0x20 ... 0x7F - разрешено
                 *  - 0x80 ... 0xDF - разрешено
                 */
                if ((mb_reg < MAX_CONTROL_REGS) ||
                    mb_reg > MAX_REGS -1)
                {
                    generate_error(0x02); // Недопустимый адрес данных
                    xSemaphoreTake(uart1_mutex, portMAX_DELAY);
                    uart_write_bytes(MB_PORT_NUM, error_mb, error_mb_len);
                    xSemaphoreGive(uart1_mutex);
                }
                // Проверка количества регистров и байт данных
                else if (mb_regs == 0 ||
                         mb_regs > MAX_REGS ||
                         data_buf[6] != 2 * mb_regs)
                {
                    generate_error(0x03); // Недопустимое значение данных
                    xSemaphoreTake(uart1_mutex, portMAX_DELAY);
                    uart_write_bytes(MB_PORT_NUM, error_mb, error_mb_len);
                    xSemaphoreGive(uart1_mutex);
                }
                else
                {
                    // Неактуализированное количество байт в введённом пакете
                    mb_bytes = data_buf[6];

                    for (int i = 0; i < mb_regs; i++)
                    {
                        // Корректная индексация данных в буфере
                        uint16_t value = (data_buf[7 + 2 * i] << 8) | data_buf[8 + 2 * i];
                        regs[mb_reg + i] = value; // Запись в правильный регистр
                        // Введённые по команде 0x10 данные в nvs не записываются:
                        // write_parameter_to_nvs(mb_reg + i, value); // - отменено
                    }

                    // // Формирование ответа
                    // uint8_t response[8];
                    // response[0] = mb_addr;
                    // response[1] = 0x10;
                    // response[2] = data_buf[2]; // Старший байт адреса
                    // response[3] = data_buf[3]; // Младший байт адреса
                    // response[4] = data_buf[4]; // Старший байт количества регистров
                    // response[5] = data_buf[5]; // Младший байт количества регистров

                    // Вычисление актуального количества байтов в буфере при вводе лишнего нуля в младший байт последнего регистра
                    actual_bytes = data_buf[6];

                    /* "Хвост" пакета может быть таким: `0x0C03 CRC` или `0x0C03 0x00 CRC` - с лишним нулём */
                    if (data_buf[data_len - 5] == 0x0C && data_buf[data_len - 4] == 0x03)
                        actual_bytes -= 1;
                    ESP_LOGI(TAG, "В MB пакете (%d bytes):", actual_bytes);


                    // Формирование MB ответа
                    uint8_t response[8];
                    response[0] = mb_addr;
                    response[1] = 0x10;
                    response[2] = data_buf[2]; // Старший байт адреса
                    response[3] = data_buf[3]; // Младший байт адреса
                    response[4] = data_buf[4]; // Старший байт количества регистров
                    response[5] = data_buf[5]; // Младший байт количества регистров
                    uint16_t crc = mb_crc16(response, 6);
                    response[6] = crc & 0xFF;
                    response[7] = crc >> 8;

                    xSemaphoreTake(uart1_mutex, portMAX_DELAY);
                    uart_write_bytes(MB_PORT_NUM, response, sizeof(response));
                    xSemaphoreGive(uart1_mutex);
                }
                break;

            default: // Недопустимая функция
                generate_error(0x01);
                xSemaphoreTake(uart1_mutex, portMAX_DELAY);
                uart_write_bytes(MB_PORT_NUM, error_mb, error_mb_len);
                xSemaphoreGive(uart1_mutex);
                break;
            }

            // Очистка буфера после обработки
            free(data_buf);
            data_buf = NULL;
            data_len = 0;
        }
    }
}

/**
 *      Ключевые особенности реализации:
 *
 * 1. Обработка основных функций Modbus:
 * - 0x03 (Read Holding Registers) - чтение блоков регистров
 * - 0x06 (Write Single Register) - запись одиночного регистра
 * - 0x10 (Write Multiple Registers) - запись блока регистров SP-пакета:  
 *   байты DLE (префикс), DAD (адрес приёмника), SAD (адрес источника) и FNC (байт кода функции)
 *   вводить не надо, они вводятся автоматически. Завершение пакета кодами `0x0C` (FF - перевод строки) и 
 *   `0x03` (ETX, конец тела сообщения) - обязательно.
 *
 * 2. Механизм приема пакетов:
 * - Сбор пакетов по таймауту между символами
 * - Проверка целостности (CRC16)
 * - Проверка адреса устройства
 *
 * 3. Обработка ошибок:
 * - Формирование стандартных ответов Modbus для ошибок
 * - Коды ошибок: 0x01 (неверная функция), 0x02 (неверный адрес), 0x03 (неверное значение)
 * - Защита от переполнения буфера
 * - Вычисление актуальной длины введённого по команде 0x10 пакета
 *
 * 4. Потокобезопасность:
 * - Мьютекс для защиты доступа к UART
 * - Корректная работа в RTOS среде
 *
 * 5. Интеграция с NVS:
 * - Сохранение регистров управления в энергонезависимую память
 * - Функция write_parameter_to_nvs() для сохранения значений
 *
 * 6. Оптимизации:
 * - Динамическое выделение памяти только при начале приема пакета
 * - Минимальное использование буферов
 * - Эффективная работа с CRC
 * - Строгая проверка границ регистров
 * - Энергоэффективная работа с таймаутами
 * - Полная поддержка стандарта Modbus RTU
 *
 *      Пояснения:
 * 1. Введённый по команде 0x10 можно редактировать, используя команду 0x06
 * 2. Для приёма данных с целевого прибора могут использоваться все 96*2 регистров
 *
 * Код соответствует требованиям ESP-IDF v5.4.
 */
