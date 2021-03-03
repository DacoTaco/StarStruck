TOOLSDIR = ../../tools
include $(TOOLSDIR)/starlet.mk

CFLAGS += -fpic
LDSCRIPT = stub.ld
LIBS =  -lIosCore -lgcc

TARGET 		= elfloader.elf
TARGET_BIN 	= elfloader.bin
SOURCES		= source
DATA 		= data 
INCLUDES	= include

include $(TOOLSDIR)/common.mk

all: $(TARGET_BIN)

$(TARGET_BIN): $(TARGET)
	@echo  "  OBJCPY    $@"
	@$(OBJCOPY) -O binary $< $@

clean: myclean

myclean:
	-rm -f $(TARGET_BIN)

