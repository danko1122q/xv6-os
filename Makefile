# --- CONFIG ---
K = kernel
U = user
A = app
B = build
I = include
S = scripts
IMG = img
OS_NAME = Northos

# --- TOOLS ---
CC = gcc
LD = ld
OBJCOPY = objcopy
OBJDUMP = objdump

CFLAGS = -fno-pic -static -fno-builtin -fno-strict-aliasing -O2 -Wall -MD -ggdb -m32 -Werror \
         -fno-omit-frame-pointer -fno-stack-protector -fno-pie -no-pie -nostdinc -I$(I) \
         -Wno-array-bounds -Wno-infinite-recursion

LDFLAGS = -m elf_i386

# --- KERNEL OBJECTS ---
OBJS_NAMES = bio.o character.o console.o exec.o file.o fs.o ide.o ioapic.o kalloc.o \
             kbd.o lapic.o log.o main.o mp.o picirq.o pipe.o proc.o \
             sleeplock.o spinlock.o string.o swtch.o syscall.o sysfile.o \
             sysproc.o trapasm.o trap.o uart.o vm.o gui.o mouse.o msg.o \
             window_manager.o icons_data.o app_icons_data.o rtc.o

OBJS = $(addprefix $(B)/, $(OBJS_NAMES))

# ============================================================================
# MAIN TARGETS
# ============================================================================

all: setup icons app_icons $(IMG)/$(OS_NAME).img $(IMG)/fs.img

setup:
	@mkdir -p $(B)
	@mkdir -p $(IMG)
	@mkdir -p app_icons

