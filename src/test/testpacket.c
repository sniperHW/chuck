#include "packet/chk_rpacket.h"
#include "packet/chk_wpacket.h"
#include "packet/chk_rawpacket.h"


int main() {

	chk_wpacket *wpk1 = chk_wpacket_new(64);
	chk_wpacket_writeU32(wpk1,100);
	chk_wpacket_writeU32(wpk1,101);
	chk_wpacket_writeCtr(wpk1,"hello world");

	chk_wpacket *wpk2 = (chk_wpacket*)chk_clone_packet(wpk1);
	chk_wpacket_writeU32(wpk2,102);
	chk_wpacket_writeU32(wpk2,103);
	chk_wpacket_writeCtr(wpk2,"hello world\nhello world\nhello world\nhello world");
	chk_wpacket_writeU32(wpk2,104);

	printf("----------------------rpk1-----------------\n");
	chk_rpacket *rpk1 = (chk_rpacket*)chk_make_readpacket(wpk1);
	printf("%u\n",chk_rpacket_readU32(rpk1));
	printf("%u\n",chk_rpacket_readU32(rpk1));
	printf("%s\n",chk_rpacket_readCStr(rpk1));
	printf("%u\n",chk_rpacket_readU32(rpk1));

	printf("----------------------rpk2-----------------\n");
	chk_rpacket *rpk2 = (chk_rpacket*)chk_make_readpacket(wpk2);
	printf("%u\n",chk_rpacket_readU32(rpk2));
	printf("%u\n",chk_rpacket_readU32(rpk2));
	printf("%s\n",chk_rpacket_readCStr(rpk2));
	printf("%u\n",chk_rpacket_readU32(rpk2));	
	printf("%u\n",chk_rpacket_readU32(rpk2));
	printf("%s\n",chk_rpacket_readCStr(rpk2));
	printf("%u\n",chk_rpacket_readU32(rpk2));
	return 0;
}