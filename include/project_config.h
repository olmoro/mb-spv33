/* ---------------------------------------------------------------------------------
 *                            Файл конфигурации проекта
 * ---------------------------------------------------------------------------------
*/
#pragma once

#include <stdint.h>
#include "esp_task.h"
#include <time.h>
#include <stdio.h>
#include <stdbool.h>
#include "esp_bit_defs.h"
// ---------------------------------------------------------------------------------
//                       New mb-spv33
// ---------------------------------------------------------------------------------

#define CURRENT_VERSION 110 // Текущая версия конфигурации
// 20250814.1xx  New: Новая плата   без WiFi                RAM:  4.6%  Flash: 16.8%

// 20250724.100  New: Новая плата   без WiFi                RAM:  4.6%  Flash: 16.8%
// 20250724.069  New: исправ: wifi_manager, http_server     RAM: 12.8%  Flash: 55.4%
// 20250722.068  New: (история: 100 значений, исправленная  RAM: 12.8%  Flash: 53.9%
// 20250721.067  New: (история: 100 значений)               RAM: 12.8%  Flash: 53.9%
// 20250720.066  New: sp_processing.c                       RAM: 12.0%  Flash: 54.0%
// 20250717.065  New: uart2_task.c +repeat() заглушка       RAM: 12.0%  Flash: 53.8%
// 20250717.065  New: RESPONS 0x20+(96), REQUEST 0x80+(96)  RAM: 12.0%  Flash: 53.4%
// 20250713.064  New: parser.h, .c                          RAM: 11.9%  Flash: 53.3%
// 20250710.063  New: SSID, PASSWORD    R/W                 RAM: 11.9%  Flash: 53.4%
// 20250710.062  New: wifi_manager, sp_storage              RAM: 11.9%  Flash: 53.4%
// 20250710.061  MAX_CONTROL_REGS 32 + WiFi   Old mng.h     RAM: 11.8%  Flash: 53.5%
// 20250702.059  parser                                     RAM:  3.7%  Flash: 16.1%
// 20250629.059  storage_handler_task() [хвост FFh]         RAM:  3.7%  Flash: 16.0%
// 20250629.058  Sp time-out 40 ms                          RAM:  3.7%  Flash: 16.0%
// 20250629.057  storage_init(void)                         RAM:  3.7%  Flash: 16.0%
// 20250629.056  42*96 [Данные (до 96-1 байт)]              RAM:  3.7%  Flash: 16.0%


// ---------------------------------------------------------------------------------
//                                  GPIO MB_SPV02
// --------------------------------------------------------------------------------- 
// Светодиоды
#define RGB_RED_GPIO 27       // Красный, катод на GND (7mA)
#define RGB_GREEN_GPIO 4      // Зелёный, катод на GND (5mA)
#define RGB_BLUE_GPIO 2       // Синий,   катод на GND (4mA)
// UART1
#define CONFIG_MB_UART_RXD 26 // MRO
#define CONFIG_MB_UART_TXD 32 // MDI nc
#define CONFIG_MB_UART_RTS 25 // MRE#
#define CONFIG_MB_UART_DTS 33 // MDE
// UART2
#define CONFIG_SP_UART_RXD 19 // SRO
#define CONFIG_SP_UART_TXD 16 // SDI nc
#define CONFIG_SP_UART_RTS 18 // SRE#
#define CONFIG_SP_UART_DTS 17 // SDE

#define A_FLAG_GPIO 22 // FA
#define B_FLAG_GPIO 23 // FB

// ---------------------------------------------------------------------------------
//                                    Общие
// ---------------------------------------------------------------------------------

#define UART_BUF_SIZE (240) // размер буфера
#define BUF_MIN_SIZE (4) // минимальный размер буфера
#define MAX_PDU_LENGTH 240

// ---------------------------------------------------------------------------------
//                                    WiFi
// ---------------------------------------------------------------------------------

