AR = $(PREFIX)ar
AS = $(PREFIX)as
CC = $(PREFIX)gcc
CXX = $(PREFIX)g++
LD = $(PREFIX)ld
OBJCOPY = $(PREFIX)objcopy
RANLIB = $(PREFIX)ranlib
STRIP = $(PREFIX)strip
OBJDIR = ./build/
INCLUDES += $(OBJDIR)
CFLAGS += $(foreach dir,$(INCLUDES), -iquote $(CURDIR)/$(dir))
CFLAGS += $(foreach dir,$(LIBSDIR), -I$(dir)/include) 
CXXFLAGS = $(CFLAGS)


CFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
CPPFILES	:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))
sFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.s)))
SFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.S)))
BINFILES	:=	$(foreach dir,$(DATA),$(notdir $(wildcard $(dir)/*.*)))

VPATH		:=	$(foreach dir,$(SOURCES),$(CURDIR)/$(dir)) \
				$(foreach dir,$(DATA),$(CURDIR)/$(dir))
					

OBJS = 	$(addprefix $(OBJDIR), $(addsuffix .o,$(BINFILES))) \
		$(addprefix $(OBJDIR), $(CPPFILES:.cpp=.o) $(CFILES:.c=.o)) \
		$(addprefix $(OBJDIR), $(sFILES:.s=.o) $(SFILES:.S=.o))

BIN2S = $(DEVKITARM)/bin/bin2s

ifeq ($(NOMAPFILE),)
LDFLAGS += -Wl,-Map,$(TARGET).map
endif

ifneq ($(LDSCRIPT),)
LDFLAGS += -Wl,-T$(LDSCRIPT)
endif

.PHONY: clean prebuild
all: prebuild $(TARGET)

clean:
	rm -rf $(OBJDIR)
	rm -f $(TARGET) $(TARGET).map
	
$(TARGET): $(OBJS)
	@echo "  LINK      $@"
	@$(CC) $(CFLAGS) $(LDFLAGS) $(OBJS) -o $@ $(foreach dir,$(LIBSDIR), -L$(dir)) $(LIBS)

ifneq ($(LDSCRIPT),)
$(TARGET): $(LDSCRIPT)
endif

$(OBJDIR)%.o: %.cpp
	@echo "  COMPILE   $(notdir $<)"
	@mkdir -p $(OBJDIR)
	@$(CC) $(CXXFLAGS) $(DEFINES) -Wp,-MMD,$(OBJDIR)$(*F).d,-MQ,"$@",-MP -c $< -o $@
	
$(OBJDIR)%.o: %.c
	@echo "  COMPILE   $(notdir $<)"
	@mkdir -p $(OBJDIR)
	@$(CC) $(CFLAGS) $(DEFINES) -Wp,-MMD,$(OBJDIR)$(*F).d,-MQ,"$@",-MP -c $< -o $@

$(OBJDIR)%.o: %.s
	@echo "  ASSEMBLE  $(notdir $<)"
	@$(CC) $(CFLAGS) $(DEFINES) $(ASFLAGS) -c $< -o $@

$(OBJDIR)%.o: %.S
	@echo "  ASSEMBLE  $(notdir $<)"
	@$(CC) $(CFLAGS) $(DEFINES) $(ASFLAGS) -c $< -o $@

define bin2o
	@echo "  BIN2S     $(notdir $<)"
	@mkdir -p $(OBJDIR)
	@$(BIN2S) -a 32 $< | $(AS) -mbig-endian -o $(@)
	@echo "extern const u8" `(echo $(<F).o | sed -e 's/^\([0-9]\)/_\1/' | tr . _)`"_end[];" > $(OBJDIR)`(echo $(<F) | tr . _)`.h
	@echo "extern const u8" `(echo $(<F) | sed -e 's/^\([0-9]\)/_\1/' | tr . _)`"[];" >> $(OBJDIR)`(echo $(<F) | tr . _)`.h
	@echo "extern const u32" `(echo $(<F) | sed -e 's/^\([0-9]\)/_\1/' | tr . _)`_size";" >> $(OBJDIR)`(echo $(<F) | tr . _)`.h
endef

$(OBJDIR)%.bin.o: %.bin
	@$(bin2o)

-include $(OBJDIR)*.d

