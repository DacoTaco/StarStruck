SDKDIR = ../../sdk
include $(SDKDIR)/starlet.mk

CFLAGS += -fpic
LDSCRIPT = stub.ld
LIBS =  -lcore -lgcc

TARGET 		= elfloader.elf
TARGET_BIN 	= elfloader.bin
SOURCES		= source
DATA 		= data 
INCLUDES	= include

include $(SDKDIR)/common.mk

all: $(TARGET_BIN)

$(TARGET_BIN): $(TARGET)
	@echo  "  OBJCPY    $@"
	@$(OBJCOPY) -O binary $< $@

clean: myclean

myclean:
	-rm -f $(TARGET_BIN)

