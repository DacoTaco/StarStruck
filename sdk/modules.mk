#---------------------------------------------------------------------------------
# options for code generation
#---------------------------------------------------------------------------------

CFLAGS	+= -DCAN_HAZ_USBGECKO -D__PRIORITY=$(PRIORITY)
CFLAGS	+= $(INCLUDE)
ASFLAGS	+= $(CFLAGS)
SOURCES	+= $(SDKDIR)/modules/

ifeq ($(BUILD),)
BUILD	:= build
endif

ifeq ($(NOMAPFILE),)
LDFLAGS += -Wl,-Map,$(notdir $@).map
endif

LDFLAGS += -Wl,-T$(TARGET).ld -Wl,--section-start,.module=$(VIRTUALADDR)

#---------------------------------------------------------------------------------
# any extra libraries we wish to link with the project
#---------------------------------------------------------------------------------
LIBS	:= -lcore -lgcc 

#---------------------------------------------------------------------------------
# list of directories containing libraries, this must be the top level containing
# include and lib
#---------------------------------------------------------------------------------
LIBDIRS	:=	../../../$(COREDIR) $(SDKDIR)

#---------------------------------------------------------------------------------
# no real need to edit anything past this point unless you need to add additional
# rules for different file extensions
#---------------------------------------------------------------------------------

ifneq ($(BUILD),$(notdir $(CURDIR)))
#---------------------------------------------------------------------------------
export TARGET			:=	$(notdir $(CURDIR))
export OUTPUT			:=	$(CURDIR)/$(TARGET)-sym.elf
export OUTPUT_STRIPPED	:=	$(CURDIR)/$(TARGET).elf
export VPATH			:=	$(foreach dir,$(SOURCES),$(CURDIR)/$(dir)) \
							$(foreach dir,$(DATA),$(CURDIR)/$(dir))

export DEPSDIR	:=	$(CURDIR)/$(BUILD)

CFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
CPPFILES	:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))
sFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.s)))
SFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.S)))
BINFILES	:=	$(foreach dir,$(DATA),$(notdir $(wildcard $(dir)/*.*)))

#---------------------------------------------------------------------------------
# use CXX for linking C++ projects, CC for standard C
#---------------------------------------------------------------------------------
ifeq ($(strip $(CPPFILES)),)
#---------------------------------------------------------------------------------
	export LD	:=	$(CC)
#---------------------------------------------------------------------------------
else
#---------------------------------------------------------------------------------
	export LD	:=	$(CXX)
#---------------------------------------------------------------------------------
endif
#---------------------------------------------------------------------------------

export OFILES_BIN 		:= $(addsuffix .o,$(BINFILES))
export OFILES_SOURCES 	:= $(CPPFILES:.cpp=.o) $(CFILES:.c=.o) $(sFILES:.s=.o) $(SFILES:.S=.o)
export OFILES 			:= $(OFILES_BIN) $(OFILES_SOURCES)
export HFILES 			:= $(addsuffix .h,$(subst .,_,$(BINFILES)))
export INCLUDE			:= $(foreach dir,$(INCLUDES),-iquote $(CURDIR)/$(dir)) \
						   $(foreach dir,$(LIBDIRS),-I$(dir)/include) \
						   -I$(CURDIR)/$(BUILD) -I$(SDKDIR)/modules
export LIBPATHS			:= $(foreach dir,$(LIBDIRS),-L$(dir)/lib)

.PHONY: $(BUILD) clean $(TARGET)

#---------------------------------------------------------------------------------

all: $(BUILD)
$(BUILD):
	@[ -d $@ ] || mkdir -p $@
	@$(MAKE) SDKDIR=../$(SDKDIR) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

$(TARGET): $(OUTPUTPATH)/$(TARGET)_module.ld
$(OUTPUTPATH)/$(TARGET)_module.ld: $(OUTPUT)
	@if [ -z $(OUTPUTPATH) ]; then\
		echo "OUTPUTPATH is a required variable to build module binary data"; \
		false; \
    fi
	$(SILENTCMD)$(OBJCOPY) -I elf32-big --dump-section .note=$(OUTPUTPATH)/$(TARGET)_notes.bin $(OUTPUT_STRIPPED)
	$(SILENTCMD)$(OBJCOPY) -I elf32-big --dump-section .module=$(OUTPUTPATH)/$(TARGET)_module.bin $(OUTPUT_STRIPPED)
	$(SILENTCMD)$(OBJCOPY) -I elf32-big --dump-section .module.data=$(OUTPUTPATH)/$(TARGET)_moduleData.bin $(OUTPUT_STRIPPED)
	@cat $(SDKDIR)/modules/embeddedModuleTemplate.ld | \
		sed 's/__PHYSADDR__/$(PHYSADDR)/g' | \
		sed 's/__VIRTADDR__/$(VIRTUALADDR)/g' | \
		sed 's/__PROCESSID__/$(PROCESSID)/g' | \
		sed 's/__BSS_SIZE__/$(shell printf "%d" $(shell readelf -l $(OUTPUT_STRIPPED) | grep LOAD | tail -1 | tr -s ' ' | cut -d ' ' -f 7))/g' | \
		sed 's/__ModuleName__/$(TARGET)/g' > $(OUTPUTPATH)/$(TARGET)_module.ld
#---------------------------------------------------------------------------------
clean:
	$(SILENTMSG) clean ...
	$(SILENTCMD)rm -fr $(BUILD) $(OUTPUT) $(OUTPUT_STRIPPED)

#---------------------------------------------------------------------------------
else

#---------------------------------------------------------------------------------
# main targets
#---------------------------------------------------------------------------------

$(OUTPUT_STRIPPED): $(OUTPUT)
	$(SILENTMSG)  "STRIP	$@"
	$(SILENTCMD)$(STRIP) $< -o $@
	
$(OUTPUT)	:	$(OFILES) $(TARGET).ld

$(OFILES_SOURCES) : $(HFILES)

$(TARGET).ld:
	@cat $(SDKDIR)/modules/moduleTemplate.ld | \
		sed 's/__PHYSADDR__/$(PHYSADDR)/g' | \
		sed 's/__PRIORITY__/$(PRIORITY)/g' | \
		sed 's/__PROCESSID__/$(PROCESSID)/g' | \
		sed 's/__STACKSIZE__/$(STACKSIZE)/g' > $@

#---------------------------------------------------------------------------------
# The bin2o rule should be copied and modified
# for each extension used in the data directories
#---------------------------------------------------------------------------------

#---------------------------------------------------------------------------------
# This rule links in binary data with the .bin extension
#---------------------------------------------------------------------------------
%.bin.o	%_bin.h :	%.bin
#---------------------------------------------------------------------------------
	@echo $(notdir $<)
	@$(bin2o)


-include $(DEPSDIR)/*.d
#---------------------------------------------------------------------------------------
endif
#---------------------------------------------------------------------------------------
