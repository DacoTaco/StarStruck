TOOLSDIR = ../../tools
include $(TOOLSDIR)/starlet.mk
## you should not be messing with anything above this point ! ##

ENTRYPOINT	= 0x13400000
SOURCES		= source
DATA 		= data 
INCLUDES	= include
LIBS 		= -lIosCore -lgcc 
OUTPUT		= $(notdir $(CURDIR))
TARGET 		= $(OUTPUT)-syms.elf
TARGET_ELF	= $(OUTPUT).elf
TARGET_BIN	= $(OUTPUT).bin
CFLAGS 		+= -DCAN_HAZ_USBGECKO
LDSCRIPT 	= $(TOOLSDIR)/module.ld

## you should not be messing with anything below this point ! ##
include $(TOOLSDIR)/module.mk

all: $(TARGET) $(TARGET_ELF) $(TARGET_BIN)

$(TARGET_ELF): $(TARGET)
	@echo "  STRIP     $@"
	@$(STRIP) $< -o $@
	
$(TARGET_BIN): $(TARGET)
	@echo "  LINK      $@"
	@$(OBJCOPY) -O binary $< $@
	
clean: myclean

myclean:
	@rm -f $(TARGET_BIN)
	@rm -f $(TARGET_ELF)