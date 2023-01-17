#
# Set this to the desired platform to build
#
# Useful values for general work
#
# amstradnc/nc100:	Amstrad NC100 (or emulator)
# amstradnc/nc200:	Amstrad NC200 (or emulator)
# coco2cart:	Tandy COCO2 or Dragon with 64K and IDE or SDC + cartridge flash
#		(or xroar emulator )
# coco3:	Tandy COCO3 512K (or MAME)
# cromemco:	Cromemco with banked memory
# dk-tm4c129x:	Texas Instruments Tiva C Series Development Board
# dragon-mooh:	Dragon 32/64 with Mooh 512K card (or xroar emulator)
# dragon-nx32:	Dragon 32/64 with Spinx 512K card (or xroar emulator)
# easy-z80:	Easy-Z80 RC2014 compatible system
# esp8266	ESP8266 module with added SD card
# msx1:		MSX1 as a cartridge
# msx2:		MSX2 with 128K or more and MegaFlashROM+SD interface
#		(or OpenMSX suitably configured)
# mtx:		Memotech MTX512 with SDX or SD (or MEMU emulator)
# multicomp09:	Extended multicomp 6809
# n8vem-mark4:	RBC/N8VEM Retrobrew Z180 board
# pentagon1024: Pentagon 1MB
# p112:		DX Designs P112
# rc2014:	RC2014 with 512K RAM/ROM and RTC
# rc2014-6502:	RC2014 with 65C02 or 65C816, VIA and 512K RAM/ROM
# rc2014-68008: RC2014 with 68008 CPU, PPIDE and flat 512/512K memory card
# rc2014-68hc11:RC2014 with a 68HC11 CPU card, SD and 512K RAM/ROM
# rc2014-8085:  RC2014 with an 80C85 CPU card, IDE and 512K RAM/ROM
# rc2014-sbc64: RC2014 Z80SBC64 128K system and RTC
# rc2014-tiny:	RC2014 with 64K RAM, banked ROM and RTC
# sam:		Sam Coupe
# sbcv2:	RBC/N8VEM SBC v2
# sc108:	Small Computer Central SC108 and SC114 systems
# sc111:	Small Computer Central SC111 system
# sc126:	Small Computer Central SC126 system
# scorpion:	Scorpion 256K (and some relatives) with NemoIDE
# scrumpel:	Scrumpel Z180 system
# searle:	Searle Z80 system with modified ROM and a timer added
# simple80:	Bill Shen's Simple80 with board bugfix and a timer
# socz80:	Will Sowerbutt's FPGA SocZ80 or extended version
# tc2068:	Timex TC2068/TS2068 with DivIDE/DivMMC disk interface
# tiny68k:	Bill Shen's Tiny68K or T68KRC
# tomssbc:	Tom's SBC running in RAM
# tomssbc-rom:	Tom's SBC using the 4x16K banked ROM for the kernel
# trs80:	TRS80 Model 4/4D/4P with 128K RAM (or some other mappers)
# trs80m1:	TRS80 Model I/III with suitable banker (also clones)
# ubee:		Microbee
# v8080:	8080 development using Z80Pack
# z80pack:	Z80Pack virtual Z80 platform
# zeta-v2:	Zeta v2 retrobrew SBC (for Zeta V1 see sbcv2)
# zx+3:		ZX Spectrum +3
# zxdiv:	ZX Spectrum 128K with DivIDE/DivMMC interface
#
# Virtual platforms for in progress development work
#
# v65c816:	Virtual platform for 65c816 development (flat memory)
# v68:		Virtual platform for 68000 development

TARGET=coco2cart

include version.mk

# Get the CPU type
include Kernel/platform-$(TARGET)/target.mk

ifeq ($(USERCPU),)
	USERCPU = $(CPU)
endif

# Base of the build directory
FUZIX_ROOT = $(shell pwd)

# FIXME: we should make it possible to do things entirely without /opt/fcc
PATH := /opt/fcc/bin:$(PATH)
# Add the tools directory
PATH := $(FUZIX_ROOT)/Build/tools/:$(PATH)

# Use Berkeley yacc always (Bison output is too large)
YACC = byacc

# TARGET is what we are building
# CPU is the CPU type for the kernel
# USERCPU is the CPU type for userspace and may be different
export TARGET CPU USERCPU PATH FUZIX_ROOT YACC

# FUZIX_CCOPTS is the global CC optimization level
ifeq ($(FUZIX_CCOPTS),)
	FUZIX_CCOPTS = -O2
endif
export FUZIX_CCOPTS

all: stand ltools libs apps kernel

stand:
	+(cd Standalone; $(MAKE))

ltools:
	+(cd Library; $(MAKE); $(MAKE) install)

libs: ltools
	+(cd Library/libs; $(MAKE) -f Makefile.$(USERCPU); \
		$(MAKE) -f Makefile.$(USERCPU) install)

apps: libs
	+(cd Applications; $(MAKE))

.PHONY: gtags
gtags:
	gtags

kernel: ltools
	mkdir -p Images/$(TARGET)
	+(cd Kernel; $(MAKE))

diskimage: stand ltools libs apps kernel
	mkdir -p Images/$(TARGET)
	+(cd Standalone/filesystem-src; ./build-filesystem $(ENDIANFLAG) $(FUZIX_ROOT)/Images/$(TARGET)/filesys.img 256 65535)
	+(cd Standalone/filesystem-src; ./build-filesystem $(ENDIANFLAG) $(FUZIX_ROOT)/Images/$(TARGET)/filesys8.img 256 16384)
	+(cd Kernel; $(MAKE) diskimage)

kclean:
	+(cd Kernel; $(MAKE) clean)

clean:
	rm -f GPATH GRTAGS GTAGS
	rm -f Images/$(TARGET)/*.img
	rm -f Images/$(TARGET)/*.DSK
	+(cd Standalone; $(MAKE) clean)
	+(cd Library/libs; $(MAKE) -f Makefile.$(USERCPU) clean)
	+(cd Library; $(MAKE) clean)
	+(cd Kernel; $(MAKE) clean)
	+(cd Applications; $(MAKE) clean)