// Конфигурация сетей STA
#define STA_SSID0      "SSID0"
#define STA_PASSWORD0  "PASSWORD0"
#define STA_SSID1      "SSID1"
#define STA_PASSWORD1  "PASSWORD1"
#define STA_SSID2      "SSID2"
#define STA_PASSWORD2  "PASSWORD2"
#define MAX_SSID 3

// Конфигурация AP
#define AP_SSID      "ESP32_AP"
#define AP_PASSWORD  "12345678"
#define AP_CHANNEL   1
#define MAX_STA_CONN 4

// Веб-сервер
#define OTA_URL "https://example.com/firmware.bin"

// ---------------------------------------------------------------------------------
//                                      NVS
// ---------------------------------------------------------------------------------

/** Размеры и положение holding-регистров от начала: - отменено
 * - для функции 0x03 доступны все регистры;
 * - для функции 0x06 доступны все регистры;
 * - для функции 0x10 для записи пакета запроса доступны регистры //50...7F.
 */

// Конфигурация хранилища определяет количество и размер файлов в разделе (до 4096)
#define SP_STORAGE_FILE_COUNT 42                          // Максимальное количество файлов
#define SP_STORAGE_FILE_SIZE 96                           // Размер файла, включая байт длины (48 регистров)
//#define SP_STORAGE_OPERATION_OFFSET SP_STORAGE_FILE_COUNT // 96 Смещение для операции записи

// номера 32-х регистров управления         0x00 ... 0x1F
// номера 96-и регистров для чтения пакета  0x20 ... 0x7F 
// номера 96-и регистров для записи пакета  0x80 ... 0xDF 

#define HLD_REGS_OFFSET        0                              // Смещение для holding-регистров
#define MAX_CONTROL_REGS      32                              // Число регистров управления (32)


#define HLD_OUTPUT            MAX_CONTROL_REGS                // Первый регистр ответа (0X20)
#define HLD_READ_REG          MAX_CONTROL_REGS                // Первый регистр для чтения (0X20)
#define HLD_READ_RESP_REG     MAX_CONTROL_REGS                // Первый регистр для чтения ответа  (0X20)

#define MAX_READ_REGS         SP_STORAGE_FILE_SIZE            // Число регистров для ответа (96)
#define MAX_OUT_BUF_REGS      SP_STORAGE_FILE_SIZE            // Максимальное количество регистров (96 слов)
                                                              // для ответа


#define HLD_WRITE_REG         HLD_READ_REG + MAX_READ_REGS    // Первый регистр запроса (0x80)
#define HLD_WRITE_RESP_REG    HLD_READ_REG + MAX_READ_REGS    // Первый регистр для записи ответа ?? уочнить

#define MAX_WRITE_REGS        SP_STORAGE_FILE_SIZE            // Число регистров для запроса (96)

#define MAX_REGS              MAX_CONTROL_REGS + MAX_READ_REGS + MAX_WRITE_REGS  // Всего регистров (32+96+96=224)
                                                              // из них по команде 0x03 читаются (96+96=192)

#define MAX_DATA_SIZE         192            // Максимальный размер пакета (192 байта)

// Константы
#define PARAMS_COUNT          10                // Всего регистров, фиксируемых в NVS
#define MAX_PARAM_INDEX       PARAMS_COUNT
#define NVS_KEY_BUFFER_SIZE   15
#define MAX_RETRY_ATTEMPTS    3

// ---------------------------------------------------------------------------------
//                                    WiFi
// ---------------------------------------------------------------------------------
// Режимы работы WiFi
typedef enum {
    WIFI_CONDITION_OFF = 0,
    WIFI_CONDITION_STA = 1,
    WIFI_CONDITION_AP = 2
} wifi_condition_t;

// // Добавим явные значения для режимов
// #define WIFI_MODE_OFF 0
// #define WIFI_MODE_STA 1
// #define WIFI_MODE_AP  2

