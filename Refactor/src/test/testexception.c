#include <stdio.h>
#include "util/chk_exception.h"

void fun(){
	int *a = NULL;
	*a = 100;
	THROW("test");
}

int main(){
	TRY{
		fun();
	}CATCH_ALL{
		printf("CATCH_ALL\n");
	}ENDTRY
	//chk_exp_log_stack();
	return 0;
}
