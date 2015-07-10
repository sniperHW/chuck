#include "daemon.h"
#include <sys/wait.h>

static char WORKDIR[256] = "./";

void daemonize()
{
	int	i, fd0, fd1, fd2;
	pid_t	pid;
	struct rlimit	rl;
	struct sigaction	sa;

	/*
	 * Clear file creation mask.
	 */
	umask(0);

	/*
	 * Get maximum number of file descriptors.
	 */
	if (getrlimit(RLIMIT_NOFILE, &rl) < 0)
		exit(0);
		//err_quit("%s: can't get file limit", cmd);

	/*
	 * Become a session leader to lose controlling TTY.
	 */
	if ((pid = fork()) < 0)
		exit(0);
		//err_quit("%s: can't fork", cmd);
	else if (pid != 0) /* parent */{
		wait(NULL);
		exit(0);
	}
	setsid();
	/*
	 * Ensure future opens won't allocate controlling TTYs.
	 */
	sa.sa_handler = SIG_IGN;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	if (sigaction(SIGHUP, &sa, NULL) < 0)
		exit(0);
	if ((pid = fork()) < 0)
		exit(0);
	else if (pid != 0) /* parent */
		exit(0);	
	if (chdir(WORKDIR) < 0){
		syslog(LOG_ERR, " can't change directory to %s\n",WORKDIR);
		exit(0);
	}
	/*
	 * Close all open file descriptors.
	 */
	if (rl.rlim_max == RLIM_INFINITY)
		rl.rlim_max = 1024;
	for (i = 0; i < rl.rlim_max; i++)
		close(i);
	/*
	 * Attach file descriptors 0, 1, and 2 to /dev/null.
	 */
	fd0 = open("/dev/null", O_RDWR);
	fd1 = dup(0);
	fd2 = dup(0);
	/*
	 * Initialize the log file.
	 */
	//openlog(cmd, LOG_CONS, LOG_DAEMON);
	if (fd0 != 0 || fd1 != 1 || fd2 != 2) {
		syslog(LOG_ERR, "unexpected file descriptors %d %d %d",fd0, fd1, fd2);
		exit(1);
	}
}

void set_workdir(const char *workdir){
	strncpy(WORKDIR,workdir,256);
}



#ifdef _CHUCKLUA

static int32_t lua_daemonize(lua_State *L)
{
	daemonize();
	return 0;
}

static int32_t lua_set_workdir(lua_State *L)
{
	if(lua_isstring(L,1))
		set_workdir(lua_tostring(L,1));
	return 0;
}


#define SET_FUNCTION(L,NAME,FUNC) do{\
	lua_pushstring(L,NAME);\
	lua_pushcfunction(L,FUNC);\
	lua_settable(L, -3);\
}while(0) 

void reg_luadaemon(lua_State *L)
{
    lua_newtable(L);
    SET_FUNCTION(L,"daemonize",lua_daemonize);
    SET_FUNCTION(L,"set_workdir",lua_set_workdir);
}

#endif