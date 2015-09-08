#include "socket/wrap/decoder.h"
#include "packet/rpacket.h"
#include "packet/rawpacket.h"

int main(){
	
	buffer_writer writer;
	bytebuffer *buff = bytebuffer_new(64);
	buffer_writer_init(&writer,buff,0);
	uint16_t len = 16;
	buffer_write(&writer,&len,sizeof(len)); 
	int32_t v = 10;
	buffer_write(&writer,&v,sizeof(v));
	v = 100;
	buffer_write(&writer,&v,sizeof(v));
	v = strlen("hello") + 1;
	buffer_write(&writer,&v,sizeof(uint16_t));
	buffer_write(&writer,"hello",strlen("hello")+1);

	decoder *d = rpacket_decoder_new(1024);
	decoder_init(d,buff,0);
	d->size = buff->size;
	rpacket *r = (rpacket*)d->unpack(d,NULL);

	printf("%d\n",rpacket_read_uint32(r));
	printf("%d\n",rpacket_read_uint32(r));
	printf("%s\n",rpacket_read_string(r));

	packet_del((packet*)r);
	
	decoder_del(d);
	printf("--------2 packet-----------\n");
	d = rpacket_decoder_new(1024);

	buff = bytebuffer_new(64);
	buffer_writer_init(&writer,buff,0);
	len = 46;
	buffer_write(&writer,&len,sizeof(len)); 
	v = 10;
	buffer_write(&writer,&v,sizeof(v));
	v = 100;
	buffer_write(&writer,&v,sizeof(v));
	v = strlen("hellofasdfasdfasdfafdasdfasfdasdfas") + 1;
	buffer_write(&writer,&v,sizeof(uint16_t));
	buffer_write(&writer,"hellofasdfasdfasdfafdasdfasfdasdfas",
			     strlen("hellofasdfasdfasdfafdasdfasfdasdfas")+1);

	buff->next = bytebuffer_new(64);

	refobj_inc((refobj*)buff->next);

	len = 16;
	buffer_write(&writer,&len,sizeof(len)); 
	v = 10;
	buffer_write(&writer,&v,sizeof(v));
	v = 100;
	buffer_write(&writer,&v,sizeof(v));
	v = strlen("hello") + 1;
	buffer_write(&writer,&v,sizeof(uint16_t));
	buffer_write(&writer,"hello",strlen("hello")+1);


	decoder_init(d,buff,0);
	d->size = buff->size + buff->next->size;

	printf("-------------r1-------------\n");
	rpacket *r1 = (rpacket*)d->unpack(d,NULL);

	printf("%d\n",rpacket_read_uint32(r1));
	printf("%d\n",rpacket_read_uint32(r1));
	printf("%s\n",rpacket_read_string(r1));

	printf("-------------r2-------------\n");

	rpacket *r2 = (rpacket*)d->unpack(d,NULL);

	printf("%d\n",rpacket_read_uint32(r2));
	printf("%d\n",rpacket_read_uint32(r2));
	printf("%s\n",rpacket_read_string(r2));	

	packet_del((packet*)r1);
	packet_del((packet*)r2);
	decoder_del(d);
	return 0;
}