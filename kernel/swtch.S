# Function: swtch
# Purpose: Performs a context switch between two kernel threads.
# Parameters:
#   old: Location to store the current stack pointer.
#   new: The stack pointer of the thread to switch to.

.globl swtch
swtch:
  # Load arguments into registers for manipulation
  movl 4(%esp), %eax
  movl 8(%esp), %edx

  # Save callee-saved registers onto the current stack
  # This builds the context structure for the outgoing task
  pushl %ebp
  pushl %ebx
  pushl %esi
  pushl %edi

  # Execute the stack switch
  movl %esp, (%eax)  # Store current ESP into the old context pointer
  movl %edx, %esp    # Set the new ESP from the target context

  # Restore callee-saved registers from the newly loaded stack
  # The order follows the LIFO principle relative to the push sequence
  popl %edi
  popl %esi
  popl %ebx
  popl %ebp

  # Return to the caller in the context of the new thread
  ret