// ---------------------------------------------------------------------------------
//                                    MODBUS
// ---------------------------------------------------------------------------------
// Структура для хранения метаданных параметра
typedef struct
{
   uint16_t min; // Минимальное допустимое значение
   uint16_t max; // Максимальное допустимое значение
   uint16_t def; // Значение по умолчанию
} ParamMeta;

// Описание регистров настроек (макросы), сохраняются в NVS
#define REG_CURRENT_VERSION regs[0x00]   // Текущая версия конфигурации
#define REG_MODBUS_SLAVE_ADDR regs[0x01] // Slave адрес контроллера в сети MODBUS
#define REG_MODBUS_BAUD_INDEX regs[0x02] // Индекс скорости в сети MODBUS (0 ... 9)
#define REG_MODBUS_TIME_OUT regs[0x03]   // Минимальная задержка между пакетами в сети MODBUS (мс)
#define REG_SP_DAD_ADDR regs[0x04]       // Магистральный адрес приёмника в сети SP (адрес целевого прибора)
#define REG_SP_SAD_ADDR regs[0x05]       // Магистральный адрес источника в сети SP (адрес этого прибора)
#define REG_SP_BAUD_INDEX regs[0x06]     // Индекс скорости в сети SP (0 ... 9)
#define REG_SP_TIME_OUT regs[0x07]       // Минимальная задержка между пакетами в сети SP (мс)
#define REG_RESERVED regs[0x08]          // Зарезервирован
#define REG_WIFI_MODE ((wifi_condition_t)regs[0x09])  // Явное преобразование регистра в режим
                                         // WiFi: 0 - OFF, 1 - STA (клиент), 2 - AP (точка доступа)

// Командные регистры 
#define REG_SP_ERROR          regs[0x0A]  // Регистр ошибок обмена с целевым прибором
#define REG_SP_COMM           regs[0x0B]  // Регистр инициализации обмена с целевым прибором

// Регистры работы с разделом `response` - шаблоны ответов
#define REG_SP_READ_RESP      regs[0x0C]  // Регистр инициализации чтения (modbus) из HLD_READ_RESP
#define REG_SP_WRITE_RESP     regs[0x0D]  // Регистр инициализации записи (modbus) в HLD_WRITE_RESP

// Регистры работы с разделом `request`  - шаблоны запросов
#define REG_SP_READ_REQ       regs[0x0E]  // Регистр инициализации чтения (modbus) из HLD_READ_REQ
#define REG_SP_WRITE_REQ      regs[0x0F]  // Регистр инициализации записи (modbus) в HLD_WRITE_REQ

#define REG_REPEAT            regs[0x17]  // Repeat request period in seconds (5+)
#define REG_TARGET            regs[0x18]  // 

// Регистры работы с разделом `config`
#define REG_CONFIG_UPDATE     regs[0x19]  // Конфигурационный регистр
#define REG_CONFIG_OPERATION  regs[0x1A]  // Операция: [15] R/W, [14:8] тип, [7:0] индекс
#define REG_CONFIG_INDEX      regs[0x1B]  // Индекс конфигурации (STA = 0 1 2)

// Размер стека задачи в БАЙТАХ (8192)
#define WIFI_MANAGER_TASK_STACK_SIZE_BYTES   8192

// Метаданные для каждого параметра
static const ParamMeta param_meta[PARAMS_COUNT] = {
    {0,    999,  CURRENT_VERSION},  // 0x00 Версия          current version
    {0,    250,  0x04},             // 0x06 mb_slave_addr   modbus slave address
    {0,      9,  0x05},             // 0x02 mb_baud_index   0: 300 ... 9: 115200 baud
    {2,     10,  0x04},             // 0x03 mb_time_out     ms
    {0,     29,  0x00},             // 0x04 dad             0 ... 29:  магистральный адрес приёмника
    {0,    255,  0x80},             // 0x05 sad             0 ... 255: магистральный адрес источника
    {0,      9,  0x09},             // 0x06 sp_baud_index   0: 300 ... 9: 115200 baud
    {4,    100,    40},             // 0x07 sp_time_out     ms
    {0,    511,     0},             // 0x08 резерв          
    {0,      2,     2},             // 0x09 выбор WIFI      0: off,  1: sta (клиент), 2: ap (точка доступа)
};

