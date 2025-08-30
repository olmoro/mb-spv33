# mb-spv33
New


### Как проверить систему (пошагово):

1. **Проверка работы тегов**:
```bash
# В мониторе ESP-IDF
I (1234) PROCESSING: Пакет успешно обработан
I (1235) DATA_TAGS: Создан новый тег: temperature (история: 100 значений)
I (1236) DATA_TAGS: Обновлено значение temperature: 25.6
```

2. **Проверка WiFi подключения**:
```bash
I (2456) WiFiManager: Switching to STA mode
I (2567) WiFiManager: Found network: MyFactoryWiFi
I (2678) WiFiManager: got ip: 192.168.1.42
```

3. **Тестирование API**:
```bash
# Получение списка тегов
curl http://192.168.1.42/tags

# Пример ответа:
{
  "tags": [
    {"name": "temperature", "value": 25.6},
    {"name": "pressure", "value": 101.3}
  ]
}

# Получение истории
curl http://192.168.1.42/history?name=temperature

# Пример ответа:
{
  "name": "temperature",
  "history": [25.1, 25.2, 25.3, ...],
  "current_index": 42
}
```

4. **Проверка переключения режимов**:
```c
// Через Modbus-регистры
REG_WIFI_MODE = 0; // WiFi OFF
REG_WIFI_MODE = 1; // STA mode
REG_WIFI_MODE = 2; // AP mode
```

### Доработки для промышленного применения:

1. **Безопасность**:
```c
// В http_server.c добавить аутентификацию
static esp_err_t check_auth(httpd_req_t *req)
{
    char username[32], password[32];
    if (httpd_req_get_hdr_value_str(req, "Authorization", auth_buf, sizeof(auth_buf)) {
        // Проверка учетных данных
    }
    return ESP_OK;
}
```

2. **Управление памятью**:
```c
// В data_tags.c добавить
void free_unused_tags()
{
    for (int i = 0; i < tags_count; i++) {
        if (tags[i].last_update < time(NULL) - 86400) { // 24 часа неактивности
            free(tags[i].history);
            // Сдвиг массива
        }
    }
}
```

3. **Диагностика**:
```c
// Новый API для диагностики
curl http://192.168.1.42/diag

// Ответ:
{
  "wifi_mode": "STA",
  "connected_ap": "MyFactoryWiFi",
  "tags_count": 12,
  "memory_used": "45.2KB"
}
```

### Рекомендации по тестированию:

1. **Этап 1: Проверка парсинга данных**
   - Эмулируйте входящие пакеты через UART
   - Убедитесь, что теги создаются и обновляются

2. **Этап 2: Проверка WiFi подключения**
   - Тестируйте все 3 сети STA из конфигурации
   - Проверьте переключение между режимами

3. **Этап 3: Тестирование API**
   - Используйте curl, Postman или браузер
   - Проверьте все эндпоинты: /tags, /history, /value

4. **Этап 4: Стресс-тестирование**
   - 100+ тегов с частым обновлением
   - Длительная работа (24+ часа)
   - Скачки напряжения питания

После этих доработок и тестирования система будет полностью готова к промышленному применению! 💪
 