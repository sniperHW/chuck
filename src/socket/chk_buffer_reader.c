#include "chk_buffer_reader.h"

int32_t packet_reader_init(packet_reader *reader,chk_bytebuffer *buff) {

	if(!reader || !buff) {
		return -1;
	}

	reader->cur = buff->head;
	reader->buff = buff;
	reader->data_remain = buff->datasize - sizeof(uint32_t);
	reader->total_packet_size = buff->datasize;
	switch(reader->cur->cap - buff->spos) {
		case 1:{reader->cur = reader->cur->next;reader->readpos = 3;}break;
		case 2:{reader->cur = reader->cur->next;reader->readpos = 2;}break;
		case 3:{reader->cur = reader->cur->next;reader->readpos = 1;}break;
		default:reader->readpos = buff->spos + 4;
	}

	return 0;
}

int32_t reader_read(packet_reader *r,char *out,uint32_t size) {
    if(size > r->data_remain) return -1;//请求数据大于剩余数据
    r->cur = chk_bytechunk_read(r->cur,out,&r->readpos,&size);
    r->data_remain -= size;
    return 0;
}