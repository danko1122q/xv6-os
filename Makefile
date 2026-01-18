# --- CONFIG ---
K = kernel
U = user
B = build
I = include
S = scripts
IMG = img
OS_NAME = xv6OS

# --- OBJECTS ---
OBJS_NAMES = bio.o character.o console.o exec.o file.o fs.o ide.o ioapic.o kalloc.o \
	     kbd.o lapic.o log.o main.o mp.o picirq.o pipe.o proc.o \
	     sleeplock.o spinlock.o string.o swtch.o syscall.o sysfile.o \
	     sysproc.o trapasm.o trap.o uart.o vm.o gui.o mouse.o msg.o \
	     window_manager.o icons_data.o

OBJS = $(addprefix $(B)/, $(OBJS_NAMES))

CC = gcc
LD = ld
OBJCOPY = objcopy
OBJDUMP = objdump

CFLAGS = -fno-pic -static -fno-builtin -fno-strict-aliasing -O2 -Wall -MD -ggdb -m32 -Werror \
	 -fno-omit-frame-pointer -fno-stack-protector -fno-pie -no-pie -nostdinc -I$(I) \
	 -Wno-array-bounds -Wno-infinite-recursion

LDFLAGS = -m elf_i386

# --- MAIN TARGETS ---
all: setup icons $(IMG)/$(OS_NAME).img $(IMG)/fs.img

setup:
	@mkdir -p $(B)
	@mkdir -p $(IMG)

icons: $(wildcard icon/*.png)
	@echo "Automatically converting PNG icons..."
	@python3 convert.py

$(IMG)/$(OS_NAME).img: $(B)/bootblock $(B)/kernel
	@dd if=/dev/zero of=$(IMG)/$(OS_NAME).img count=10000 status=none
	@dd if=$(B)/bootblock of=$(IMG)/$(OS_NAME).img conv=notrunc status=none
	@dd if=$(B)/kernel of=$(IMG)/$(OS_NAME).img seek=1 conv=notrunc status=none

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

# Rule khusus untuk character.c di folder include
$(B)/character.o: $(I)/character.c
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

# --- USER LAND ---
ULIB = $(addprefix $(B)/, ulib.o usys.o printf.o umalloc.o user_gui.o user_window.o user_handler.o icons_data.o character.o)

$(B)/_%: $(B)/%.o $(ULIB)
	$(LD) $(LDFLAGS) -N -e main -Ttext 0 -o $@ $^

$(B)/%.o: $(U)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

$(B)/usys.o: $(U)/usys.S
	$(CC) $(CFLAGS) -c $< -o $@

# --- FILE SYSTEM ---
UPROGS = \
	$(B)/_init \
	$(B)/_ls \
	$(B)/_sh \
	$(B)/_mkdir \
	$(B)/_echo \
	$(B)/_proc_demo \
	$(B)/_desktop \
	$(B)/_startWindow \
	$(B)/_editor \
	$(B)/_explorer \
	$(B)/_terminal \
	$(B)/_floppybird

$(IMG)/fs.img: $(B)/mkfs README.md LICENSE readme.txt $(UPROGS)
	$(B)/mkfs $(IMG)/fs.img README.md LICENSE readme.txt $(UPROGS)

$(B)/mkfs: $(K)/mkfs.c
	gcc -Werror -Wall -o $(B)/mkfs $(K)/mkfs.c

QEMUOPTS = -drive file=$(IMG)/fs.img,index=1,media=disk,format=raw \
	   -drive file=$(IMG)/$(OS_NAME).img,index=0,media=disk,format=raw \
	   -smp 2 -m 512

run:
	@qemu-system-i386 -serial mon:stdio $(QEMUOPTS)

makerun: all run

clean:
	rm -rf $(B) $(IMG)

# --- FORMATTING ---
format:
	@echo "Formatting source code (.c files only)..."
	@find $(K) $(U) -name "*.c" | grep -v "icons_data.c" | xargs clang-format -i
	@echo "Formatting complete."

.PHONY: all clean setup run icons makerun format