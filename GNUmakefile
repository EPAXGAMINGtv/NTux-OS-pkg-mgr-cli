# NTux Package Manager (ntuxpkg) — Standalone Build
# ELF binary for NTux-OS
#
# Usage:
#   make              # build ntuxpkg.elf
#   make run          # deploy to NTux-OS and start QEMU
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

# === NTux-OS deployment ===
NTUX_OS_DIR  ?= NTux-OS
NTUX_OS_REPO ?= https://github.com/EPAXGAMINGtv/NTux-OS-V2.git

# === Userspace Sources ===
SRC := $(wildcard src/*.c)
OBJ := $(patsubst src/%.c,obj/%.o,$(SRC))

# === Local libc runtime ===
RUNTIME_C_SRC   := $(wildcard libc/*.c)
RUNTIME_ASM_SRC := $(wildcard libc/*.asm)

RUNTIME_C_OBJ   := $(patsubst libc/%.c,obj/libc/%.o,$(RUNTIME_C_SRC))
RUNTIME_ASM_OBJ := $(patsubst libc/%.asm,obj/libc/%.o,$(RUNTIME_ASM_SRC))

RUNTIME_OBJ := $(RUNTIME_C_OBJ) $(RUNTIME_ASM_OBJ)

OBJ_DIR := obj
OUT_DIR := out

.PHONY: all clean run

all: $(OUT_DIR)/$(APP_NAME).elf

$(OUT_DIR)/$(APP_NAME).elf: $(OBJ) $(RUNTIME_OBJ) linker.ld
	@mkdir -p $(OUT_DIR)
	$(LD) $(LDFLAGS) \
		--defsym=USER_BASE=$(APP_BASE) \
		$(filter %.o,$^) \
		-o $@
	@echo "BUILT $(OUT_DIR)/$(APP_NAME).elf"
	@echo "Base Address: 0x$$(printf '%X' $(APP_BASE))"

# Compile userspace C sources
obj/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

# Compile libc C sources
obj/libc/%.o: libc/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

# Assemble libc ASM sources
obj/libc/%.o: libc/%.asm
	@mkdir -p $(dir $@)
	$(ASM) $< -o $@

run: $(OUT_DIR)/$(APP_NAME).elf
	@if [ ! -d "$(NTUX_OS_DIR)" ]; then \
		echo "Cloning NTux-OS into $(NTUX_OS_DIR)..."; \
		git clone $(NTUX_OS_REPO) $(NTUX_OS_DIR); \
		cd $(NTUX_OS_DIR) && git submodule update --init; \
	fi

	@mkdir -p $(NTUX_OS_DIR)/userspace/bin
	@cp $(OUT_DIR)/$(APP_NAME).elf \
		$(NTUX_OS_DIR)/userspace/bin/$(APP_NAME).elf

	@echo "==> Deployed $(APP_NAME).elf to NTux-OS"

	-$(MAKE) -C $(NTUX_OS_DIR) run

	@if [ ! -f "$(NTUX_OS_DIR)/NTux-OS-x86_64.iso" ]; then \
		echo "ISO build failed, rebuilding..."; \
		$(MAKE) -C $(NTUX_OS_DIR) kernel; \
		$(MAKE) -C $(NTUX_OS_DIR) NTux-OS-x86_64.iso -i; \
	fi

	@echo "==> Starting QEMU..."

	cd $(NTUX_OS_DIR) && \
	qemu-system-x86_64 \
		-enable-kvm \
		-cpu host \
		-m 8G \
		-cdrom NTux-OS-x86_64.iso \
		-serial stdio

clean:
	rm -rf \
		$(OBJ_DIR) \
		$(OUT_DIR) \
		$(NTUX_OS_DIR)