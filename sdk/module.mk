
#include the common that does most of the work
include $(SDKDIR)/starlet.mk

#Add the modules sources to the list
SOURCES += $(SDKDIR)/modules/source

#add module related stuff
MODULE_NAME	:= $(shell echo $(MODULE_NAME) | tr '[:upper:]' '[:lower:]')
CFLAGS 		+= -fno-asynchronous-unwind-tables -fno-builtin -fno-ident -DMODULE_NAME=\"$(MODULE_NAME)\"


ifneq ($(ENTRYPOINT),)
CFLAGS += -Wl,--section-start,.init=$(ENTRYPOINT)
endif

LDSCRIPT 	= $(SDKDIR)/modules/module.ld