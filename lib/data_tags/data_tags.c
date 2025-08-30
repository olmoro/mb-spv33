/**
 *  Используется динамическая система тегов, которая будет:
 * 1. Автоматически создавать элементы данных при первом появлении параметра
 * 2. Хранить исторические значения для построения графиков
 * 3. Обеспечивать быстрый доступ по имени параметра
 * 
 * Версия 22 июля 2025г. Не тестировалось
 */

#include "data_tags.h"
#include "project_config.h"
#include "esp_log.h"
#include <string.h>
#include <stdlib.h>

#define MAX_TAGS 50     // 

portMUX_TYPE tags_mutex = portMUX_INITIALIZER_UNLOCKED;

static const char *TAG = "DATA_TAGS";

static DataTag tags[MAX_TAGS];
static uint8_t tags_count = 0;

/**
 * @brief Выбирает тег или создаёт
 * @param name Указатель на имя ...
 * @param history_size ...
 * 
 * Логика обработки:
 * 1. Если не существует ...
 *    
 * 2. 
 */
DataTag *get_or_create_tag(const char *name, uint16_t history_size)
{
    // Поиск существующего тега
    for (int i = 0; i < tags_count; i++)
    {
        if (strcmp(tags[i].name, name) == 0)
        {
            return &tags[i];
        }
    }

    // Создание нового тега  
    // При создании тега инициализируем мьютекс.

    // ...

    if (tags_count >= MAX_TAGS)
    {
        ESP_LOGE(TAG, "Достигнут лимит тегов (%d)", MAX_TAGS);
        return NULL;
    }

    DataTag *new_tag = &tags[tags_count];
    strncpy(new_tag->name, name, sizeof(new_tag->name) - 1);
    new_tag->name[sizeof(new_tag->name) - 1] = '\0';

    // Выделение памяти для истории
    if (history_size > 0)
    {
        new_tag->history = malloc(history_size * sizeof(float));
        if (!new_tag->history)
        {
            ESP_LOGE(TAG, "Ошибка выделения памяти для истории тега '%s'", name);
            return NULL;
        }
        memset(new_tag->history, 0, history_size * sizeof(float));
        new_tag->history_size = history_size;
    }
    else
    {
        new_tag->history = NULL;
        new_tag->history_size = 0;
    }

    new_tag->history_index = 0;
    new_tag->current_value = 0.0f;
    new_tag->last_update = 0; // Будет устанавливаться при обновлении
    new_tag->flags = 0;

    tags_count++;
    ESP_LOGI(TAG, "Создан новый тег: %s (история: %d значений)", name, history_size);
    return new_tag;
}


/**
 * @brief Обновляет ...
 * @param tag Указатель на имя ...
 * @param value 
 * 
 * Логика обновления истории:
 * 
 * добавить логику оповещения
 */
void update_tag_value(DataTag *tag, float value)
{
    if (!tag)
        return;

    tag->current_value = value;

    // Обновление истории
    if (tag->history && tag->history_size > 0)
    {
        tag->history[tag->history_index] = value;
        tag->history_index = (tag->history_index + 1) % tag->history_size;
    }

    // Здесь можно добавить логику оповещения (например, для WebSocket)
    // ...
}

DataTag *find_tag_by_name(const char *name)
{
    for (int i = 0; i < tags_count; i++)
    {
        if (strcmp(tags[i].name, name) == 0)
        {
            return &tags[i];
        }
    }
    return NULL;
}

uint8_t get_tags_count(void) 
{
    return tags_count;
}

DataTag *get_tag_by_index(uint8_t index) 
{
    if (index < tags_count) 
    {
        return &tags[index];
    }
    return NULL;
}






