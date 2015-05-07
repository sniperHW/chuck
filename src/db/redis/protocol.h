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

#define SIZE_TMP_BUFF 512

typedef struct parse_tree{
	redisReply         *reply;
	struct parse_tree **childs;
	size_t              want;
	size_t 				pos;
	char                type;
	char                linebreak;
	char   				tmp_buff[SIZE_TMP_BUFF]; 	 	
}parse_tree;

static int32_t 
parse_status(parse_tree *current,char **str)
{
	redisReply *reply = current->reply;
	char termi = current->linebreak ? '\n':'\r';
	do{
		while(**str != termi) {
			if(**str != '\0')
	        	current->tmp_buff[current->pos++] = *(*str)++;
	        else
	        	return REDIS_RETRY;
	    }
	    if(termi == '\n'){
	    	++(*str);
	    	break;
	    }
	    else{
	    	current->linebreak = 1;
	    	termi = '\n';
	    	++(*str);
	    }
    }while(1);


	current->tmp_buff[current->pos] = 0;
	reply->str = current->tmp_buff;
	current->pos = 0;
	return REDIS_OK;
}


static int32_t 
parse_error(parse_tree *current,char **str)
{
	redisReply *reply = current->reply;
	char termi = current->linebreak ? '\n':'\r';
	do{
		while(**str != termi) {
			if(**str != '\0')
	        	current->tmp_buff[current->pos++] = *(*str)++;
	        else
	        	return REDIS_RETRY;
	    }
	    if(termi == '\n'){
	    	++(*str);
	    	break;
	    }
	    else{
	    	current->linebreak = 1;
	    	termi = '\n';
	    	++(*str);
	    }
    }while(1);

	current->tmp_buff[current->pos] = 0;
	reply->str = current->tmp_buff;
	current->pos = 0;
	return REDIS_OK;
}


static int32_t 
parse_integer(parse_tree *current,char **str)
{
	redisReply *reply = current->reply;	
	char termi = current->linebreak ? '\n':'\r';
	do{
		while(**str != termi) {
			if(**str == '-'){
				current->want = -1;
			}else{
				if(**str != '\0')
		        	reply->integer = (reply->integer*10)+(**str - '0');
		        else
		        	return REDIS_RETRY;
	    	}
	    	++(*str);
	    }				
	    if(termi == '\n'){
	    	++(*str);
	    	break;
	    }
	    else{
	    	current->linebreak = 1;
	    	termi = '\n';
	    	++(*str);
	    }
    }while(1);


    if(current->want == -1)
    	reply->integer *= -1;    
    return REDIS_OK;
}


static int32_t 
parse_breply(parse_tree *current,char **str)
{
	redisReply *reply = current->reply;
	char termi;
	if(!current->want){
		termi = current->linebreak ? '\n':'\r';
		do{
			while(**str != termi) {
				if(**str == '-'){
					reply->len = -1;
				}else{	
					if(**str != '\0'){
			        	if(reply->len != -1)
			        		reply->len = (reply->len*10)+(**str - '0');
					}else
			        	return REDIS_RETRY;
		        }
		        (*str)++;	
		    }
		    if(termi == '\n'){
		    	current->linebreak = 0;
		    	++(*str);
		    	break;
		    }
		    else{
		    	current->linebreak = 1;
		    	termi = '\n';
		    	++(*str);
		    }
	    }while(1);
	    current->want = reply->len;
	}

	if(reply->len == -1){
		reply->type = REDIS_REPLY_NIL;
		return REDIS_OK;
	}
	if(!reply->str){
		if(reply->len + 1 <= SIZE_TMP_BUFF)
			reply->str = current->tmp_buff;
		else
			reply->str = calloc(1,reply->len + 1);
		current->pos = 0;
	}

	termi = current->linebreak ? '\n':'\r';
	do{
		while(**str != termi) {
			if(**str != '\0')
				reply->str[current->pos++] = **str;
			else
				return REDIS_RETRY;
			++(*str);
	    }
	    if(termi == '\n'){
	    	current->linebreak = 0;
	    	++(*str);
	    	break;
	    }
	    else{
	    	current->linebreak = 1;
	    	termi = '\n';
	    	++(*str);
	    }
    }while(1);
	reply->str[reply->len] = 0;
	return REDIS_OK;  
}

