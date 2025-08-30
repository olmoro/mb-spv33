/**
 *          Ключевые особенности:
 * 1. Управление миганием:
 *    - `led_blink(gpio, on_ms, off_ms, count)` - запуск мигания
 *    - `led_stop_blink(gpio)` - принудительная остановка
 * 
 * 2. Параметры мигания:
 *    - `on_time_ms` - длительность включения (мс)
 *    - `off_time_ms` - длительность выключения (мс)
 *    - `count` - количество циклов (-1 = бесконечно)
 * 
 * 3. Автоматическое прерывание:
 *    - Любое ручное управление светодиодом (вкл/выкл/переключение) останавливает мигание
 *    - После завершения заданных циклов мигание автоматически прекращается
 * 
 * 4. Технические детали:
 *    - Используется высокоточный таймер ESP (10 мс период)
 *    - Поддержка всех выводов RGB
 *    - Оптимизированная обработка состояний
 *    - Проверка ошибок инициализации таймера
 * 
 *          Пример использования:
 *      Мигание красным: 3 раза по 200мс вкл/400мс выкл
 *  led_blink(RGB_RED_GPIO, 200, 400, 3);
 * 
 *      Бесконечное мигание красным (1 сек период)
 *  led_blink(RGB_RED_GPIO, 500, 500, -1);
 * 
 *      Прервать мигание синего
 *  led_stop_blink(RGB_BLUE_GPIO);
 * 
 *  Данная реализация соответствует требованиям ESP-IDF v5.4 и предоставляет гибкий контроль  
 * над режимами работы светодиодов с минимальным потреблением ресурсов.
 * 
 * Версия от 7 июня 2025г.
 */

#include "board.h"
#include "project_config.h"
#include <stdio.h>
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"

// static const char *TAG = "BOARD";

// Blink control structure
typedef struct {
    bool is_blinking;
    uint32_t on_time_us;
    uint32_t off_time_us;
    int32_t count;
    int32_t current_count;
    bool current_state;
    uint64_t last_toggle_timestamp;
    gpio_num_t gpio;
} blink_state_t;

// Global variables
gpio_num_t _rgb_red_gpio = RGB_RED_GPIO;
gpio_num_t _rgb_green_gpio = RGB_GREEN_GPIO;
gpio_num_t _rgb_blue_gpio = RGB_BLUE_GPIO;
gpio_num_t _flag_a_gpio = A_FLAG_GPIO;
gpio_num_t _flag_b_gpio = B_FLAG_GPIO;

blink_state_t blink_states[3]; // RGB
esp_timer_handle_t blink_timer;

// Timer callback function
static void blink_timer_callback(void* arg) 
{
    uint64_t now = esp_timer_get_time();
    
    for (int i = 0; i < 3; i++) 
    {
        blink_state_t *led = &blink_states[i];
        
        if (led->is_blinking) 
        {
            uint64_t elapsed = now - led->last_toggle_timestamp;
            uint64_t required_duration = led->current_state ? led->on_time_us : led->off_time_us;

            if (elapsed >= required_duration) 
            {
                if (led->current_state) 
                {
                    // Switch ON -> OFF
                    led->current_state = false;
                    gpio_set_level(led->gpio, 0);
                    led->last_toggle_timestamp = now;

                    // Decrement counter (if not infinite)
                    if (led->count != -1) 
                    {
                        led->current_count--;
                        if (led->current_count <= 0) 
                        {
                            led->is_blinking = false;
                        }
                    }
                } 
                else 
                {
                    // Switch OFF -> ON (only if still blinking)
                    if (led->is_blinking) 
                    {
                        led->current_state = true;
                        gpio_set_level(led->gpio, 1);
                        led->last_toggle_timestamp = now;
                    }
                }
            }
        }
    }
}

