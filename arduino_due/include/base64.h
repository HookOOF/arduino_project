#ifndef BASE64_H
#define BASE64_H

#include <stdint.h>
#include <stddef.h>

/**
 * Кодирование данных в Base64
 * @param input входные данные
 * @param inputLen длина входных данных
 * @param output буфер для выходной строки
 * @param outputLen размер буфера
 * @return длина закодированной строки или 0 при ошибке
 */
size_t base64_encode(const uint8_t* input, size_t inputLen, char* output, size_t outputLen);

/**
 * Вычисление размера буфера для Base64 кодирования
 * @param inputLen длина входных данных
 * @return необходимый размер буфера
 */
size_t base64_encode_length(size_t inputLen);

#endif // BASE64_H