static parse_tree*
parse_tree_new()
{
	parse_tree *tree = calloc(1,sizeof(*tree));
	tree->reply = calloc(1,sizeof(*tree->reply));
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
		char termi = current->linebreak ? '\n':'\r';
		do{
			while(**str != termi) {
				if(**str != '\0'){
		        	reply->elements = (reply->elements*10)+(**str - '0');
				}else
		        	return REDIS_RETRY;
		        (*str)++;	
		    }
		    if(termi == '\n'){
		    	++(*str);
		    	break;
		    }
		    else{
		    	termi = '\n';
		    	++(*str);
		    }
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
		current->pos = 0;
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
			if(**str == '+'  || **str == '-'  ||
			   **str == ':'  || **str == '$'  ||
			   **str == '*')
			{
				current->type = **str;
				++(*str);
				break;
			}
			else if(**str == '\0')
				return REDIS_RETRY;
			++(*str);
		}while(1);
	}
	switch(current->type){
		case '+':{
			reply->type = REDIS_REPLY_STATUS;
			ret = parse_status(current,str);
			break;
		}
		case '-':{
			reply->type = REDIS_REPLY_ERROR;
			ret = parse_error(current,str);
			break;
		}
		case ':':{
			reply->type = REDIS_REPLY_INTEGER;
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
	size_t   b;
	size_t   e;
}word;


//for request

static rawpacket*
convert(list *l,size_t space)
{
	char  tmp[32];
	char  c;
	char *end = "\r\n";
	word *w;
	buffer_writer writer;
	bytebuffer *buffer;
	rawpacket *p;	
	space += sprintf(tmp,"%u",(uint32_t)list_size(l)) + 3;//plus head *,tail \r\n
	buffer = bytebuffer_new(space);
	buffer_writer_init(&writer,buffer,0);
	//write the head;
	c = '*';
	buffer_write(&writer,&c,sizeof(c));
	buffer_write(&writer,tmp,strlen(tmp));
	buffer_write(&writer,end,2);

	c = '$';
	while(NULL != (w = (word*)list_pop(l))){
		sprintf(tmp,"%u",(uint32_t)(w->e - w->b));
		buffer_write(&writer,&c,sizeof(c));	
		buffer_write(&writer,tmp,strlen(tmp));
		buffer_write(&writer,end,2);

		buffer_write(&writer,w->buff+w->b,w->e - w->b);
		buffer_write(&writer,end,2);

		free(w);
	}
	buffer_write(&writer,&c,sizeof(c));
	p = rawpacket_new_by_buffer(buffer,0);
	refobj_dec((refobj*)buffer);
	return p;
}


static packet*
build_request(const char *cmd)
{
	list l;
	list_init(&l);
	char  tmp[32];
	size_t len   = strlen(cmd);
	word  *w = NULL;
	size_t i,j,space;
	i = j = space = 0;
	for(; i < len; ++i){
		if(cmd[i] != ' '){
			if(!w){ 
				w = calloc(1,sizeof(*w));
				w->b = i;
				w->buff = (char*)cmd;
			}
		}else{
			if(w){
				//word finish
				w->e = i;
				space += (sprintf(tmp,"%u",(uint32_t)(w->e - w->b)) + 3);//plus head $,tail \r\n
				space += (w->e - w->b) + 2;//plus tail \r\n
				list_pushback(&l,(listnode*)w);	
				w = NULL;
				--i;
			}
		}
	}
	w->e = i;
	space += (sprintf(tmp,"%u",(uint32_t)(w->e - w->b)) + 3);//plus head $,tail \r\n
	space += (w->e - w->b) + 2;//plus tail \r\n
	list_pushback(&l,(listnode*)w);
	return (packet*)convert(&l,space);
}



#endif