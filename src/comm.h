/*
    Copyright (C) <2015>  <sniperHW@163.com>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#ifndef _COMMON_H
#define _COMMON_H

#include    <unistd.h>
#include	<sys/types.h>	/* basic system data types */
#include	<sys/socket.h>	/* basic socket definitions */
#include	<sys/time.h>	/* timeval{} for select() */
#include	<time.h>		/* timespec{} for pselect() */
#include	<netinet/in.h>	/* sockaddr_in{} and other Internet defns */
#include	<arpa/inet.h>	/* inet(3) functions */
#include	<errno.h>
#include	<fcntl.h>		/* for nonblocking */
#include	<netdb.h>
#include	<signal.h>
#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>
#include	<sys/stat.h>	/* for S_xxx file mode constants */
#include	<sys/uio.h>		/* for iovec{} and readv/writev */
#include	<unistd.h>
#include	<sys/wait.h>
#include	<sys/un.h>		/* for Unix domain sockets */
#include    <net/if.h>
#include    <sys/ioctl.h>
#include    <netinet/tcp.h>
#include    <fcntl.h>
#include    <stdint.h>
#include    "util/list.h"


#ifdef _LINUX

#include    <sys/epoll.h>

enum{
    EVENT_READ  =  EPOLLIN, //| EPOLLERR | EPOLLHUP | EPOLLRDHUP),
    EVENT_WRITE =  EPOLLOUT,    
};

#elif _BSD

#include    <sys/event.h>

enum{
    EVENT_READ  =  EVFILT_READ,
    EVENT_WRITE =  EVFILT_WRITE,    
};

#else

#error "un support platform!"   

#endif

typedef struct {
    union{
        struct sockaddr_in  in;   //for ipv4 
        struct sockaddr_in6 in6;  //for ipv6
        struct sockaddr_un  un;   //for unix domain
    }; 
}sockaddr_;

typedef struct
{
    listnode      lnode;
    struct iovec *iovec;
    int32_t       iovec_count;
    sockaddr_     addr;//for datagram request
}iorequest;

typedef void *(*generic_callback)(void*);

typedef struct engine engine;

typedef struct handle{
    int32_t  fd;
    union{
        int32_t     events;
        struct{
            int16_t set_read;
            int16_t set_write;
        };
    };
    engine *e;
    int32_t (*imp_engine_add)(engine*,struct handle*,generic_callback);          
    void    (*on_events)(struct handle*,int32_t events);
}handle;


#ifndef TEMP_FAILURE_RETRY
#define TEMP_FAILURE_RETRY(expression)\
    ({ long int __result;\
    do __result = (long int)(expression);\
    while(__result == -1L&& errno == EINTR);\
    __result;})
#endif


#define MAX_UINT32 0xffffffff
#define MAX_UINT16 0xffff
#define likely(x) __builtin_expect(!!(x), 1)  
#define unlikely(x) __builtin_expect(!!(x), 0)

static inline int32_t 
is_pow2(uint32_t size)
{
    return !(size&(size-1));
}

static inline uint32_t 
size_of_pow2(uint32_t size)
{
    if(is_pow2(size)) return size;
    size = size-1;
    size = size | (size>>1);
    size = size | (size>>2);
    size = size | (size>>4);
    size = size | (size>>8);
    size = size | (size>>16);
    return size + 1;
}

static inline uint8_t 
get_pow2(uint32_t size)
{
    uint8_t pow2 = 0;
    if(!is_pow2(size)) size = (size << 1);
    while(size > 1){
        pow2++;
        size = size >> 1;
    }
    return pow2;
}

static inline uint32_t 
align_size(uint32_t size,uint32_t align)
{
    align = size_of_pow2(align);
    if(align < 4) align = 4;
    uint32_t mod = size % align;
    if(mod == 0) 
        return size;
    else
        return (size/align + 1) * align;
}

