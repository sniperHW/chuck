//一个解包器,包头2字节,表示后面数据大小.
typedef struct _decoder {
	void (*update)(chk_decoder*,chk_bytechunk *b,uint32_t spos,uint32_t size);
	chk_bytebuffer *(*unpack)(chk_decoder*,int32_t *err);
	void (*dctor)(chk_decoder*);
	uint32_t       spos;
	uint32_t       size;
	uint32_t       max;
	chk_bytechunk *b;
}_decoder;

static inline void _update(chk_decoder *_,chk_bytechunk *b,uint32_t spos,uint32_t size) {
	_decoder *d = ((_decoder*)_);
    if(!d->b) {
	    d->b = chk_bytechunk_retain(b);
	    d->spos  = spos;
	    d->size = 0;
    }
    d->size += size;
}

static inline chk_bytebuffer *_unpack(chk_decoder *_,int32_t *err) {
	_decoder *d = ((_decoder*)_);
	chk_bytebuffer *ret  = NULL;
	chk_bytechunk  *head = d->b;
	uint16_t        pk_len;
	uint32_t        pk_total,size,pos;
	do {
		if(d->size <= sizeof(pk_len)) break;
		size = sizeof(pk_len);
		pos  = d->spos;
		chk_bytechunk_read(head,(char*)&pk_len,&pos,&size);//读取payload大小
		pk_len = chk_ntoh16(pk_len);
		if(pk_len == 0) {
			if(err) *err = chk_error_invaild_packet_size;
			break;
		}
		pk_total = size + pk_len;
		if(pk_total > d->max) {
			if(err) *err = chk_error_packet_too_large;//数据包操作限制大小
			break;
		}
		if(pk_total > d->size) break;//没有足够的数据
		ret = chk_bytebuffer_new_bychunk_readonly(head,d->spos,pk_total);
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

static inline void _dctor(chk_decoder *_) {
	_decoder *d = ((_decoder*)_);
	if(d->b) chk_bytechunk_release(d->b);
	free(d);
}

static inline _decoder *_decoder_new(uint32_t max) {
	_decoder *d = calloc(1,sizeof(*d));
	d->update = _update;
	d->unpack = _unpack;
	d->max    = max;
	d->dctor  = _dctor;
	return d;
}