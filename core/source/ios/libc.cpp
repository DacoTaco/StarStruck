#include <stdio.h>
#include "ios/syscalls.h"

extern "C" {

[[noreturn]]
void __syscall_exit(int code)
{
	OSStopThread(0, code);
}

int __syscall_stdout_putc(char c, FILE *file)
{
	OSPrintk(&c);
	return c;
}

int _getpid_r(struct _reent *ptr)
{
	return OSGetProcessId();
}
}