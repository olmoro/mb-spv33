#ifndef SP_PROCESSING_H
#define SP_PROCESSING_H

#include <stdint.h>
#include <stddef.h>

// Обработка принятых от целевого прибора данных
void sp_exe_in(const uint8_t* data, size_t length, uint16_t* out_buf, size_t* out_len);

#endif // SP_PROCESSING_H
