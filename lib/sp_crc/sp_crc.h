#ifndef _SP_CRC_H_
#define _SP_CRC_H_

#include <stdint.h>
//#include "freertos/FreeRTOS.h"


#ifdef __cplusplus
extern "C"
{
#endif
 
    uint16_t sp_crc16(const uint8_t *msg, size_t len);

#ifdef __cplusplus
}
#endif

#endif // !_SP_CRC_H_
