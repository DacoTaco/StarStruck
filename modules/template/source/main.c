#include <ios_module.h>
#include <string.h>
#include <geckocore.h>

int request_handler(ipcreq* request, unsigned char* do_reply) 
{
	return 0xDEADCAFE;
}

int request_open(char* filepath, u32 mode, unsigned char* do_reply)
{
	return 0xBEEFCAFE;
}

//this is our 'main' function. it gets called when the module gets loaded and should :
// * do any module specific actions on startup
// * return a value which the kernel will report back on
int load(ios_module* output)
{
	gecko_init();
	
	if(output == NULL)
		return IOS_EINVAL;
	
	gecko_printf("hello from inside the module %s!\n", output->device_name);
	
	return 0xCAFECAFE;
}





