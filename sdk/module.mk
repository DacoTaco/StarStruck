
#Add the modules sources to the list
SOURCES += $(SDKDIR)/modules/source

#include the common that does most of the work
include $(SDKDIR)/common.mk

#add module related stuff
MODULE_NAME	= $(shell echo $(OUTPUT) | tr '[:upper:]' '[:lower:]')
CFLAGS 		+= -fno-asynchronous-unwind-tables -fno-builtin -fno-ident -DMODULE_NAME=\"$(MODULE_NAME)\"

ifneq ($(ENTRYPOINT),)
CFLAGS += -Wl,--section-start,.init=$(ENTRYPOINT)
endif

LDSCRIPT 	= $(SDKDIR)/modules/module.ld