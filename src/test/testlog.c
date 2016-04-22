#include "util/chk_log.h"

int main(){
	CHK_SYSLOG(LOG_DEBUG,"debug");
	CHK_SYSLOG(LOG_WARN,"warn");
	CHK_SYSLOG(LOG_ERROR,"error");
	CHK_SYSLOG(LOG_CRITICAL,"critical");
	return 0;
}