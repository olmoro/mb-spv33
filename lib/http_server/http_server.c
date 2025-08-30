/**
 * Файл `http_server.c`содержит HTTP-сервер для доступа к данным тегов.
 *  Основные задачи:
 *  1. Инициализация HTTP-сервера.
 *  2. Реализация обработчиков для:
 *     - получения списка всех тегов (с текущими значениями);
 *     - получения истории конкретного тега;
 *     - возможно, установки значения тега (если нужно) - но в нашем случае данные только читаются.
 *  3. Использование библиотеки cJSON для формирования JSON-ответов.
 * Предположения:
 *  - У нас уже есть система тегов, реализованная в `data_tags.c`.
 *  - Мы используем ESP-IDF и его компонент `esp_http_server`.
 *  План:
 *  1. Подключим необходимые заголовочные файлы.
 *  2. Объявим обработчики запросов.
 *  3. Реализуем функцию инициализации HTTP-сервера.
 * 
 * Версия не тестирована, задел на будущее
 */

#include "http_server.h"
#include "project_config.h"
#include "wifi_manager.h"
#include "data_tags.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "cJSON.h"
#include <string.h>

static const char *TAG = "HTTP_SERVER";

static httpd_handle_t server = NULL;

// Функция проверки состояния сервера
bool http_server_is_running(void) {
    return server != NULL;
}

/**
 * @brief Обработчик для получения списка всех тегов
 */
static esp_err_t get_tags_handler(httpd_req_t *req)
{
    // Создаем корневой JSON объект
    cJSON *root = cJSON_CreateObject();
    cJSON *tags_array = cJSON_CreateArray();

    // В реальной системе здесь должна быть блокировка мьютексом!
    // portENTER_CRITICAL(&tags_mutex);
    
    // Перебираем все теги через функции доступа
    uint8_t count = get_tags_count();
    for (int i = 0; i < count; i++)
    {
        DataTag *tag = get_tag_by_index(i);
        if (!tag) continue;
        
        cJSON *tag_obj = cJSON_CreateObject();
        cJSON_AddStringToObject(tag_obj, "name", tag->name);
        cJSON_AddNumberToObject(tag_obj, "value", tag->current_value);
        cJSON_AddItemToArray(tags_array, tag_obj);
    }
    
    // portEXIT_CRITICAL(&tags_mutex);
    
    cJSON_AddItemToObject(root, "tags", tags_array);
    
    // Преобразуем JSON в строку
    char *json_str = cJSON_PrintUnformatted(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, json_str);
    
    // Освобождаем ресурсы
    free(json_str);
    cJSON_Delete(root);
    
    ESP_LOGI(TAG, "Отправлен список тегов (%d элементов)", count);
    return ESP_OK;
}

/**
 * @brief Обработчик для получения исторических данных тега
 */
static esp_err_t get_tag_history_handler(httpd_req_t *req)
{
    char query[50];
    char tag_name[32];
    
    // Извлекаем параметры запроса
    if (httpd_req_get_url_query_len(req) >= sizeof(query)) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Query too long");
        return ESP_FAIL;
    }
    
    httpd_req_get_url_query_str(req, query, sizeof(query));
    
    // Извлекаем значение параметра 'name'
    if (httpd_query_key_value(query, "name", tag_name, sizeof(tag_name)) != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing 'name' parameter");
        return ESP_FAIL;
    }
    
    // В реальной системе здесь должна быть блокировка мьютексом!
    DataTag *tag = find_tag_by_name(tag_name);
    if (!tag) {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Tag not found");
        return ESP_FAIL;
    }
    
    // Проверяем наличие истории
    if (!tag->history || tag->history_size == 0) {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "No history available");
        return ESP_FAIL;
    }
    
    // Создаем JSON-ответ
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "name", tag->name);
    
    // Добавляем исторические данные (кольцевой буфер)
    cJSON *history_array = cJSON_CreateArray();
    for (int i = 0; i < tag->history_size; i++) {
        int idx = (tag->history_index + i) % tag->history_size;
        cJSON_AddItemToArray(history_array, cJSON_CreateNumber(tag->history[idx]));
    }
    cJSON_AddItemToObject(root, "history", history_array);
    
    // Отправляем ответ
    char *json_str = cJSON_PrintUnformatted(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, json_str);
    
    // Освобождаем ресурсы
    free(json_str);
    cJSON_Delete(root);
    
    ESP_LOGI(TAG, "Отправлена история тега '%s'", tag_name);
    return ESP_OK;
}

/**
 * @brief Обработчик для получения текущего значения тега
 */
