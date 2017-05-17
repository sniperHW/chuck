#include "chk_decoder.h"
#include "util/chk_log.h"
#include "util/chk_order.h"

void packet_decoder_update(chk_decoder *_,chk_bytechunk *b,uint32_t spos,uint32_t size) {
	packet_decoder *d = ((packet_decoder*)_);
    if(!d->b) {
	    d->b = chk_bytechunk_retain(b);
	    d->spos  = spos;
	    d->size = 0;
    }
    d->size += size;
}

chk_bytebuffer *packet_decoder_unpack(chk_decoder *_,int32_t *err) {
	packet_decoder *d = ((packet_decoder*)_);
	chk_bytebuffer *ret  = NULL;
	chk_bytechunk  *head = d->b;
	uint32_t        pk_len;
	uint32_t        pk_total,size,pos;
	do {
		if(d->size <= sizeof(pk_len)) break;
		size = sizeof(pk_len);
		pos  = d->spos;
		chk_bytechunk_read(head,(char*)&pk_len,&pos,&size);//读取payload大小
		pk_len = chk_ntoh32(pk_len);
		if(pk_len == 0) {
			CHK_SYSLOG(LOG_ERROR,"pk_len == 0");
			if(err) *err = chk_error_invaild_packet_size;
			break;
		}
		pk_total = size + pk_len;	
		if(pk_total > d->max) {
			CHK_SYSLOG(LOG_ERROR,"pk_total > d->max,pk_total:%d,max:%d",pk_total,d->max);
			if(err) *err = chk_error_packet_too_large;//数据包操作限制大小
			break;
		}
		if(pk_total > d->size) break;//没有足够的数据	
		ret = chk_bytebuffer_new_bychunk(head,d->spos,pk_total);

		if(!ret) {
			CHK_SYSLOG(LOG_ERROR,"chk_bytebuffer_new_bychunk() failed");
			if(err) *err = chk_error_no_memory;
			return NULL;
		}

		//调整pos及其b
		do {
			head = d->b;
			size = head->cap - d->spos;
			size = pk_total > size ? size:pk_total;
			d->spos  += size;
			pk_total-= size;
			d->size -= size;
			if(d->spos >= head->cap) { //当前b数据已经用完
				d->b = chk_bytechunk_retain(head->next);
				chk_bytechunk_release(head);
				d->spos = 0;
				if(!d->b) break;
			}
		}while(pk_total);			
	}while(0);
	return ret;
}

void packet_decoder_release(chk_decoder *_) {
	packet_decoder *d = ((packet_decoder*)_);
	if(d->b) chk_bytechunk_release(d->b);
	free(d);
}

packet_decoder *packet_decoder_new(uint32_t max) {
	packet_decoder *d = calloc(1,sizeof(*d));

	if(!d) {
		CHK_SYSLOG(LOG_ERROR,"calloc(packet_decoder) failed");		
		return NULL;
	}

	d->update = packet_decoder_update;
	d->unpack = packet_decoder_unpack;
	d->max    = max;
	d->release  = packet_decoder_release;
	return d;
}