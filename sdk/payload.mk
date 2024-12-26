#---------------------------------------------------------------------------------
# options for code generation
#---------------------------------------------------------------------------------
CFLAGS	+= $(INCLUDE) -fno-asynchronous-unwind-tables -fno-builtin
CXXFLAGS = $(CFLAGS)
ASFLAGS += $(CFLAGS)
ifeq ($(BUILD),)
BUILD	:= build
endif

ifeq ($(NOMAPFILE),)
LDFLAGS += -Wl,-Map,$(notdir $@).map
endif

#---------------------------------------------------------------------------------
# any extra libraries we wish to link with the project
#---------------------------------------------------------------------------------
LIBDIRS	:=

#---------------------------------------------------------------------------------
# no real need to edit anything past this point unless you need to add additional
# rules for different file extensions
#---------------------------------------------------------------------------------

ifneq ($(BUILD),$(notdir $(CURDIR)))
#---------------------------------------------------------------------------------

export TARGET			:=	$(notdir $(CURDIR))
export OUTPUT			:=	$(CURDIR)/$(TARGET)
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

export OFILES 			:= $(addsuffix .o,$(BINFILES)) $(CPPFILES:.cpp=.o) $(CFILES:.c=.o) $(sFILES:.s=.o) $(SFILES:.S=.o)
export LIBPATHS			:=	$(foreach dir,$(LIBDIRS),-L$(dir)/lib)
export INCLUDE			:=	$(foreach dir,$(INCLUDES),-iquote $(CURDIR)/$(dir)) \
							$(foreach dir,$(LIBDIRS),-I$(dir)/include) \
							-I$(CURDIR)/$(BUILD)

.PHONY: $(BUILD) clean
#---------------------------------------------------------------------------------
all: $(BUILD)
$(BUILD):
	@[ -d $@ ] || mkdir -p $@
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile SDKDIR=../$(SDKDIR)

#---------------------------------------------------------------------------------
clean:
	$(SILENTMSG) clean ...
	$(SILENTCMD)rm -fr $(BUILD) $(OUTPUT).elf $(OUTPUT).bin


#---------------------------------------------------------------------------------
else

#---------------------------------------------------------------------------------
# main targets
#---------------------------------------------------------------------------------
	
$(OUTPUT).bin:	$(OUTPUT).elf
	$(SILENTMSG) chanting $(OUTPUT) payload
	$(SILENTCMD)$(OBJCOPY) -O binary $^ $@
$(OUTPUT).elf: $(OFILES)

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
