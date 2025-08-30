#ifndef DATA_TAGS_H
#define DATA_TAGS_H

#include <stdint.h>
#include <stddef.h>

typedef struct
{
    char name[32];          // Имя параметра
    float current_value;    // Текущее значение
    float *history;         // Буфер истории значений
    uint16_t history_size;  // Размер буфера истории
    uint16_t history_index; // Текущая позиция в истории
    uint32_t last_update;   // Время последнего обновления
    uint8_t flags;          // Флаги (для будущего расширения)
} DataTag;

// Функции для работы с тегами
DataTag *get_or_create_tag(const char *name, uint16_t history_size);
void update_tag_value(DataTag *tag, float value);
DataTag *find_tag_by_name(const char *name);
uint8_t get_tags_count(void);                // Новая функция
DataTag *get_tag_by_index(uint8_t index);    // Новая функция








// #include <stdint.h>
// #include <stddef.h>
// #include "freertos/FreeRTOS.h"
//  #include "freertos/task.h"

//  extern portMUX_TYPE tags_mutex;

// /**
//  * Вместо структуры предлагаю использовать динамическую систему тегов, которая будет:
//  * 1. Автоматически создавать элементы данных при первом появлении параметра
//  * 2. Хранить исторические значения для построения графиков
//  * 3. Обеспечивать быстрый доступ по имени параметра
//  */

// // typedef struct
// // {
// //     char name[32];          // Имя параметра
// //     float current_value;    // Текущее значение
// //     float *history;         // Буфер истории значений
// //     uint16_t history_size;  // Размер буфера истории
// //     uint16_t history_index; // Текущая позиция в истории
// //     uint32_t last_update;   // Время последнего обновления
// //     uint8_t flags;          // Флаги (для будущего расширения)
// // } DataTag;

// // Пример расширенной структуры с защитой
// typedef struct {
//     char name[32];
//     float current_value;
//     float *history;
//     uint16_t history_size;
//     uint16_t history_index;
//     uint32_t last_update;
//     uint32_t update_count;  // Счетчик обновлений
//     uint8_t flags;
//     portMUX_TYPE mux;       // Мьютекс для безопасного доступа
// } DataTag;

// // Функции для работы с тегами
// DataTag *get_or_create_tag(const char *name, uint16_t history_size);
// void update_tag_value(DataTag *tag, float value);
// DataTag *find_tag_by_name(const char *name);

#endif // DATA_TAGS_H
