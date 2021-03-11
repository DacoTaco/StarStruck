SDKDIR = ../../sdk
include $(SDKDIR)/starlet.mk
## you should not be messing with anything above this point ! ##

ENTRYPOINT	= 0x13400000
SOURCES		= source
DATA 		= data 
INCLUDES	= include
LIBS 		= -lcore -lgcc 
OUTPUT		= $(notdir $(CURDIR))
TARGET 		= $(OUTPUT)-syms.elf
TARGET_ELF	= $(OUTPUT).elf
TARGET_BIN	= $(OUTPUT).bin
CFLAGS 		+= -DCAN_HAZ_USBGECKO

## you should not be messing with anything below this point ! ##
include $(SDKDIR)/module.mk

all: $(TARGET) $(TARGET_ELF) $(TARGET_BIN)

$(TARGET_ELF): $(TARGET)
	@echo "  STRIP     $@"
	@$(STRIP) $< -o $@
	
$(TARGET_BIN): $(TARGET)
	@echo "  LINK      $@"
	@$(OBJCOPY) -O binary $< $@
	
clean: myclean

myclean:
	@echo $(OBJS)
	@echo $(SOURCES)
	@echo $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
	@rm -f $(TARGET_BIN)
	@rm -f $(TARGET_ELF)