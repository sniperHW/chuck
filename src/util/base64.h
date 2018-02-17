#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <sys/types.h>

#ifndef _BASE64_H
#define _BASE64_H

#define base64_encoded_length(len) (((len + 2) / 3) * 4)
#define base64_decoded_length(len) (((len + 3) / 4) * 3)
int base64_encode(unsigned char *dst, const unsigned char *src, int len);
int base64_encode_url(unsigned char *dst, const unsigned char *src, int len);
int base64_decode(unsigned char *dst, const unsigned char *src, size_t slen);
int base64_decode_url(unsigned char *dst, const unsigned char *src, size_t slen);

#endif
