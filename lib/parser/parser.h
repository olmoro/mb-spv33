#ifndef _PARSER_H_
#define _PARSER_H_

#include <stdio.h>
#include <stdint.h>
#include <string.h>

/**
 * @brief Обрабатывает ответ с элементами индексного массива
 * @param fnc Код функции (должен быть 0x14)
 * @param data Указатель на данные пакета (от SOH = 01h до ETX = 03h)
 * @param len Длина данных пакета в байтах
 */
void handle_read_elements_index_array(const uint8_t fnc, const uint8_t *data, size_t len);

/**
 * @brief Обработка ответа с параметрами
 * @param fnc Код функции (должен быть 0x03)
 * @param data Указатель на данные пакета (от SOH = 01h до ETX = 03h)
 * @param len Длина данных пакета в байтах
 */
void handle_read_parameter(const uint8_t fnc, const uint8_t *data, size_t len);

// Объявление функции для отправки по WiFi
#ifdef WIFI_ENABLED
void wifi_send_data(const uint8_t *data, size_t len);
#endif

#endif // !_PARSER_H_
