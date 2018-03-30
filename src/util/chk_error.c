#include "chk_error.h"


static const char *errno_to_name[] = 
{
#define XX(num,name) #name,
  CHUCK_ERRNO_MAP(XX)
#undef XX
};

const char *chk_error_unknow = "unknow errno";

const char *chk_get_errno_str(int err) {
	if(err < 0 || err >= sizeof(errno_to_name)/sizeof(const char*)) {
		return chk_error_unknow;
	} else {
		return errno_to_name[err];
	}
}