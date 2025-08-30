#ifndef _HTTP_SERVER_H_
#define _HTTP_SERVER_H_

#include <stdbool.h>


// Функция проверки состояния сервера
bool http_server_is_running(void);

/**
 * @brief Инициализация и запуск HTTP-сервера
 */
void http_server_start(void);

/**
 * @brief Остановка HTTP-сервера
 */
void http_server_stop(void);






// // Получение списка всех тегов
// esp_err_t get_tags_handler(httpd_req_t *req);

// // Получение истории тега
// esp_err_t get_tag_history_handler(httpd_req_t *req);

// #include <stdio.h>
// #include <stdint.h>
// #include <string.h>

// /**
//  * @brief Обрабатывает ответ с элементами индексного массива
//  * @param fnc Код функции (должен быть 0x14)
//  * @param data Указатель на данные пакета (от SOH = 01h до ETX = 03h)
//  * @param len Длина данных пакета в байтах
//  */
// void handle_read_elements_index_array(const uint8_t fnc, const uint8_t *data, size_t len);

// /**
//  * @brief Обработка ответа с параметрами
//  * @param fnc Код функции (должен быть 0x03)
//  * @param data Указатель на данные пакета (от SOH = 01h до ETX = 03h)
//  * @param len Длина данных пакета в байтах
//  */
// void handle_read_parameter(const uint8_t fnc, const uint8_t *data, size_t len);

// // Объявление функции для отправки по WiFi
// #ifdef WIFI_ENABLED
// void wifi_send_data(const uint8_t *data, size_t len);
// #endif

#endif // !_HTTP_SERVER_H_
