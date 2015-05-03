//#define _GNU_SOURCE
#include <stdio.h>
#include <signal.h>
#include "util/exception.h"
#include "thread/thread.h"

void func1()
{
    int *ptr = NULL;
    *ptr = 10;
}

void entry1()
{
    TRY{
        func1();
    }CATCH(except_segv_fault)
    {
        printf("catch:%s stack below\n",exception_description(EXPNO));
        PRINT_CALL_STACK;
    }CATCH_ALL
    {
        printf("catch all: %s\n",exception_description(EXPNO));
    }ENDTRY;
    
    return;
}


struct testst{
	int a;
	int b;
};

void func2()
{
    struct testst *a = NULL;
    printf("%d\n",a->b);
    printf("func2 end\n");
}

void entry2()
{
    TRY{
        func2();
    }CATCH(except_arith){
        PRINT_CALL_STACK;
    }CATCH_ALL{
        PRINT_CALL_STACK;
    }ENDTRY;
    
    return;
}

void func3()
{
    THROW(testexception3);
}

void *routine1(void *arg)
{
    TRY{
        func3();
    }CATCH_ALL{
        PRINT_CALL_STACK;
    }ENDTRY;
    
    return NULL;
}

static volatile int8_t stop = 0;

static void stop_handler(int signo){
    printf("stop_handler\n");
    stop = 1;
}

void setup_signal_handler()
{
    struct sigaction act;
    bzero(&act, sizeof(act));
    act.sa_handler = stop_handler;
    sigaction(SIGINT, &act, NULL);
    sigaction(SIGTERM, &act, NULL);
}


int main()
{
    setup_signal_handler();
    entry2();
    
    thread *t1 = thread_new(JOINABLE,routine1,NULL);
    thread *t2 = thread_new(JOINABLE,routine1,NULL);
    thread_del(t1);    
    thread_del(t2);
    getchar();
    printf("main end\n");
    return 0;
}
