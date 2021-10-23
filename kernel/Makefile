#---------------------------------------------------------------------------------
.SUFFIXES:
#---------------------------------------------------------------------------------
ifeq ($(SDKDIR),)
SDKDIR = ../sdk
endif

include $(SDKDIR)/starstruck.mk

#---------------------------------------------------------------------------------
# TARGET is the name of the output
# BUILD is the directory where object files & intermediate files will be placed
# SOURCES is a list of directories containing source code
# INCLUDES is a list of directories containing extra header files
# DATA is a list of directories containing binary data
#
# All directories are specified relative to the project directory where
# the makefile is found
#
#---------------------------------------------------------------------------------
TARGET			:= armboot
LDSCRIPT 		= tools/kernel.ld
MAKEBIN 		= tools/makebin.py
BUILD			:= build
SOURCES			:= source $(wildcard source/*/)
INCLUDES		:= include
DATA			:=

#---------------------------------------------------------------------------------
# options for code generation
#---------------------------------------------------------------------------------

CFLAGS += -DCAN_HAZ_USBGECKO
# disables debug spew over usbgecko
#CFLAGS += -DNDEBUG
# enables LFCR linefeeds for usbgecko output, useful for some terminal apps
#CFLAGS += -DGECKO_LFCR
# uses the 'safe' version of the usbgecko receive and send functions
#CFLAGS += -DGECKO_SAFE

CFLAGS	+= $(INCLUDE)
ASFLAGS += $(CFLAGS)

ifeq ($(NOMAPFILE),)
LDFLAGS += -Wl,-Map,$(notdir $@).map
endif

ifneq ($(LDSCRIPT),)
LDFLAGS += -Wl,-T../$(LDSCRIPT)
endif

#---------------------------------------------------------------------------------
# any extra libraries we wish to link with the project
#---------------------------------------------------------------------------------
LIBS	:= -lcore -lgcc 

#---------------------------------------------------------------------------------
# list of directories containing libraries, this must be the top level containing
# include and lib
#---------------------------------------------------------------------------------
LIBDIRS	:=	../../$(COREDIR)

#---------------------------------------------------------------------------------
# no real need to edit anything past this point unless you need to add additional
# rules for different file extensions
#---------------------------------------------------------------------------------

ifneq ($(BUILD),$(notdir $(CURDIR)))
#---------------------------------------------------------------------------------

export ELFLOADER        :=  $(CURDIR)/../elfloader/elfloader.bin
export OUTPUT			:=	$(CURDIR)/$(TARGET)-sym.elf
export OUTPUT_STRIPPED	:=	$(CURDIR)/$(TARGET).elf
export OUTPUT_BIN		:=	$(CURDIR)/$(TARGET).bin
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

export OFILES_BIN := $(addsuffix .o,$(BINFILES))

export OFILES_SOURCES := $(CPPFILES:.cpp=.o) $(CFILES:.c=.o) $(sFILES:.s=.o) $(SFILES:.S=.o)

export OFILES := $(OFILES_BIN) $(OFILES_SOURCES)

export HFILES := $(addsuffix .h,$(subst .,_,$(BINFILES)))

export INCLUDE	:=	$(foreach dir,$(INCLUDES),-iquote $(CURDIR)/$(dir)) \
					$(foreach dir,$(LIBDIRS),-I$(dir)/include) \
					-I$(CURDIR)/$(BUILD)

export LIBPATHS	:=	$(foreach dir,$(LIBDIRS),-L$(dir)/lib)

.PHONY: $(BUILD) clean

#---------------------------------------------------------------------------------

$(BUILD):
ifeq ($(wildcard $(ELFLOADER)),)
	@echo "elfloader is missing (elfloader.bin)."
	@echo "please run 'make clean' followed by 'make' in the root directory to rebuild"
	@echo
	@exit 1
endif
	@[ -d $@ ] || mkdir -p $@
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile SDKDIR=../$(SDKDIR)

#---------------------------------------------------------------------------------
clean:
	$(SILENTMSG) clean ...
	$(SILENTCMD)rm -fr $(BUILD) $(OUTPUT) $(OUTPUT_STRIPPED) $(OUTPUT_BIN)


#---------------------------------------------------------------------------------
else

#---------------------------------------------------------------------------------
# main targets
#---------------------------------------------------------------------------------

$(OUTPUT_BIN): $(OUTPUT_STRIPPED)
	$(SILENTMSG)  "MAKEBIN	$@"
	$(SILENTCMD)../$(MAKEBIN) $(ELFLOADER) $< $@

$(OUTPUT_STRIPPED): $(OUTPUT)
	$(SILENTMSG)  "STRIP	$@"
	$(SILENTCMD)$(STRIP) $< -o $@
	
$(OUTPUT)	:	$(OFILES)

$(OFILES_SOURCES) : $(HFILES)

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


