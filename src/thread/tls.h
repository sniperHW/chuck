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

#ifndef _TLS_H
#define _TLS_H

#include <stdint.h>
#include <pthread.h>

#define MAX_TLS_SIZE 4096

void*    tls_get(uint16_t key);

int32_t  tls_set(uint16_t key,void*);

#endif
