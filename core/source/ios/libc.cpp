#include <malloc.h>
#include "ios/syscalls.h"

void __malloc_lock (struct _reent *reent) { throw "malloc not supported"; }
void __malloc_unlock (struct _reent *reent) { throw "malloc not supported"; }

int _close_r(struct _reent *ptr, int fd){return -1;}
int _execve_r(struct _reent *ptr, const char *name,char *const argv[], char *const env[]) {return -1;}
int __getreent(){return 0;}

void *_sbrk_r(struct _reent *ptr, void* incr) { return (void*)-1;}
int _getpid_r(struct _reent *ptr) { return OSGetProcessId(); }