#define MB_PORT_NUM UART_NUM_1
#define MB_QUEUE_SIZE 2       // Нужен для инициализации UART

// ---------------------------------------------------------------------------------
//                                    SP
// ---------------------------------------------------------------------------------

#define SP_PORT_NUM UART_NUM_2

#define SP_QUEUE_SIZE 2
#define SP_FRAME_TIMEOUT_MS_DEFAULT 10   // По факту

// Константы протокола
#define SOH 0x01        // Байт начала заголовка
#define ISI 0x1F        // Байт указателя кода функции
#define STX 0x02        // Байт начала тела сообщения
#define ETX 0x03        // Байт конца тела сообщения
#define DLE 0x10        // Байт символа-префикса
#define CRC_INIT 0x1021 // Битовая маска полинома

#define HT  0x09        // разделитель (табуляция)   
#define FF  0x0C        // разделитель (подача формы) 
#define CR  0x0D        // перевод строки (возврат каретки)
#define LF  0x0A        // подача строки (курсор вниз)

// Команды парсера
typedef enum
{
    CMD_READ_PARAMS = 0x1D03,             // Reading parameters
    CMD_WRITE_PARAM = 0x037F,             // Parameter entry nu
    CMD_READ_INDEX_ARRAY = 0x0C14,        // Reading the elements of an index array
    CMD_WRITE_INDEXED_ARRAY = 0x147F,     // Write the elements of an index array nu
    CMD_READ_TIME_STAMPS_ARRAY = 0x0E16,  // Reading time arrays nu
    CMD_READ_TIME_SLICE_ARCHIVE = 0x1820, // Reading a time slice of an archive nu
    CMD_WRITE_ARCHIVE_STRUCT = 0x1921,    // Defining the archive structure nu
} parser_command_t;

#define RAW_MODE_THRESHOLD 0xFF00        // Порог для режима RAW (отключить парсинг)
#define MAX_BLOCKS            20         // Максимальное количество параметров ??? точнее блоков
#define MIN_PAYLOAD_SIZE 4               // Минимальный размер полезной нагрузки

#define REG_REPEAT_MIN 5                 // Repeat request period min (seconds)


// // Режимы работы WiFi
// typedef enum {
//     WIFI_CONDITION_OFF = 0,
//     WIFI_CONDITION_STA = 1,
//     WIFI_CONDITION_AP = 2
// } wifi_condition_t;

// // Добавим явные значения для режимов
// #define WIFI_MODE_OFF 0
// #define WIFI_MODE_STA 1
// #define WIFI_MODE_AP  2

// // Явное преобразование регистра в режим
// #define REG_WIFI_MODE ((wifi_condition_t)regs[0x09])


// ---------------------------------------------------------------------------------
//                  Улучшенная архитектура обработки параметров
// ---------------------------------------------------------------------------------
// /**
//  * Вместо структуры предлагаю использовать динамическую систему тегов, которая будет:
//  * 1. Автоматически создавать элементы данных при первом появлении параметра
//  * 2. Хранить исторические значения для построения графиков
//  * 3. Обеспечивать быстрый доступ по имени параметра
//  */
// typedef struct {
//     char name[32];              // Имя параметра
//     float current_value;         // Текущее значение
//     float *history;              // Буфер истории значений
//     uint16_t history_size;       // Размер буфера истории
//     uint16_t history_index;      // Текущая позиция в истории
//     uint32_t last_update;        // Время последнего обновления
//     uint8_t flags;               // Флаги (для будущего расширения)
// } DataTag;

// // Функции для работы с тегами
// DataTag* get_or_create_tag(const char *name, uint16_t history_size);
// void update_tag_value(DataTag *tag, float value);
// DataTag* find_tag_by_name(const char *name);
