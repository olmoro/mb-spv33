#ifndef _REBOOT_H_
#define _REBOOT_H_

#include <stdint.h>
#include <stdio.h>
#include "nvs_flash.h"

#ifdef __cplusplus
extern "C"
{
#endif

    void custom_shutdown_handler();

#ifdef __cplusplus
}
#endif

#endif // !_REBOOT_H_
