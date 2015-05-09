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

#ifndef __PROTOCOL_H
#define __PROTOCOL_H

#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "db/redis/client.h"


#define SIZE_TMP_BUFF 512 //size must enough for status/error string

typedef struct parse_tree{
	redisReply         *reply;
	struct parse_tree **childs;
	size_t              want;
	size_t 				pos;
	char                type;
	char                break_;
	char   				tmp_buff[SIZE_TMP_BUFF]; 	 	
}parse_tree;

static int32_t 
parse_string(parse_tree *current,char **str)
{
	redisReply *reply = current->reply;
	char c;
	if(!reply->str)
		reply->str = current->tmp_buff;
	if(!reply->len){
		do{
			char termi = current->break_;
			while((c = *(*str)++) != termi) {
				if(c != '\0')
		        	reply->str[current->pos++] = c;
		        else
		        	return REDIS_RETRY;
		    }
		    if(termi == '\n') break;
		    else current->break_ = '\n';
	    }while(1);
	}else{
		while(current->want && (c = *(*str)++) != '\0'){
			reply->str[current->pos++] = c;
			--current->want;
		}
		if(c == '\0')
			return REDIS_RETRY;		
	}
	reply->str[current->pos] = 0;
	return REDIS_OK;
}

static int32_t 
parse_integer(parse_tree *current,char **str)
{
	redisReply *reply = current->reply;	
	do{
		char c,termi = current->break_;
		while((c = *(*str)++) != termi) {
			if(c == '-'){
				current->want = -1;
			}else{
				if(c != '\0')
		        	reply->integer = (reply->integer*10)+(c - '0');
		        else
		        	return REDIS_RETRY;
	    	}
	    }				
	    if(termi == '\n') break;
	    else current->break_ = '\n';
    }while(1);

    reply->integer *= current->want;    
    return REDIS_OK;
}


static int32_t 
parse_breply(parse_tree *current,char **str)
{
	redisReply *reply = current->reply;
	if(!current->want){
		do{
			char c,termi = current->break_;
			while((c = *(*str)++) != termi) {
				if(c == '-'){
					reply->type = REDIS_REPLY_NIL;
					return REDIS_OK;
				}else{	
					if(c != '\0'){
			        	reply->len = (reply->len*10)+(c - '0');
					}else
			        	return REDIS_RETRY;
		        }	
		    }
		    if(termi == '\n') break;
		    else current->break_ = '\n';
	    }while(1);   
	    current->want = reply->len;
	}

	if(!reply->str){
		if(reply->len + 1 > SIZE_TMP_BUFF)
			reply->str = calloc(1,reply->len + 1);
	}
	return parse_string(current,str);  
}


static parse_tree*
parse_tree_new()
{

	parse_tree *tree = calloc(1,sizeof(*tree));
	tree->reply = calloc(1,sizeof(*tree->reply));
	tree->break_ = '\r';
	return tree;
}

static void 
parse_tree_del(parse_tree *tree)
{
	size_t i;
	if(tree->childs){
		for(i = 0; i < tree->want; ++i){
			parse_tree_del(tree->childs[i]);
		}
		free(tree->childs);
		free(tree->reply->element);
	}
	free(tree->reply);
	free(tree);
}

static int32_t  
parse(parse_tree *current,char **str);

static int32_t 
parse_mbreply(parse_tree *current,char **str)
{
	redisReply *reply = current->reply;
	size_t  i;
	int32_t ret;
	if(!current->want){
		do{
			char c,termi = current->break_;
			while((c = *(*str)++) != termi){
				if(c != '\0'){
		        	reply->elements = (reply->elements*10)+(c - '0');
				}else
		        	return REDIS_RETRY;	
		    }
		    if(termi == '\n') break;
		    else current->break_ = '\n';
	    }while(1);	    
	    current->want = reply->elements;
	}

	if(!current->childs){
		current->childs = calloc(current->want,sizeof(*current->childs));
		reply->element = calloc(current->want,sizeof(*reply->element));
		for(i = 0; i < current->want; ++i){
			current->childs[i] = parse_tree_new();
			reply->element[i] = current->childs[i]->reply;
		}
	}

	for(;current->pos < current->want; ++current->pos){
		if(*(*str) == '\0') 
			return REDIS_RETRY;
		ret = parse(current->childs[current->pos],str);
		if(ret != 0)
			return ret;
	}
	return REDIS_OK;	
}


