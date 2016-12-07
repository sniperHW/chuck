#include <stdio.h>
#include "util/chk_exception.h"


void fun1(){
	int *a = NULL;
	*a = 100;
	THROW("test");
}

void fun2(){
	fun1();
}

int main(){
	TRY{
		fun2();
	}CATCH_ALL{
		printf("exception stack\n");
		chk_exp_log_exption_stack();
		printf("call stack\n");
		chk_exp_log_call_stack("[call stack]");
	}ENDTRY
	//chk_exp_log_stack();
	return 0;
}
