#include <stdio.h>
#include <stdlib.h>
#include "util/base64.h"
#include "util/sha1.h"

int main() {
	const char *str = "Er+PNTJDnDT4G+VX8V/1GA==258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
	unsigned char output[20];
	sha1(str,strlen(str),output);
	printf("%s\n",b64_encode(output,20));
	return 0;
}