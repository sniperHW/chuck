#include "httppacket.h"

static packet*
httppacket_clone(packet*);

#define INIT_CONSTROUCTOR(p){\
	cast(packet*,p)->construct_write = NULL;             	\
	cast(packet*,p)->construct_read  = httppacket_clone;    \
	cast(packet*,p)->clone           = httppacket_clone;    \
}

void 
httppacket_dctor(void *_)
{
	st_header  *h;
	httppacket *p = cast(httppacket*,_);
	while((h = cast(st_header*,list_pop(&p->headers))))
		free(h);
}

httppacket*
httppacket_new(bytebuffer *b)
{
	httppacket *p = calloc(1,sizeof(*p));
	cast(packet*,p)->type = HTTPPACKET;
	p->method     = -1;
	if(b){
		cast(packet*,p)->head  = b;
		cast(packet*,p)->dctor = httppacket_dctor;
		refobj_inc(cast(refobj*,b));
	}
	INIT_CONSTROUCTOR(p);
	return p;		
}

static packet*
httppacket_clone(packet *_){
	st_header *h,*hh;
	listnode  *cur,*end;
	httppacket *o = cast(httppacket*,_);
	httppacket *p = calloc(1,sizeof(*p));
	cast(packet*,p)->type = HTTPPACKET;
	if(_->head){
		cast(packet*,p)->head  = _->head;
		cast(packet*,p)->dctor = httppacket_dctor;
		p->method              = o->method;
		p->url                 = o->url;
		p->status              = o->status;
		p->body                = o->body;
		p->bodysize            = o->bodysize;
		refobj_inc(cast(refobj*,_->head));
		cur  = list_begin(&o->headers);
		end  = list_end(&o->headers);
		for(; cur != end;cur = cur->next){
			h    = cast(st_header*,cur);
			hh   = calloc(1,sizeof(*hh));
			hh->field = h->field;
			hh->value = h->value;
			list_pushback(&p->headers,cast(listnode*,hh));
		}
	}
	INIT_CONSTROUCTOR(p);
	return cast(packet*,p);	
}

void
httppacket_on_buffer_expand(httppacket *p,bytebuffer *b)
{
	bytebuffer_set(&cast(packet*,p)->head,b);
}


int32_t
httppacket_on_header_field(httppacket *p,char *at, size_t length)
{
	st_header  *h = calloc(1,sizeof(*h));
	h->field      = at - &cast(packet*,p)->head->data[0];
	list_pushback(&p->headers,cast(listnode*,h));
	return 0;
}	

int32_t
httppacket_on_header_value(httppacket *p,char *at, size_t length)
{
	st_header *h = cast(st_header*,p->headers.tail);
	char   *data = cast(packet*,p)->head->data;
	h->value     = at - &data[0];
	return 0;
}

string*
httppacket_get_header(httppacket *p,const char *field)
{
	st_header *h;
	string        *ret = NULL;
	listnode    *cur  = list_begin(&p->headers);
	listnode    *end  = list_end(&p->headers);
	char        *data = cast(packet*,p)->head->data;
	for(; cur != end;cur = cur->next){
		h = cast(st_header*,cur);
		if(strcasecmp(field,&data[h->field]) == 0){
			if(!ret){
				ret = string_new(cast(const char*,&data[h->value]));
			}else{
				string_append(ret,",");
				string_append(ret,cast(const char*,&data[h->value]));
			}
		}
	}
	return ret;
}