/**
 * Преимущества этого подхода
 * 1. Автоматическое управление данными:
 *    - Теги создаются при первом появлении параметра
 *    - История хранится в кольцевом буфере
 *    - Нет необходимости в предварительной конфигурации
 * 
 * 2. Эффективное использование памяти:
 *    - История выделяется только для реально используемых параметров
 *    - Размер истории можно настраивать индивидуально
 * 
 * 3. Гибкость для визуализации:
 *    - Готовый буфер истории для построения графиков
 *    - Быстрый доступ к данным по имени тега
 *    - Временные метки обновлений
 * 
 * 4. Интеграция с WiFi:
 *    - Можно реализовать REST API для доступа к тегам
 *    - Поддержка WebSocket для реального времени
 *    - Легкая сериализация в JSON
 * 
 *      Пример API для WiFi
 * ```c
 * // В файле http_server.c
 * #include "data_tags.h"
 * 
 * // Получение списка всех тегов
 * esp_err_t get_tags_handler(httpd_req_t *req) 
 * {
 *     cJSON *root = cJSON_CreateObject();
 *     cJSON *tags_array = cJSON_CreateArray();
 * 
 *     for (int i = 0; i < tags_count; i++) 
 *     {
 *         cJSON *tag_obj = cJSON_CreateObject();
 *         cJSON_AddStringToObject(tag_obj, "name", tags[i].name);
 *         cJSON_AddNumberToObject(tag_obj, "value", tags[i].current_value);
 *         cJSON_AddItemToArray(tags_array, tag_obj);
 *     }
 * 
 *     cJSON_AddItemToObject(root, "tags", tags_array);
 *     char *json_str = cJSON_PrintUnformatted(root);
 *     httpd_resp_sendstr(req, json_str);
 *     free(json_str);
 *     cJSON_Delete(root);
 *     return ESP_OK;
 * }
 * 
 * // Получение истории тега
 * esp_err_t get_tag_history_handler(httpd_req_t *req) 
 * {
 *     char param[32];
 *     if (httpd_req_get_url_query_str(req, param, sizeof(param)) 
 *     {
 *         httpd_resp_send_404(req);
 *         return ESP_FAIL;
 *     }
 * 
 *     char name[32];
 *     if (httpd_query_key_value(param, "name", name, sizeof(name)) 
 *     {
 *         httpd_resp_send_404(req);
 *         return ESP_FAIL;
 *     }
 *     
 *     DataTag *tag = find_tag_by_name(name);
 *     if (!tag) {
 *         httpd_resp_send_404(req);
 *         return ESP_OK;
 *     }
 * 
 *     cJSON *root = cJSON_CreateObject();
 *     cJSON *history = cJSON_CreateFloatArray(tag->history, tag->history_size);
 *     cJSON_AddItemToObject(root, "history", history);
 * 
 *     char *json_str = cJSON_PrintUnformatted(root);
 *     httpd_resp_sendstr(req, json_str);
 *     free(json_str);
 *     cJSON_Delete(root);
 *     return ESP_OK;
 * }
 * ```
 * 
 *          Оптимизация для промышленного применения
 * 1. Защита данных:
 *    - Добавить мьютексы для доступа к тегам
 *    - Валидация входных значений
 *    - Ограничение длины имен параметров
 * 
 * 2. Эффективность памяти:
 *    - Использовать статический пул памяти вместо malloc
 *    - Реализовать механизм "устаревания" неиспользуемых тегов
 * 
 * 3. Диагностика:
 *    - Мониторинг использования памяти
 *    - Статистика по тегам (частота обновлений)
 *    - Логирование критических событий
 * 
 * ```c
 * // Пример расширенной структуры с защитой
 * typedef struct {
 *     char name[32];
 *     float current_value;
 *     float *history;
 *     uint16_t history_size;
 *     uint16_t history_index;
 *     uint32_t last_update;
 *     uint32_t update_count;  // Счетчик обновлений
 *     uint8_t flags;
 *     portMUX_TYPE mux;       // Мьютекс для безопасного доступа
 * } DataTag;
 * ```
 * 
 *      Заключение
 * Предложенная архитектура с динамическими тегами:
 * 1. Идеально подходит для вашей задачи визуализации данных
 * 2. Позволяет легко добавлять новые параметры без изменения кода
 * 3. Обеспечивает хранение истории для построения графиков
 * 4. Упрощает интеграцию с WiFi-интерфейсом
 * 5. Соответствует требованиям промышленного применения
 * 
 * Для реализации:
 * 1. Добавьте файл `data_tags.c` с реализацией системы тегов
 * 2. Обновите `sp_processing.c` для использования тегов
 * 3. Реализуйте HTTP-интерфейс для доступа к данным
 * 4. На клиентской стороне используйте библиотеки типа Chart.js для визуализации
 * 
 * Такой подход обеспечит вам гибкую и масштабируемую систему мониторинга параметров 
 * с возможностью построения графиков в реальном времени!
 */
