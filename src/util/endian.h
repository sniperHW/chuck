/*
    Copyright (C) <2015>  <sniperHW@163.com>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef ___ENDIAN_H__
#define ___ENDIAN_H__

#include <stdint.h>
#include <arpa/inet.h>
#ifdef _LINUX
#include <endian.h>
#elif _FREEBSD
#include <machine/endian.h>
#endif

#ifdef _ENDIAN_CHANGE_

static inline uint16_t _swap16(uint16_t num){
	return ((num << 8) & 0xFF00) | ((num & 0xFF00) >> 8); 
}

static inline uint32_t _swap32(uint32_t num){
	return  (_swap16(((uint16_t*)&num)[0]) << 16) | _swap16(((uint16_t*)&num)[1]); 
}

static inline uint64_t _swap64(uint64_t num){
	return ((uint64_t)_swap32(((uint32_t*)&num)[0]) << 32) | _swap32(((uint32_t*)&num)[1]);
}

#define _hton16(x) ((uint16_t)htons(x))
#define _ntoh16(x) ((uint16_t)ntohs(x))
#define _hton32(x) ((uint32_t)htonl(x))
#define _ntoh32(x) ((uint32_t)ntohl(x))

static inline uint64_t _hton64(uint64_t num){
#if __BYTE_ORDER == __BIG_ENDIAN 
	return num;
#else
	return _swap64(num);
#endif
}

static inline uint64_t kn_ntoh64(uint64_t num){
#if __BYTE_ORDER == __BIG_ENDIAN 
	return num;
#else
	return kn_swap64(num);
#endif
}

#else

#define _hton16(x) (x)
#define _ntoh16(x) (x)
#define _hton32(x) (x)
#define _ntoh32(x) (x)

static inline uint64_t _hton64(uint64_t num){
	return num;
}

static inline uint64_t _ntoh64(uint64_t num){
	return num;
}

#endif

#endif