static int32_t  
parse(parse_tree *current,char **str)
{
	int32_t ret = REDIS_RETRY;
	redisReply *reply = current->reply;		
	if(!current->type){
		do{
			char c = *(*str)++;
			if(c == '+'  || c == '-'  ||
			   c == ':'  || c == '$'  ||
			   c == '*')
			{
				current->type = c;
				break;
			}
			else if(c == '\0')
				return REDIS_RETRY;
		}while(1);
	}
	switch(current->type){
		case '+':{
			reply->type = REDIS_REPLY_STATUS;
			ret = parse_string(current,str);
			break;
		}
		case '-':{
			reply->type = REDIS_REPLY_ERROR;
			ret = parse_string(current,str);
			break;
		}
		case ':':{
			reply->type = REDIS_REPLY_INTEGER;
			current->want = 1;
			ret = parse_integer(current,str);
			break;
		}
		case '$':{
			reply->type = REDIS_REPLY_STRING;
			ret = parse_breply(current,str);
			break;
		}
		case '*':{
			reply->type = REDIS_REPLY_ARRAY;
			ret = parse_mbreply(current,str);
			break;
		}							
		default:{
			ret = REDIS_ERR;
			break;
		}
	}		
	return ret;
}

typedef struct{
	listnode node;
	char    *buff;
	size_t   size;
}word;

static inline size_t digitcount(uint32_t num){
	size_t i = 0;
	do{
		++i;
	}while(num/=10);
	return i;
}

static inline void u2s(uint32_t num,char **ptr){
	char *tmp = *ptr + digitcount(num);
	do{
		*--tmp = '0'+(char)(num%10);
		(*ptr)++;
	}while(num/=10);	
}


//for request
static packet*
convert(list *l,size_t space)
{
	static char *end = "\r\n";
	char *ptr,*ptr1;
	word *w;
	rawpacket *p;
	uint32_t headsize = (uint32_t)digitcount((uint32_t)list_size(l)); 		
	space += (headsize + 3);//plus head *,tail \r\n
	bytebuffer *buffer = bytebuffer_new(space);
	ptr1 = buffer->data;
	*ptr1++ = '*';
	u2s((uint32_t)list_size(l),&ptr1);
	for(ptr=end;*ptr;)*ptr1++ = *ptr++;	
	while(NULL != (w = (word*)list_pop(l))){
		*ptr1++ = '$';
		u2s((uint32_t)(w->size),&ptr1);
		for(ptr=end;*ptr;)*ptr1++ = *ptr++;		
		size_t i = 0;
		for(; i < w->size;) *ptr1++ = w->buff[i++];
		for(ptr=end;*ptr;)*ptr1++ = *ptr++;	
		free(w);
	}
	buffer->size = space;
	p = rawpacket_new_by_buffer(buffer,0);
	refobj_dec((refobj*)buffer);
	return (packet*)p;
}


static packet*
build_request(const char *cmd)
{
	list l;
	list_init(&l);
	size_t len   = strlen(cmd);
	word  *w = NULL;
	size_t i,j,space;
	char   quote  = 0;
	char   c;
	i = j = space = 0;
	for(; i < len; ++i){
		c = cmd[i];
		if(c == '\"' || c == '\''){
			if(!quote){
				quote = c;
			}else if(c == quote){
				quote = 0;
			}
		}
		if(c != ' '){
			if(!w){ 
				w = calloc(1,sizeof(*w));
				w->buff = (char*)&cmd[i];
			}
		}
		else if(!quote)
		{
			if(w){
				//word finish
				w->size = &cmd[i] - w->buff;
				space += digitcount((uint32_t)w->size) + 3;//plus head $,tail \r\n
				space += w->size + 2;//plus tail \r\n
				list_pushback(&l,(listnode*)w);	
				w = NULL;
				--i;
			}
		}
	}
	if(w){
		w->size = &cmd[i] - w->buff;
		space += digitcount((uint32_t)w->size) + 3;//plus head $,tail \r\n
		space += w->size + 2;//plus tail \r\n
		list_pushback(&l,(listnode*)w);
	}
	return convert(&l,space);
}

#endif