# --- CONFIG ---
K = kernel
U = user
B = build
I = include
S = scripts

# --- OBJECTS ---
OBJS_NAMES = bio.o console.o exec.o file.o fs.o ide.o ioapic.o kalloc.o \
             kbd.o lapic.o log.o main.o mp.o picirq.o pipe.o proc.o \
             sleeplock.o spinlock.o string.o swtch.o syscall.o sysfile.o \
             sysproc.o trapasm.o trap.o uart.o vm.o gui.o mouse.o msg.o \
             window_manager.o

OBJS = $(addprefix $(B)/, $(OBJS_NAMES))

# --- TOOLS ---
CC = gcc
LD = ld
OBJCOPY = objcopy
OBJDUMP = objdump

# Flags untuk xv6 (32-bit, nostdinc)
CFLAGS = -fno-pic -static -fno-builtin -fno-strict-aliasing -O2 -Wall -MD -ggdb -m32 -Werror \
         -fno-omit-frame-pointer -fno-stack-protector -fno-pie -no-pie -nostdinc -I$(I)
LDFLAGS = -m elf_i386

# --- TARGETS ---
all: build_dir $(B)/xv6.img $(B)/fs.img

build_dir:
	@mkdir -p $(B)

$(B)/xv6.img: $(B)/bootblock $(B)/kernel
	dd if=/dev/zero of=$(B)/xv6.img count=10000
	dd if=$(B)/bootblock of=$(B)/xv6.img conv=notrunc
	dd if=$(B)/kernel of=$(B)/xv6.img seek=1 conv=notrunc

# --- BOOTBLOCK ---
$(B)/bootblock: $(K)/bootasm.S $(K)/bootmain.c $(S)/sign.pl | build_dir
	$(CC) $(CFLAGS) -fno-pic -O -c $(K)/bootmain.c -o $(B)/bootmain.o
	$(CC) $(CFLAGS) -fno-pic -c $(K)/bootasm.S -o $(B)/bootasm.o
	$(LD) $(LDFLAGS) -N -e start -Ttext 0x7C00 -o $(B)/bootblock.o $(B)/bootasm.o $(B)/bootmain.o
	$(OBJCOPY) -S -O binary -j .text $(B)/bootblock.o $(B)/bootblock
	perl $(S)/sign.pl $(B)/bootblock

# --- KERNEL ---
$(B)/kernel: $(OBJS) $(B)/entry.o $(B)/entryother $(B)/initcode $(B)/vectors.o $(K)/kernel.ld | build_dir
	cd $(B) && $(LD) $(LDFLAGS) -T ../$(K)/kernel.ld -o kernel ../$(B)/entry.o ../$(B)/vectors.o $(OBJS_NAMES) -b binary initcode entryother
	$(OBJDUMP) -S $(B)/kernel > $(B)/kernel.asm
	$(OBJDUMP) -t $(B)/kernel | sed '1,/SYMBOL TABLE/d; s/ .* / /; /^$$/d' > $(B)/kernel.sym

# --- RULES ---
$(B)/%.o: $(K)/%.c | build_dir
	$(CC) $(CFLAGS) -c $< -o $@

$(B)/%.o: $(K)/%.S | build_dir
	$(CC) $(CFLAGS) -c $< -o $@

$(B)/vectors.o: $(S)/vectors.pl | build_dir
	perl $(S)/vectors.pl > $(B)/vectors.S
	$(CC) $(CFLAGS) -c $(B)/vectors.S -o $(B)/vectors.o

$(B)/initcode: $(K)/initcode.S | build_dir
	$(CC) $(CFLAGS) -c $< -o $(B)/initcode.o
	$(LD) $(LDFLAGS) -N -e start -Ttext 0 -o $(B)/initcode.out $(B)/initcode.o
	$(OBJCOPY) -S -O binary $(B)/initcode.out $(B)/initcode

$(B)/entryother: $(K)/entryother.S | build_dir
	$(CC) $(CFLAGS) -fno-pic -c $< -o $(B)/entryother.o
	$(LD) $(LDFLAGS) -N -e start -Ttext 0x7000 -o $(B)/bootblockother.o $(B)/entryother.o
	$(OBJCOPY) -S -O binary -j .text $(B)/bootblockother.o $(B)/entryother

# --- USER ---
ULIB = $(addprefix $(B)/, ulib.o usys.o printf.o umalloc.o user_gui.o user_window.o user_handler.o)

$(B)/_%: $(B)/%.o $(ULIB) | build_dir
	$(LD) $(LDFLAGS) -N -e main -Ttext 0 -o $@ $^

$(B)/%.o: $(U)/%.c | build_dir
	$(CC) $(CFLAGS) -c $< -o $@

$(B)/usys.o: $(U)/usys.S | build_dir
	$(CC) $(CFLAGS) -c $< -o $@

# --- FS ---
UPROGS = \
	$(B)/_init $(B)/_ls $(B)/_sh $(B)/_mkdir $(B)/_zombie \
	$(B)/_desktop $(B)/_startWindow $(B)/_editor $(B)/_explorer \
	$(B)/_shell $(B)/_demo

$(B)/fs.img: $(B)/mkfs readme.txt $(UPROGS) | build_dir
	$(B)/mkfs $(B)/fs.img readme.txt $(UPROGS)

# KUNCI PERBAIKAN:
$(B)/mkfs: $(K)/mkfs.c | build_dir
	gcc -Werror -Wall -o $(B)/mkfs $(K)/mkfs.c
# --- QEMU ---
QEMUOPTS = -drive file=$(B)/fs.img,index=1,media=disk,format=raw \
           -drive file=$(B)/xv6.img,index=0,media=disk,format=raw \
           -smp 2 -m 512

qemu: all
	qemu-system-i386 -serial mon:stdio $(QEMUOPTS)

clean:
	rm -rf $(B)

.PHONY: all clean qemu build_dir