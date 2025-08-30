#ifndef _MB_CRC_H_
#define _MB_CRC_H_

#include <stdint.h>
#include "freertos/FreeRTOS.h"

#ifdef __cplusplus
extern "C"
{
#endif

    uint16_t mb_crc16(const uint8_t *buffer, size_t length);

#ifdef __cplusplus
}
#endif

#endif // !_MB_CRC_H_
