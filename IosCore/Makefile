TOOLSDIR = ../tools
include $(TOOLSDIR)/starlet.mk

SOURCES		= source
DATA 		= data 
INCLUDES	= include

ASFLAGS += -D_LANGUAGE_ASSEMBLY
CFLAGS += -DCAN_HAZ_IRQ -DCAN_HAZ_IPC -DCAN_HAZ_USBGECKO -fpie
LIBS = -lgcc
LDSCRIPT = 
TARGET = libIosCore.a

include $(TOOLSDIR)/common.mk

all: $(TARGET)

$(TARGET): ./include/git_version.h $(OBJS)
	@echo "  BUILDING  $@"
	@ar -rc $@ $(OBJS)
	@$(RANLIB) $@

./include/git_version.h: describesimple.sh
	@echo "  GITVER    $@"
	@echo 'const char git_version[] = "'`./describesimple.sh`'";' > ./include/git_version.h
	
clean: myclean

myclean:
	@rm -f ./include/git_version.h