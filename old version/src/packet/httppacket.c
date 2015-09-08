#include "httppacket.h"

static packet*
httppacket_clone(packet*);

#define INIT_CONSTROUCTOR(p){\
	cast(packet*,p)->construct_write = NULL;             	\
	cast(packet*,p)->construct_read  = httppacket_clone;    \
	cast(packet*,p)->clone           = httppacket_clone;    \
}

void httppacket_dctor(void *_)
{
	st_header  *h;
	httppacket *p = cast(httppacket*,_);
	while((h = cast(st_header*,list_pop(&p->headers))))
	{
		string_release(h->field);
		string_release(h->value);
	}
	if(p->body) string_release(p->body);
	if(p->url)  string_release(p->url);
	if(p->status) string_release(p->status);
}

httppacket *httppacket_new()
{
	httppacket *p = calloc(1,sizeof(*p));
	cast(packet*,p)->type = HTTPPACKET;
	p->method     = -1;
	cast(packet*,p)->dctor = httppacket_dctor;
	INIT_CONSTROUCTOR(p);
	return p;		
}

static packet *httppacket_clone(packet *_){
	st_header *h,*hh;
	listnode  *cur,*end;
	httppacket *o = cast(httppacket*,_);
	httppacket *p = calloc(1,sizeof(*p));
	cast(packet*,p)->type = HTTPPACKET;
	cast(packet*,p)->dctor = httppacket_dctor;
	p->method              = o->method;
	p->url                 = string_retain(o->url);
	p->status              = string_retain(o->status);
	p->body                = string_retain(o->body);
	cur  = list_begin(&o->headers);
	end  = list_end(&o->headers);
	for(; cur != end;cur = cur->next){
		h    = cast(st_header*,cur);
		hh   = calloc(1,sizeof(*hh));
		hh->field = string_retain(h->field);
		hh->value = string_retain(h->value);
		list_pushback(&p->headers,cast(listnode*,hh));
	}
	INIT_CONSTROUCTOR(p);
	return cast(packet*,p);	
}


int32_t httppacket_on_header_field(httppacket *p,char *at, size_t length)
{
	if(!p->current){
		p->current = calloc(1,sizeof(*p->current));
		p->current->field = string_new(at,length);
	}else
		string_append(p->current->field,at,length);
	return 0;
}	

int32_t httppacket_on_header_value(httppacket *p,char *at, size_t length)
{
	st_header *h;
	if(p->current){
		list_pushback(&p->headers,cast(listnode*,p->current));
		p->current->value = string_new(at,length);
		p->current = NULL;
	}else{
		h = cast(st_header*,p->headers.tail);
		string_append(h->value,at,length);
	}
	return 0;
}


/*
string *httppacket_get_header(httppacket *p,const char *field)
{
	st_header *h;
	string        *ret = NULL;
	listnode    *cur  = list_begin(&p->headers);
	listnode    *end  = list_end(&p->headers);
	char        *data = cast(packet*,p)->head->data;
	for(; cur != end;cur = cur->next){
		h = cast(st_header*,cur);
		if(strcasecmp(field,&data[h->field]) == 0){
			if(!ret)
				ret = string_new(cast(const char*,&data[h->value]));
			else
				string_append(ret,",",cast(const char*,&data[h->value]));
			
		}
	}
	return ret;
}*/