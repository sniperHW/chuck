#ifndef _CHK_SINGLETON_H
#define _CHK_SINGLETON_H

#include <pthread.h>

#define CHK_DECLARE_SINGLETON(TYPE)                                     \
        extern pthread_once_t TYPE##_key_once;                          \
        extern TYPE*          TYPE##_instance;                          \
        extern void (*TYPE##_fn_destroy)(void*);                        \
        void TYPE##_on_process_end();                                   \
        void TYPE##_once_routine()


#define CHK_IMPLEMENT_SINGLETON(TYPE,CREATE_FUNCTION,DESTYOY_FUNCTION)  \
        pthread_once_t TYPE##_key_once = PTHREAD_ONCE_INIT;             \
        TYPE*          TYPE##_instance = NULL;                          \
        void (*TYPE##_fn_destroy)(void*) = DESTYOY_FUNCTION;            \
        void TYPE##_on_process_end(){                                   \
            TYPE##_fn_destroy((void*)TYPE##_instance);                  \
        }                                                               \
        void TYPE##_once_routine(){                                     \
            TYPE##_instance = CREATE_FUNCTION();                        \
            if(TYPE##_fn_destroy) atexit(TYPE##_on_process_end);        \
        }

#define CHK_GET_INSTANCE(TYPE)                                          \
        ({do pthread_once(&TYPE##_key_once,TYPE##_once_routine);        \
            while(0);                                                   \
            TYPE##_instance;})

#endif