// Initialize blink timer
void init_blink_timer() 
{
    const esp_timer_create_args_t timer_args = 
    {
        .callback = &blink_timer_callback,
        .name = "blink_timer"
    };
    ESP_ERROR_CHECK(esp_timer_create(&timer_args, &blink_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(blink_timer, 10000)); // 10ms
}

// Stop blinking for specific GPIO
void stop_blinking(gpio_num_t gpio) 
{
    for (int i = 0; i < 3; i++) 
    {
        if (blink_states[i].gpio == gpio) 
        {
            blink_states[i].is_blinking = false;
            break;
        }
    }
}

// Start blinking for specific GPIO
void start_blinking(gpio_num_t gpio, uint32_t on_time_ms, uint32_t off_time_ms, int32_t count) 
{
    if (count == 0) return;
    
    for (int i = 0; i < 3; i++) 
    {
        if (blink_states[i].gpio == gpio) 
        {
            blink_states[i].is_blinking = true;
            blink_states[i].on_time_us = on_time_ms * 1000;
            blink_states[i].off_time_us = off_time_ms * 1000;
            blink_states[i].count = count;
            blink_states[i].current_count = count;
            blink_states[i].current_state = true;
            blink_states[i].last_toggle_timestamp = esp_timer_get_time();
            
            gpio_set_level(gpio, 1); // Start with ON state
            break;
        }
    }
}


void boardInit()
{
    /* Инициализация GPIO (push/pull output) */
    gpio_reset_pin(_rgb_red_gpio);
    gpio_reset_pin(_rgb_green_gpio);
    gpio_reset_pin(_rgb_blue_gpio);
    gpio_reset_pin(_flag_a_gpio);
    gpio_reset_pin(_flag_b_gpio);

    gpio_set_direction(_rgb_red_gpio, GPIO_MODE_INPUT_OUTPUT);
    gpio_set_direction(_rgb_green_gpio, GPIO_MODE_INPUT_OUTPUT);
    gpio_set_direction(_rgb_blue_gpio, GPIO_MODE_INPUT_OUTPUT);
    gpio_set_direction(_flag_a_gpio, GPIO_MODE_INPUT_OUTPUT);
    gpio_set_direction(_flag_b_gpio, GPIO_MODE_INPUT_OUTPUT);

    /* Установка начального состояния (выключено) */
    gpio_set_level(_rgb_red_gpio, 0);
    gpio_set_level(_rgb_green_gpio, 0);
    gpio_set_level(_rgb_blue_gpio, 0);
    gpio_set_level(_flag_a_gpio, 0);
    gpio_set_level(_flag_b_gpio, 0);

    /* Initialize blink states */
    blink_states[0] = (blink_state_t){ .gpio = _rgb_red_gpio };
    blink_states[1] = (blink_state_t){ .gpio = _rgb_green_gpio };
    blink_states[2] = (blink_state_t){ .gpio = _rgb_blue_gpio };
    blink_states[3] = (blink_state_t){ .gpio = _flag_a_gpio };
    blink_states[4] = (blink_state_t){ .gpio = _flag_b_gpio };

    /* Start blink timer */
    init_blink_timer();
}

// Helper function to stop blinking for RGB LEDs
void stop_rgb_blinking() {
    stop_blinking(_rgb_red_gpio);
    stop_blinking(_rgb_green_gpio);
    stop_blinking(_rgb_blue_gpio);
}

// Functions with blink control
void ledsOn()
{
    stop_rgb_blinking();
    gpio_set_level(_rgb_red_gpio, 1);
    gpio_set_level(_rgb_green_gpio, 1);
    gpio_set_level(_rgb_blue_gpio, 1);
}

void ledsRed()
{
    stop_rgb_blinking();
    gpio_set_level(_rgb_red_gpio, 1);
    gpio_set_level(_rgb_green_gpio, 0);
    gpio_set_level(_rgb_blue_gpio, 0);
}

void ledsGreen()
{
    stop_rgb_blinking();
    gpio_set_level(_rgb_red_gpio, 0);
    gpio_set_level(_rgb_green_gpio, 1);
    gpio_set_level(_rgb_blue_gpio, 0);
}

void ledsBlue()
{
    stop_rgb_blinking();
    gpio_set_level(_rgb_red_gpio, 0);
    gpio_set_level(_rgb_green_gpio, 0);
    gpio_set_level(_rgb_blue_gpio, 1);
}

void ledsOff()
{
    stop_rgb_blinking();
    gpio_set_level(_rgb_red_gpio, 0);
    gpio_set_level(_rgb_green_gpio, 0);
    gpio_set_level(_rgb_blue_gpio, 0);
}

void ledRedToggle()
{
    stop_blinking(_rgb_red_gpio);
    gpio_set_level(_rgb_red_gpio, !gpio_get_level(_rgb_red_gpio));    
}

void ledGreenToggle()
{
    stop_blinking(_rgb_green_gpio);
    gpio_set_level(_rgb_green_gpio, !gpio_get_level(_rgb_green_gpio));    
}

void ledBlueToggle()
{
    stop_blinking(_rgb_blue_gpio);
    gpio_set_level(_rgb_blue_gpio, !gpio_get_level(_rgb_blue_gpio));    
}

void flagA()
{
    stop_blinking(_flag_a_gpio);
    gpio_set_level(_flag_a_gpio, !gpio_get_level(_flag_a_gpio));   
}

void flagB()
{
    stop_blinking(_flag_b_gpio);
    gpio_set_level(_flag_b_gpio, !gpio_get_level(_flag_b_gpio));    
}

// API for blinking control
void led_blink(gpio_num_t gpio, uint32_t on_time_ms, uint32_t off_time_ms, int32_t count) 
{
    start_blinking(gpio, on_time_ms, off_time_ms, count);
}

void led_stop_blink(gpio_num_t gpio) 
{
    stop_blinking(gpio);
}
