/*	this --MUST-- be the first code that is in the binary output
	we insure this is the case by defining the code to go into the .text.startup section, placing it in front of all the rest.
	when run, we will jump to _load instantly*/

#include <ios_module.h>

#define DEVICE_NAME "/dev/"PROJECT_NAME

asm(".section	\".crt0\",\"ax\"");

extern request_handler(ipcreq* request, unsigned char* do_reply);
extern request_open(char* filepath, u32 mode, unsigned char* do_reply);
extern int load(ios_module* output);

int _start(ios_module* output)
{
	if(output == NULL)
		return IOS_EINVAL;
	
	output->device_name = DEVICE_NAME;
	output->request_handler = &request_handler;
	output->open_handler = &request_open;
	
	return load(output);
}
