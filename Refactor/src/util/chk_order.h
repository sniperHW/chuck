#ifndef _CHK_ORDER_H
#define _CHK_ORDER_H

#include <stdint.h>
#include <arpa/inet.h>

#ifdef _LINUX
# include <endian.h>
#elif _FREEBSD
# include <machine/endian.h>
#endif

static inline uint16_t chk_swap16(uint16_t num) {
	return ((num << 8) & 0xFF00) | ((num & 0xFF00) >> 8); 
}

static inline uint32_t chk_swap32(uint32_t num) {
	return  (chk_swap16(((uint16_t*)&num)[0]) << 16) | chk_swap16(((uint16_t*)&num)[1]); 
}

static inline uint64_t chk_swap64(uint64_t num) {
	return ((uint64_t)chk_swap32(((uint32_t*)&num)[0]) << 32) | chk_swap32(((uint32_t*)&num)[1]);
}

#define chk_hton16(x) ((uint16_t)htons(x))

#define chk_ntoh16(x) ((uint16_t)ntohs(x))

#define chk_hton32(x) ((uint32_t)htonl(x))

#define chk_ntoh32(x) ((uint32_t)ntohl(x))

static inline uint64_t chk_hton64(uint64_t num) {
#if __BYTE_ORDER == __BIG_ENDIAN 
	return num;
#else
	return chk_swap64(num);
#endif
}

static inline uint64_t chk_ntoh64(uint64_t num) {
#if __BYTE_ORDER == __BIG_ENDIAN 
	return num;
#else
	return chk_swap64(num);
#endif
}

#endif