static esp_err_t get_tag_value_handler(httpd_req_t *req)
{
    char query[50];
    char tag_name[32];
    
    // Извлекаем параметры запроса
    if (httpd_req_get_url_query_len(req) >= sizeof(query)) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Query too long");
        return ESP_FAIL;
    }
    
    httpd_req_get_url_query_str(req, query, sizeof(query));
    
    // Извлекаем значение параметра 'name'
    if (httpd_query_key_value(query, "name", tag_name, sizeof(tag_name)) != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing 'name' parameter");
        return ESP_FAIL;
    }
    
    DataTag *tag = find_tag_by_name(tag_name);
    if (!tag) {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Tag not found");
        return ESP_FAIL;
    }
    
    // Формируем минимальный JSON-ответ
    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "value", tag->current_value);
    
    char *json_str = cJSON_PrintUnformatted(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, json_str);
    
    free(json_str);
    cJSON_Delete(root);
    
    ESP_LOGD(TAG, "Отправлено значение тега '%s': %.2f", tag_name, tag->current_value);
    return ESP_OK;
}

/**
 * @brief Обработчик для диагностической информации
 */
static esp_err_t get_diag_handler(httpd_req_t *req)
{
    cJSON *root = cJSON_CreateObject();
    
    // // Добавляем информацию о WiFi
    // cJSON_AddStringToObject(root, "wifi_mode", 
    //     current_wifi_mode == WIFI_CONDITION_OFF ? "OFF" :
    //     current_wifi_mode == WIFI_CONDITION_STA ? "STA" : "AP");
    
    // Используем функцию доступа к режиму
    wifi_condition_t mode = get_wifi_mode();
    
    cJSON_AddStringToObject(root, "wifi_mode", 
        mode == WIFI_CONDITION_OFF ? "OFF" :
        mode == WIFI_CONDITION_STA ? "STA" : "AP");


    // Добавляем информацию о тегах
    cJSON_AddNumberToObject(root, "tags_count", get_tags_count());
    
    // Добавляем информацию о памяти
    size_t free_heap = esp_get_free_heap_size();
    cJSON_AddNumberToObject(root, "free_heap", free_heap);
    
    // Формируем ответ
    char *json_str = cJSON_PrintUnformatted(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, json_str);
    
    free(json_str);
    cJSON_Delete(root);
    
    ESP_LOGI(TAG, "Отправлена диагностическая информация");
    return ESP_OK;
}

// Таблица URI-обработчиков
static const httpd_uri_t uri_handlers[] = {
    {.uri = "/tags",       .method = HTTP_GET, .handler = get_tags_handler},
    {.uri = "/history",    .method = HTTP_GET, .handler = get_tag_history_handler},
    {.uri = "/value",      .method = HTTP_GET, .handler = get_tag_value_handler},
    {.uri = "/diag",       .method = HTTP_GET, .handler = get_diag_handler},
};

/**
 * @brief Инициализация и запуск HTTP-сервера
 */
void http_server_start(void)
{
    if (server) {
        ESP_LOGW(TAG, "HTTP-сервер уже запущен");
        return;
    }
    
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    config.uri_match_fn = httpd_uri_match_wildcard;
    
    // Запускаем HTTP-сервер
    if (httpd_start(&server, &config) == ESP_OK) {
        // Регистрируем обработчики URI
        for (int i = 0; i < sizeof(uri_handlers) / sizeof(uri_handlers[0]); i++) {
            httpd_register_uri_handler(server, &uri_handlers[i]);
        }
        ESP_LOGI(TAG, "HTTP-сервер запущен на порту %d", config.server_port);
    } else {
        ESP_LOGE(TAG, "Ошибка запуска HTTP-сервера");
    }

    #ifdef OTA
        httpd_uri_t update_uri = {
            .uri = "/update",
            .method = HTTP_GET,
            .handler = update_get_handler};
        httpd_register_uri_handler(server, &update_uri);
    #endif
}

/**
 * @brief Остановка HTTP-сервера
 */
void http_server_stop(void)
{
    if (server) {
        httpd_stop(server);
        server = NULL;
        ESP_LOGI(TAG, "HTTP-сервер остановлен");
    }
}

#ifdef OTA
static esp_err_t update_get_handler(httpd_req_t *req)
{
    httpd_resp_send(req, "Starting OTA update", HTTPD_RESP_USE_STRLEN);
    xTaskCreate(ota_update_task, "ota_task", 8192, NULL, 5, NULL);
    return ESP_OK;
}
#endif
