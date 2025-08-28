#---------------------------------------------------------------------------------
.SUFFIXES:
#---------------------------------------------------------------------------------
ifeq ($(SDKDIR),)
export SDKDIR = $(CURDIR)/../sdk
endif

include $(SDKDIR)/starstruck_rules

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
TARGET		:= libcore
INSTALLDIR	:= ../sdk
BUILD		:= build
SOURCES		:= source $(wildcard source/*/)
INCLUDES	:= include
LIBHEADERS	:= include
DATA		:=

#---------------------------------------------------------------------------------
# options for code generation
#---------------------------------------------------------------------------------
CFLAGS 	+= -fpie $(INCLUDE)
ASFLAGS += $(CFLAGS)

#---------------------------------------------------------------------------------
# any extra libraries we wish to link with the project
#---------------------------------------------------------------------------------
LIBS	:=

#---------------------------------------------------------------------------------
# list of directories containing libraries, this must be the top level containing
# include and lib
#---------------------------------------------------------------------------------
export LIBDIRS	:=

#---------------------------------------------------------------------------------
# no real need to edit anything past this point unless you need to add additional
# rules for different file extensions
#---------------------------------------------------------------------------------

ifneq ($(BUILD),$(notdir $(CURDIR)))
#---------------------------------------------------------------------------------

export OUTPUT		:=	$(CURDIR)/$(INSTALLDIR)/lib/$(TARGET)
export HEADEROUTPUT	:=	$(CURDIR)/$(INSTALLDIR)/include
export HEADERSPATH	:=	$(shell realpath $(LIBHEADERS))
export HEADERS 		:=	git_version.h $(foreach dir, $(LIBHEADERS), $(foreach file, $(shell find $(dir) -name '*.h'), $(subst $(dir)/,,$(file)))) \
						$(foreach dir, $(LIBHEADERS), $(foreach file, $(shell find $(dir) -name '*.hpp'), $(subst $(dir)/,,$(file)))) \

export VPATH		:=	$(foreach dir,$(SOURCES),$(CURDIR)/$(dir)) \
						$(foreach dir,$(DATA),$(CURDIR)/$(dir))

export DEPSDIR		:=	$(CURDIR)/$(BUILD)

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

export OFILES 	:= 	$(addsuffix .o,$(BINFILES)) $(CPPFILES:.cpp=.o) $(CFILES:.c=.o) $(sFILES:.s=.o) $(SFILES:.S=.o)
export INCLUDE	:=	$(foreach dir,$(INCLUDES),-iquote $(CURDIR)/$(dir)) \
					$(foreach dir,$(LIBDIRS),-I$(dir)/include) \
					-I$(CURDIR)/$(BUILD)
export LIBPATHS	:=	$(foreach dir,$(LIBDIRS),-L$(dir)/lib)
.PHONY: $(BUILD) clean

#---------------------------------------------------------------------------------
$(BUILD): ./include/git_version.h
	@[ -d $@ ] || mkdir -p $@
	$(SILENTCMD) mkdir -p $(dir $(OUTPUT)) $(HEADEROUTPUT)
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

./include/git_version.h: describesimple.sh
	$(SILENTCMD)echo "  GITVER    $@"
	$(SILENTCMD)echo 'const char git_version[] = "'`./describesimple.sh`'";' > ./include/git_version.h

#---------------------------------------------------------------------------------
clean:
	$(SILENTCMD)echo clean ...
	$(SILENTCMD)rm -fr $(BUILD) $(OUTPUT).a ./include/git_version.h $(addprefix $(HEADEROUTPUT)/,$(HEADERS))

#---------------------------------------------------------------------------------
else

# Copy headers function, this will copy the header, which is in $(1)
define CopyHeader =
$(HEADEROUTPUT)/$(1): $(HEADERSPATH)/$(1)
	$(SILENTMSG) copying $1...
	$(SILENTCMD) mkdir -p $(dir $(HEADEROUTPUT)/$(1))
	$(SILENTCMD)cp $$^ $$@
endef

#---------------------------------------------------------------------------------
# main targets
#---------------------------------------------------------------------------------

#$(addprefix $(HEADEROUTPUT)/,$(HEADERS))
all: $(OUTPUT).a $(addprefix $(HEADEROUTPUT)/,$(HEADERS))

# Use the function to create our targets
$(foreach file,$(HEADERS),$(eval $(call CopyHeader,$(file))))

$(OUTPUT).a	: $(OFILES)

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