icons: $(wildcard icon/*.png)
	@echo "Converting window icons..."
	@python3 convert.py

app_icons: $(wildcard app_icons/*.png)
	@echo "Converting app desktop icons..."
	@python3 convert_icons.py

$(IMG)/$(OS_NAME).img: $(B)/bootblock $(B)/kernel
	@dd if=/dev/zero of=$(IMG)/$(OS_NAME).img count=10000 status=none
	@dd if=$(B)/bootblock of=$(IMG)/$(OS_NAME).img conv=notrunc status=none
	@dd if=$(B)/kernel of=$(IMG)/$(OS_NAME).img seek=1 conv=notrunc status=none

# ============================================================================
# KERNEL BUILD
# ============================================================================

$(B)/bootblock: $(K)/bootasm.S $(K)/bootmain.c $(S)/sign.go
	$(CC) $(CFLAGS) -fno-pic -O -c $(K)/bootmain.c -o $(B)/bootmain.o
	$(CC) $(CFLAGS) -fno-pic -c $(K)/bootasm.S -o $(B)/bootasm.o
	$(LD) $(LDFLAGS) -N -e start -Ttext 0x7C00 -o $(B)/bootblock.o $(B)/bootasm.o $(B)/bootmain.o
	$(OBJCOPY) -S -O binary -j .text $(B)/bootblock.o $(B)/bootblock
	@echo "Signing bootblock with Go..."
	go run $(S)/sign.go $(B)/bootblock

$(B)/kernel: $(OBJS) $(B)/entry.o $(B)/entryother $(B)/initcode $(B)/vectors.o $(K)/kernel.ld
	cd $(B) && $(LD) $(LDFLAGS) -T ../$(K)/kernel.ld -o kernel entry.o vectors.o $(OBJS_NAMES) -b binary initcode entryother
	$(OBJDUMP) -S $(B)/kernel > $(B)/kernel.asm
	$(OBJDUMP) -t $(B)/kernel | sed '1,/SYMBOL TABLE/d; s/ .* / /; /^$$/d' > $(B)/kernel.sym

$(B)/%.o: $(K)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

$(B)/character.o: $(K)/character.c
	$(CC) $(CFLAGS) -c $< -o $@

$(B)/%.o: $(K)/%.S
	$(CC) $(CFLAGS) -c $< -o $@

$(B)/vectors.o: $(S)/vectors.go
	@echo "Generating vectors.S with Go..."
	go run $(S)/vectors.go > $(B)/vectors.S
	$(CC) $(CFLAGS) -c $(B)/vectors.S -o $(B)/vectors.o

$(B)/initcode: $(K)/initcode.S
	$(CC) $(CFLAGS) -c $< -o $(B)/initcode.o
	$(LD) $(LDFLAGS) -N -e start -Ttext 0 -o $(B)/initcode.out $(B)/initcode.o
	$(OBJCOPY) -S -O binary $(B)/initcode.out $(B)/initcode

$(B)/entryother: $(K)/entryother.S
	$(CC) $(CFLAGS) -fno-pic -c $< -o $(B)/entryother.o
	$(LD) $(LDFLAGS) -N -e start -Ttext 0x7000 -o $(B)/bootblockother.o $(B)/entryother.o
	$(OBJCOPY) -S -O binary -j .text $(B)/bootblockother.o $(B)/entryother

# ============================================================================
# USER LIBRARY
# ============================================================================

ULIB_OBJS = ulib.o usys.o printf.o umalloc.o user_gui.o user_window.o \
            user_handler.o icons_data.o app_icons_data.o character.o

ULIB = $(addprefix $(B)/, $(ULIB_OBJS))

# User library compilation
$(B)/%.o: $(U)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

$(B)/%.o: $(U)/%.S
	$(CC) $(CFLAGS) -c $< -o $@

# ============================================================================
# BASE USER PROGRAMS (single file in user/)
# ============================================================================

# Pattern: build/_program depends on build/program.o
$(B)/_%: $(B)/%.o $(ULIB)
	$(LD) $(LDFLAGS) -N -e main -Ttext 0 -o $@ $^

# ============================================================================
# app
# ============================================================================

# --- Terminal
$(B)/terminal.o: $(A)/terminal/terminal.c
	$(CC) $(CFLAGS) -c $< -o $@

$(B)/_terminal: $(B)/terminal.o $(ULIB)
	$(LD) $(LDFLAGS) -N -e main -Ttext 0 -o $@ $^

# --- Editor
$(B)/editor.o: $(A)/editor/editor.c
	$(CC) $(CFLAGS) -c $< -o $@

$(B)/_editor: $(B)/editor.o $(ULIB)
	$(LD) $(LDFLAGS) -N -e main -Ttext 0 -o $@ $^

# --- Explorer
$(B)/explorer.o: $(A)/explorer/explorer.c
	$(CC) $(CFLAGS) -c $< -o $@

$(B)/_explorer: $(B)/explorer.o $(ULIB)
	$(LD) $(LDFLAGS) -N -e main -Ttext 0 -o $@ $^

# --- Calculator
$(B)/calculator.o: $(A)/calculator/calculator.c
	$(CC) $(CFLAGS) -c $< -o $@

$(B)/_calculator: $(B)/calculator.o $(ULIB)
	$(LD) $(LDFLAGS) -N -e main -Ttext 0 -o $@ $^

# --- Flappy Bird
$(B)/floppybird.o: $(A)/floppybird/floppybird.c
	$(CC) $(CFLAGS) -c $< -o $@

$(B)/_floppybird: $(B)/floppybird.o $(ULIB)
	$(LD) $(LDFLAGS) -N -e main -Ttext 0 -o $@ $^

# ============================================================================
# FILE SYSTEM IMAGE
# ============================================================================

UPROGS = \
	$(B)/_init \
	$(B)/_ls \
	$(B)/_sh \
	$(B)/_mkdir \
	$(B)/_echo \
	$(B)/_proc_demo \
	$(B)/_desktop \
	$(B)/_startWindow \
	$(B)/_terminal \
	$(B)/_editor \
	$(B)/_explorer \
	$(B)/_floppybird \
	$(B)/_calculator \
	
$(IMG)/fs.img: $(B)/mkfs README.md LICENSE readme.txt $(UPROGS)
	$(B)/mkfs $(IMG)/fs.img README.md LICENSE readme.txt $(UPROGS)

$(B)/mkfs: $(K)/mkfs.c
	gcc -Werror -Wall -o $(B)/mkfs $(K)/mkfs.c

# ============================================================================
# UTILITIES
# ============================================================================

QEMUOPTS = -drive file=$(IMG)/fs.img,index=1,media=disk,format=raw \
	   -drive file=$(IMG)/$(OS_NAME).img,index=0,media=disk,format=raw \
	   -smp 2 -m 512

run:
	@qemu-system-i386 -serial mon:stdio $(QEMUOPTS)

makerun: all run

clean:
	rm -rf $(B) $(IMG)

format:
	@echo "Formatting source code..."
	@find $(K) $(U) $(A) -name "*.c" | grep -v "icons_data.c" | grep -v "app_icons_data.c" | xargs clang-format -i
	@echo "Formatting complete."

.PHONY: all clean setup run icons app_icons makerun format

-include $(B)/*.d