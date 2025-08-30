/*=====================================================================================
 * Description:
 *  // Функция обработки пакета стаффингом
 *
 * *input           - указатель на входной буфер
 * *output          - указатель на выходной буфер
 * input_len        - размер входного буфера
 * output_max_len   - зарезервированный размер выходного буфера
 * 
 * return           - актуальный размер выходного буфера
 *=====================================================================================
 */

#ifndef _STAFF_H_
#define _STAFF_H_

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

    int staff(const uint8_t *input, size_t input_len, uint8_t *output, size_t output_max_len);

#ifdef __cplusplus
}
#endif

#endif // _STAFF_H_
