#include "chuck.h"


void static on_signal(signaler *s,int32_t signum,void *ud)
{
	printf("on_signal\n");
	engine *e = (engine*)ud;
	engine_stop(e);
}

int main(int argc,char **argv){

	engine *e = engine_new();
	signaler *s = signaler_new(SIGINT,e);
	engine_associate(e,s,on_signal);
	engine_run(e);

	return 0;
}