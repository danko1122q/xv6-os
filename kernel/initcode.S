# Function: start
# Purpose: Entry point for the very first user-space process (Process 1).
# This code manually constructs a stack frame to execute the 'exec' system call,
# which replaces the current process image with the "/init" binary.

#include "syscall.h"
#include "traps.h"

.globl start
start:
  # --- Setup the Stack for exec(char *path, char **argv) ---
  # According to the C calling convention (cdecl), arguments are pushed 
  # onto the stack in reverse order.
  
  pushl $argv           # Argument 2: Pointer to the array of argument strings
  pushl $init           # Argument 1: Pointer to the program path string ("/init")
  
  # The 'call' instruction usually pushes a return address. Since we are 
  # manually jumping into a system call, we push a '0' as a fake return 
  # address to maintain correct stack alignment.
  pushl $0              
  
  # --- System Call Invocation ---
  # We move the system call index into EAX and trigger a software interrupt.
  # This switches the CPU from User Mode to Kernel Mode.
  
  movl $SYS_exec, %eax  # Load the numeric constant for 'exec' (defined in syscall.h)
  int $T_SYSCALL        # Trigger the trap (usually 0x40 or 0x80) to enter the kernel

# Function: exit
# Purpose: Error handling fallback.
# If exec() is successful, it never returns here because the process 
# memory is replaced. If it DOES return, something went wrong.
exit:
  movl $SYS_exit, %eax  # Load the 'exit' system call number
  int $T_SYSCALL        # Tell the kernel to terminate this process
  jmp exit              # Protection: Infinite loop if exit fails to prevent 
                        # executing random memory/garbage instructions.

# --- Data Section ---

# The path to the first executable on the disk. 
# It must be null-terminated ('\0') for the kernel string functions.
init:
  .string "/init\0"

# The argv array: An array of pointers to strings.
# By convention: argv[0] = program name, argv[1] = NULL (terminator).
.p2align 2              # Align to 4-byte boundary for proper pointer access
argv:
  .long init            # argv[0] points to the "/init" string
  .long 0               # argv[1] is the NULL pointer signaling the end of args