#if 0
#define EPERM           1 /* Operation not permitted */
#define ENOENT          2 /* No such file or directory */
#define ESRCH           3 /* No such process */
#define EINTR           4 /* Interrupted system call */
#define EIO             5 /* I/O error */
#define ENXIO           6 /* No such device or address */
#define E2BIG           7 /* Arg list too long */
#define ENOEXEC         8 /* Exec format error */
#define EBADF           9 /* Bad file number */
#define ECHILD          10 /* No child processes */
#define EAGAIN          11 /* Try again */
#define ENOMEM          12 /* Out of memory */
#define EACCES          13 /* Permission denied */
#define EFAULT          14 /* Bad address */
#define ENOTBLK         15 /* Block device required */
#define EBUSY           16 /* Device or resource busy */
#define EEXIST          17 /* File exists */
#define EXDEV           18 /* Cross-device link */
#define ENODEV          19 /* No such device */
#define ENOTDIR         20 /* Not a directory */
#define EISDIR          21 /* Is a directory */
#define EINVAL          22 /* Invalid argument */
#define ENFILE          23 /* File table overflow */
#define EMFILE          24 /* Too many open files */
#define ENOTTY          25 /* Not a typewriter */
#define ETXTBSY         26 /* Text file busy */
#define EFBIG           27 /* File too large */
#define ENOSPC          28 /* No space left on device */
#define ESPIPE          29 /* Illegal seek */
#define EROFS           30 /* Read-only file system */
#define EMLINK          31 /* Too many links */
#define EPIPE           32 /* Broken pipe */
#define EDOM            33 /* Math argument out of domain of func */
#define ERANGE          34 /* Math result not representable */
#define EDEADLK         35      /* Resource deadlock would occur */
#define ENAMETOOLONG    36      /* File name too long */
#define ENOLCK          37      /* No record locks available */
#define ENOSYS          38      /* Function not implemented */
#define ENOTEMPTY       39      /* Directory not empty */
#define ELOOP           40      /* Too many symbolic links encountered */
#define EWOULDBLOCK     EAGAIN  /* Operation would block */
#define ENOMSG          42      /* No message of desired type */
#define EIDRM           43      /* Identifier removed */
#define ECHRNG          44      /* Channel number out of range */
#define EL2NSYNC        45      /* Level 2 not synchronized */
#define EL3HLT          46      /* Level 3 halted */
#define EL3RST          47      /* Level 3 reset */
#define ELNRNG          48      /* Link number out of range */
#define EUNATCH         49      /* Protocol driver not attached */
#define ENOCSI          50      /* No CSI structure available */
#define EL2HLT          51      /* Level 2 halted */
#define EBADE           52      /* Invalid exchange */
#define EBADR           53      /* Invalid request descriptor */
#define EXFULL          54      /* Exchange full */
#define ENOANO          55      /* No anode */
#define EBADRQC         56      /* Invalid request code */
#define EBADSLT         57      /* Invalid slot */
#define EDEADLOCK       EDEADLK
#define EBFONT          59      /* Bad font file format */
#define ENOSTR          60      /* Device not a stream */
#define ENODATA         61      /* No data available */
#define ETIME           62      /* Timer expired */
#define ENOSR           63      /* Out of streams resources */
#define ENONET          64      /* Machine is not on the network */
#define ENOPKG          65      /* Package not installed */
#define EREMOTE         66      /* Object is remote */
#define ENOLINK         67      /* Link has been severed */
#define EADV            68      /* Advertise error */
#define ESRMNT          69      /* Srmount error */
#define ECOMM           70      /* Communication error on send */
#define EPROTO          71      /* Protocol error */
#define EMULTIHOP       72      /* Multihop attempted */
#define EDOTDOT         73      /* RFS specific error */
#define EBADMSG         74      /* Not a data message */
#define EOVERFLOW       75      /* Value too large for defined data type */
#define ENOTUNIQ        76      /* Name not unique on network */
#define EBADFD          77      /* File descriptor in bad state */
#define EREMCHG         78      /* Remote address changed */
#define ELIBACC         79      /* Can not access a needed shared library */
#define ELIBBAD         80      /* Accessing a corrupted shared library */
#define ELIBSCN         81      /* .lib section in a.out corrupted */
#define ELIBMAX         82      /* Attempting to link in too many shared libraries */
#define ELIBEXEC        83      /* Cannot exec a shared library directly */
#define EILSEQ          84      /* Illegal byte sequence */
#define ERESTART        85      /* Interrupted system call should be restarted */
#define ESTRPIPE        86      /* Streams pipe error */
#define EUSERS          87      /* Too many users */
#define ENOTSOCK        88      /* Socket operation on non-socket */
#define EDESTADDRREQ    89      /* Destination address required */
#define EMSGSIZE        90      /* Message too long */
#define EPROTOTYPE      91      /* Protocol wrong type for socket */
#define ENOPROTOOPT     92      /* Protocol not available */
#define EPROTONOSUPPORT 93      /* Protocol not supported */
#define ESOCKTNOSUPPORT 94      /* Socket type not supported */
#define EOPNOTSUPP      95      /* Operation not supported on transport endpoint */
#define EPFNOSUPPORT    96      /* Protocol family not supported */
#define EAFNOSUPPORT    97      /* Address family not supported by protocol */
#define EADDRINUSE      98      /* Address already in use */
#define EADDRNOTAVAIL   99      /* Cannot assign requested address */
#define ENETDOWN        100     /* Network is down */
#define ENETUNREACH     101     /* Network is unreachable */
#define ENETRESET       102     /* Network dropped connection because of reset */
#define ECONNABORTED    103     /* Software caused connection abort */
#define ECONNRESET      104     /* Connection reset by peer */
#define ENOBUFS         105     /* No buffer space available */
#define EISCONN         106     /* Transport endpoint is already connected */
#define ENOTCONN        107     /* Transport endpoint is not connected */
#define ESHUTDOWN       108     /* Cannot send after transport endpoint shutdown */
#define ETOOMANYREFS    109     /* Too many references: cannot splice */
#define ETIMEDOUT       110     /* Connection timed out */
#define ECONNREFUSED    111     /* Connection refused */
#define EHOSTDOWN       112     /* Host is down */
#define EHOSTUNREACH    113     /* No route to host */
#define EALREADY        114     /* Operation already in progress */
#define EINPROGRESS     115     /* Operation now in progress */
#define ESTALE          116     /* Stale NFS file handle */
#define EUCLEAN         117     /* Structure needs cleaning */
#define ENOTNAM         118     /* Not a XENIX named type file */
#define ENAVAIL         119     /* No XENIX semaphores available */
#define EISNAM          120     /* Is a named type file */
#define EREMOTEIO       121     /* Remote I/O error */
#define EDQUOT          122     /* Quota exceeded */
#define ENOMEDIUM       123     /* No medium found */
#define EMEDIUMTYPE     124     /* Wrong medium type */
#define ECANCELED       125     /* Operation Canceled */
#define ENOKEY          126     /* Required key not available */
#define EKEYEXPIRED     127     /* Key has expired */
#define EKEYREVOKED     128     /* Key has been revoked */
#define EKEYREJECTED    129     /* Key was rejected by service */
/* for robust mutexes */
#define EOWNERDEAD      130     /* Owner died */
#define ENOTRECOVERABLE 131     /* State not recoverable */

#endif

//extend errno
enum{
    ESOCKCLOSE = 140,           /*socket close*/
    EUNSPPLAT,                  /*unsupport platform*/
    ENOASSENG,                  /*no associate engine*/
    EINVISOKTYPE,               /*invaild socket type*/
    EASSENG,                    /*already associate*/
    EINVIPK,                    /*invaild packet type*/
    EACTCLOSE,                  /*active close*/
    ERDISPERROR,                /*redis reply error*/
    EALSETMBOX,                 /*already setup mailbox*/
    EMAXTHD,                    /*reach max thread*/
    EPIPECRERR,                 /*notifier create error*/
    EENASSERR,                  /*event add error*/
    ETMCLOSE,                   /*target mailbox close*/
};
    
#endif
