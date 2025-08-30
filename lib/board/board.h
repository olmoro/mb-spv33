/*
  Файл board.h
  Проект GW-MORO
  pcb: spn.55
  2025 июнь 7
*/

#ifndef _BOARD_H_
#define _BOARD_H_

#include "driver/gpio.h"

#ifdef __cplusplus
extern "C"
{
#endif

    void boardInit();

    // API for blinking control
    void led_blink(gpio_num_t gpio, uint32_t on_time_ms, uint32_t off_time_ms, int32_t count);
    void led_stop_blink(gpio_num_t gpio);

    void ledsOn();
    void ledsRed();
    void ledsGreen();
    void ledsBlue();
    void ledsOff();

    void ledRedToggle();
    void ledGreenToggle();
    void ledBlueToggle();

    void flagA();
    void flagB();

#ifdef __cplusplus
}
#endif

#endif // !_BOARD_H_
