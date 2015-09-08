#ifndef __DAEMONIZE_H
#define __DAEMONIZE_H
#include <syslog.h>
#include <fcntl.h>
#include <sys/resource.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <sys/stat.h>
#include <signal.h>

void     daemonize();
void     set_workdir(const char *workdir);


#ifdef _CHUCKLUA

#include "lua/lua_util.h"

void reg_luadaemon(lua_State *L); 

#endif



#endif
