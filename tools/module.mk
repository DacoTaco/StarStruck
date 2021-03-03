
include $(TOOLSDIR)/common.mk
CFLAGS 		+= -fno-asynchronous-unwind-tables -fno-builtin -fno-ident

ifneq ($(ENTRYPOINT),)
CFLAGS += -Wl,--section-start,.init=$(ENTRYPOINT)
endif