#ifndef __COMMON_H__
#define __COMMON_H__ 1

#include <config.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>
#include <stdarg.h>
#include <signal.h>
#include <limits.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdbool.h>
#include <syslog.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <netdb.h>
#include <poll.h>
#include <dlfcn.h>
#include <spawn.h>

#define VERSION_MAJOR 1

#ifndef __GNUC__
# ifdef __attribute__
#  undef __attribute__
# endif
# define __attribute__(a)
#endif

#ifndef environ
# ifdef __APPLE__
#  include <crt_externs.h>
#  define environ (*_NSGetEnviron())
# else
extern char **environ;
# endif
#endif

#if defined(__APPLE__)
# define USE_INTERPOSERS 1
#endif

#ifdef USE_INTERPOSERS
# define INTERPOSE(F) sixjack_interposed_ ## F
#else
# define INTERPOSE(F) F
#endif

#if defined(__SUNPRO_C)
# define DLL_LOCAL  __attribute__ __hidden
# define DLL_PUBLIC __attribute__ __global
#elif defined(_MSG_VER)
# define DLL_LOCAL
# define DLL_PUBLIC extern __declspec(dllexport)
#else
# define DLL_LOCAL  __attribute__ ((visibility ("hidden")))
# define DLL_PUBLIC __attribute__ ((visibility ("default")))
#endif

#ifdef DEFINE_HOOK_GLOBALS
# define HOOK_GLOBAL
# ifndef DONT_BYPASS_HOOKS
#  error HOOK_GLOBAL cannot be defined without DONT_BYPASS_HOOKS
# endif
#else
# define HOOK_GLOBAL extern
#endif

#define STORAGE_PORT(X)  (*storage_port(&(X)))
#define STORAGE_PORT6(X) (*storage_port6(&(X)))
#define STORAGE_SIN_ADDR(X) (storage_sin_addr(&(X))->s_addr)
#define STORAGE_SIN_ADDR6(X) (storage_sin_addr6(&(X))->s6_addr)
#define STORAGE_SIN_ADDR6_NF(X) (*(storage_sin_addr6(&(X))))

#ifdef HAVE_SS_LEN
# define STORAGE_LEN(X) ((X).ss_len)
# define SET_STORAGE_LEN(X, Y) do { STORAGE_LEN(X) = (Y); } while(0)
#elif defined(HAVE___SS_LEN)
# define STORAGE_LEN(X) ((X).__ss_len)
# define SET_STORAGE_LEN(X, Y) do { STORAGE_LEN(X) = (Y); } while(0)
#else
# define STORAGE_LEN(X) (STORAGE_FAMILY(X) == AF_INET ? sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6))
# define SET_STORAGE_LEN(X, Y) (void) 0
#endif

#ifdef HAVE___SS_FAMILY
# define STORAGE_FAMILY(X) ((X).__ss_family)
#else
# define STORAGE_FAMILY(X) ((X).ss_family)
#endif

#ifndef SOL_IP
# define SOL_IP IPPROTO_IP
#endif
#ifndef SOL_TCP
# define SOL_TCP IPPROTO_TCP
#endif

#ifndef INADDR_NONE
# define INADDR_NONE 0
#endif

#if !defined(O_NDELAY) && defined(O_NONBLOCK)
# define O_NDELAY O_NONBLOCK
#endif

#ifndef FNDELAY
# define FNDELAY O_NDELAY
#endif

typedef struct AppContext_ {
    bool initialized;
    int log_fd;
    size_t pid;
    struct Filter_ *filter;
} AppContext;

AppContext *sixjack_get_context(void);
void sixjack_free_context(void);

#include "hooks.h"

#endif
