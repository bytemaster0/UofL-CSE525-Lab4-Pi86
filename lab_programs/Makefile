# =============================================================================
#  Makefile for pi86 Lab Programs
#  Requires: nasm  (sudo apt install nasm)
#
#  Usage:
#    make          -- build all COM files
#    make clean    -- remove all output files
#    make install  -- copy COM files onto the pi86 floppy disk image
#
#  Each .asm file is assembled into a DOS COM file with nasm.
#  The -f bin flag produces a flat binary starting at ORG 0x100.
# =============================================================================

NASM    := nasm
NFLAGS  := -f bin

# Floppy image path (adjust if your pi86 setup uses a different path)
FLOPPY  := ../code/v20/floppy.img
# Sector offset within the floppy to inject programs (offset 0 = boot sector)
# For a formatted FAT12 floppy, copy via mtools or mcopy instead:
MCOPY   := mcopy -i $(FLOPPY)

PROGRAMS := \
    prog1_memcopy.com \
    prog2_io_toggle.com \
    prog3_waitstates.com \
    prog4_memdecode.com \
    prog5_isr.com

.PHONY: all clean install

all: $(PROGRAMS)

# Pattern rule: assemble any .asm -> .com
%.com: %.asm
	$(NASM) $(NFLAGS) -o $@ $<
	@echo "Built: $@  ($(shell wc -c < $@) bytes)"

# --------------------------------------------------------------------------
# Firmware convenience targets
# --------------------------------------------------------------------------
.PHONY: firmware

FIRMWARE_DIR := ../firmware
FIRMWARE_SRC := $(FIRMWARE_DIR)/x86.cpp \
                $(FIRMWARE_DIR)/buslog.cpp
FIRMWARE_HDR := $(FIRMWARE_DIR)/x86.h \
                $(FIRMWARE_DIR)/buslog.h

firmware:
	@echo "Firmware source files are in $(FIRMWARE_DIR)/"
	@echo "To rebuild pi86 with the augmented firmware, copy the files"
	@echo "in $(FIRMWARE_DIR)/ over the originals in pi86/code/v20/ and"
	@echo "then run:  make  in that directory."

# --------------------------------------------------------------------------
# Install COM files onto the floppy image using mtools
# Requires:  sudo apt install mtools
# --------------------------------------------------------------------------
install: all
	@echo "Installing programs onto $(FLOPPY) ..."
	@for f in $(PROGRAMS); do \
	    $(MCOPY) $$f ::$$f && echo "  Installed $$f"; \
	done
	@echo "Done. Boot pi86 and run the programs from the DOS prompt."

clean:
	rm -f $(PROGRAMS)
	@echo "Cleaned."
