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
#ifndef _STRING_H__
#define _STRING_H__
#include <stdint.h>

typedef struct string string;

string*     string_new(const char *);

string*     string_copy_new(string *);

void        string_del(string*);

const char *string_cstr(string*);

int32_t     string_len(string*);
    
void        string_append(string*,const char*);


#endif
