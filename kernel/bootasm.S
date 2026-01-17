#include "asm.h"
#include "memlayout.h"
#include "mmu.h"
#include "gui.h"

# bootstrap sequence
# This code transitions the system from BIOS Real Mode (16-bit) to 32-bit Protected Mode.
# The BIOS loads the first sector of the boot disk into physical address 0x7c00.

.code16                         # Assemble for 16-bit real mode
.globl start
start:
  cli                           # Disable hardware interrupts. We cannot handle them 
                                # safely during the processor mode transition.

setupsvga:
  # --- VESA BIOS Extensions (VBE) Initialization ---
  # We set the video resolution here because BIOS interrupts (int $0x10) 
  # are generally unavailable once we switch to 32-bit Protected Mode.
  
  movb    $0x4f,%ah             # VBE Function: 0x4f is the VBE standard prefix
  movb    $0x01,%al             # VBE Sub-function 01: Return VBE mode information
  movw    $0x4115,%cx           # Mode 0x115: 800x600 resolution with 24-bit color
  movw    $0x1000,%di           # Destination buffer for mode info at address 0x1000
  int     $0x10                 # Call Video BIOS Interrupt

  movw    $GUI_BUF,%bx          # Load the GUI buffer segment into BX
  movw    %bx, %es              # ES now points to the GUI buffer area

  # --- Apply Video Mode ---
  movb    $0x4f,%ah             
  movb    $0x02,%al             # VBE Sub-function 02: Set VBE mode
  movw    $0x4115,%bx           # Bit 14 (0x4000) is set for Linear Frame Buffer access
  int     $0x10                 # The screen clears and switches to 800x600 graphics

  # Reset data segment registers to 0 to ensure a clean, predictable memory state
  xorw    %ax,%ax               # Zero out AX
  movw    %ax,%ds               # Data Segment = 0
  movw    %ax,%es               # Extra Segment = 0
  movw    %ax,%ss               # Stack Segment = 0



seta20.1:
  # --- Enable A20 Gate ---
  # Legacy x86 PCs wrap memory access at 1MB for 8086 compatibility.
  # We enable the A20 line via the Keyboard Controller (8042 chip) to access 
  # all physical memory up to 4GB.
  
  inb     $0x64,%al             # Read Status Register from keyboard controller
  testb   $0x2,%al              # Check bit 1 (Input buffer status)
  jnz     seta20.1              # If busy (1), loop until controller is ready

  movb    $0xd1,%al             # Command 0xd1: Write to Output Port
  outb    %al,$0x64             # Send command to port 0x64

seta20.2:
  inb     $0x64,%al             # Wait for the controller to process the command
  testb   $0x2,%al
  jnz     seta20.2

  movb    $0xdf,%al             # 0xdf = 11011111 (enables A20 line)
  outb    %al,$0x60             # Write data to port 0x60

  # --- Transition to Protected Mode ---
  # 1. Load the Global Descriptor Table (GDT)
  lgdt    gdtdesc               
  
  # 2. Set the PE (Protection Enable) bit in Control Register 0 (CR0)
  movl    %cr0, %eax
  orl     $CR0_PE, %eax         # CR0_PE is bit 0
  movl    %eax, %cr0            # After this line, the CPU is in 32-bit Protected Mode

  # 3. Perform a Long Jump (ljmp)
  # This is mandatory to:
  #   a) Flush the 16-bit instruction prefetch queue.
  #   b) Reload the Code Segment (CS) register with a valid GDT selector.
  ljmp    $(SEG_KCODE<<3), $start32

.code32                         # Assemble for 32-bit protected mode
start32:
  # Set up 32-bit Data Segment registers using the GDT selector
  # (SEG_KDATA << 3) calculates the byte offset into the GDT.
  movw    $(SEG_KDATA<<3), %ax
  movw    %ax, %ds              # Set Data Segment
  movw    %ax, %es              # Set Extra Segment
  movw    %ax, %ss              # Set Stack Segment
  
  # Initialize remaining segments to null
  movw    $0, %ax
  movw    %ax, %fs
  movw    %ax, %gs

  # Initialize the stack pointer to point below the boot code (growing downwards)
  movl    $start, %esp
  
  # Hand over control to the C kernel loader (bootmain.c)
  call    bootmain

  # --- Error Handling ---
  # If bootmain returns (it shouldn't), send debug codes to I/O ports for logic analyzers.
  movw    $0x8a00, %ax
  movw    %ax, %dx
  outw    %ax, %dx
  movw    $0x8ae0, %ax
  outw    %ax, %dx

spin:
  jmp     spin                  # Infinite loop to halt execution

# --- Global Descriptor Table (GDT) ---
# This table defines memory segments, permissions, and base/limit addresses.
.p2align 2                      # Align to 4-byte boundary for CPU efficiency
gdt:
  SEG_NULLASM                             # Null segment (Required by x86)
  SEG_ASM(STA_X|STA_R, 0x0, 0xffffffff)   # Kernel Code: Exec/Read, 0-4GB
  SEG_ASM(STA_W, 0x0, 0xffffffff)         # Kernel Data: Read/Write, 0-4GB

# GDT Descriptor passed to the 'lgdt' instruction
gdtdesc:
  .word   (gdtdesc - gdt - 1)             # Size of GDT (Limit)
  .long   gdt                             # Physical address of the GDT
