#---------------------------------------------------------------------------------
.SUFFIXES:
#---------------------------------------------------------------------------------
ifeq ($(SDKDIR),)
SDKDIR = ../../sdk
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
SOURCES			:= source $(wildcard source/*/)
INCLUDES		:= source
DATA			:=
PROCESSID		:= 0x02
PRIORITY		:= 0x58
VIRTUALADDR		:= 0x20000000
PHYSADDR		:= 0x13A10000
STACKSIZE		:= 0x800

include $(SDKDIR)/modules.mk