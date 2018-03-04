#ifdef _CORE_


typedef struct _idle {
	chk_timer *idle_timer;
	uint64_t   fire_tick;

#ifdef CHUCK_LUA
	chk_luaRef  on_idle;	
#else	
	void      (*on_idle)();
#endif

}_idle;


#define _chk_loop				 	 \
	 chk_timermgr  *timermgr;        \
     int32_t        notifyfds[2];    \
     chk_dlist      handles;         \
     chk_list       closures;        \
     int32_t        status;          \
     pid_t          threadid;		 \
     _idle          idle;         

#ifdef _LINUX
	struct chk_event_loop {
		_chk_loop;
		int32_t    tfd;
		int32_t    epfd;
		struct     epoll_event* events;
		int32_t    maxevents;
	};
#elif _MACH

	struct chk_event_loop{
		_chk_loop;
		//int    tfd;		
		int32_t kfd;
		struct kevent* events;
		int    maxevents;
		//for timer
	   	//struct kevent change;	
	};

#else
#	error "un support platform!"		
#endif

#endif