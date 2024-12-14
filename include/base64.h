/*
*Copyright (c) 2005, Jouni Malinen <j@w1.fi>
*
*This software may be distributed under the terms of the BSD license.
*See README for more details.
*/

#ifndef BASE64_H
#define BASE64_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

//unsigned char *base64_encode (const unsigned char *src, size_t len, size_t *out_len);
//unsigned char *base64_decode (const unsigned char *src, size_t len, size_t * out_len);
int32_t Base64_decode (const char *in, size_t in_len, uint8_t *out, size_t max_out_len);
int32_t Base64_encode (const unsigned char *data, size_t data_length, char *result, size_t max_result_length);


#endif /* BASE64_H */