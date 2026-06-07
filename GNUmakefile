# NTux Package Manager (ntuxpkg) — Standalone Build
# ELF binary for NTux-OS
#
# Usage:
#   make              # build ntuxpkg.elf
#   make clean
#

CC  := cc
LD  := ld
ASM := as

CFLAGS := -std=gnu11 -O2 -g -Wall -Wextra \
          -ffreestanding -fno-stack-protector -fno-pie \
          -m64 -nostdlib -nostdinc
CPPFLAGS := -Iinclude -Isrc
LDFLAGS := -m elf_x86_64 -nostdlib -static -T linker.ld

APP_NAME ?= ntuxpkg
APP_BASE ?= 0x00400000

RUNTIME_C_SRC := $(wildcard libc/*.c)
RUNTIME_ASM_SRC := $(wildcard libc/*.asm)
RUNTIME_C_OBJ := $(patsubst libc/%.c,obj/libc/%.o,$(RUNTIME_C_SRC))
RUNTIME_ASM_OBJ := $(patsubst libc/%.asm,obj/libc/%.o,$(RUNTIME_ASM_SRC))
RUNTIME_OBJ := $(RUNTIME_C_OBJ) $(RUNTIME_ASM_OBJ)

APP_SRC := $(wildcard src/*.c)
APP_OBJ := $(patsubst src/%.c,obj/%.o,$(APP_SRC))

OBJ_DIR := obj
OUT_DIR := out

.PHONY: all clean

all: $(OUT_DIR)/$(APP_NAME).elf

$(OUT_DIR)/$(APP_NAME).elf: $(APP_OBJ) $(RUNTIME_OBJ) linker.ld
	@mkdir -p $(OUT_DIR)
	$(LD) $(LDFLAGS) --defsym=USER_BASE=$(APP_BASE) $(filter %.o,$^) -o $@
	@echo "  BUILT $(OUT_DIR)/$(APP_NAME).elf (base: 0x$$(printf '%X' $(APP_BASE)))"

obj/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

obj/libc/%.o: libc/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

obj/libc/%.o: libc/%.asm
	@mkdir -p $(dir $@)
	$(ASM) $< -o $@

clean:
	rm -rf $(OBJ_DIR) $(OUT_DIR)
