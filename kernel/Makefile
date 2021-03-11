SDKDIR = ../sdk
include $(SDKDIR)/starlet.mk
LDSCRIPT = tools/kernel.ld

SOURCES			= source
DATA 			= data 
INCLUDES		= include
ELFLOADER 		= elfloader/elfloader.bin
MAKEBIN 		= tools/makebin.py
TARGET 			= armboot-sym.elf
TARGET_STRIPPED = armboot.elf
TARGET_BIN 		= armboot.bin
LIBS 			= -lcore -lgcc 

#Compilation Flags
ASFLAGS += -D_LANGUAGE_ASSEMBLY

# enabled usbgecko support
CFLAGS += -DCAN_HAZ_USBGECKO
# disables debug spew over usbgecko
#CFLAGS += -DNDEBUG
# enables LFCR linefeeds for usbgecko output, useful for some terminal apps
#CFLAGS += -DGECKO_LFCR
# uses the 'safe' version of the usbgecko receive and send functions
#CFLAGS += -DGECKO_SAFE

#PrepareCompilation
include $(SDKDIR)/common.mk

#Compîle
all: $(TARGET_BIN)

$(TARGET_STRIPPED): $(TARGET)
	@echo  "STRIP	$@"
	@$(STRIP) $< -o $@

$(TARGET_BIN): $(TARGET_STRIPPED) $(ELFLOADER)
	@echo  "MAKEBIN	$@"
	@$(MAKEBIN) $(ELFLOADER) $< $@

clean: myclean

$(ELFLOADER):
	@$(MAKE) -C elfloader
	
myclean:
	-rm -f $(TARGET) $(TARGET_STRIPPED) $(TARGET_BIN)
	@$(MAKE) -C elfloader clean

