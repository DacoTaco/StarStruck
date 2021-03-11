SDKDIR = ../sdk
include $(SDKDIR)/starlet.mk

SOURCES		= source
DATA 		= data 
INCLUDES	= include

ASFLAGS += -D_LANGUAGE_ASSEMBLY
CFLAGS += -DCAN_HAZ_USBGECKO -fpie
LIBS = -lgcc
LDSCRIPT = 
LIBRARY = libcore.a

include $(SDKDIR)/common.mk

all: $(LIBRARY)

$(LIBRARY): ./include/git_version.h $(OBJS)
	@echo "  BUILDING  $@"
	@$(AR) -rc $@ $(OBJS)
	@$(RANLIB) $@

./include/git_version.h: describesimple.sh
	@echo "  GITVER    $@"
	@echo 'const char git_version[] = "'`./describesimple.sh`'";' > ./include/git_version.h
	
clean: myclean

myclean:
	@rm -f ./include/git_version.h
	@rm -f $(LIBRARY)