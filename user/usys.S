#include "syscall.h"
#include "traps.h"

#define SYSCALL(name) \
  .globl name; \
  name: \
    movl $SYS_ ## name, %eax; \
    int $T_SYSCALL; \
    ret

SYSCALL(fork)
SYSCALL(exit)
SYSCALL(wait)
SYSCALL(pipe)
SYSCALL(read)
SYSCALL(write)
SYSCALL(close)
SYSCALL(kill)
SYSCALL(exec)
SYSCALL(open)
SYSCALL(mknod)
SYSCALL(unlink)
SYSCALL(fstat)
SYSCALL(link)
SYSCALL(mkdir)
SYSCALL(chdir)
SYSCALL(dup)
SYSCALL(getpid)
SYSCALL(sbrk)
SYSCALL(sleep)
SYSCALL(uptime)
SYSCALL(GUI_createWindow)
SYSCALL(GUI_closeWindow)
SYSCALL(GUI_minimizeWindow)
SYSCALL(GUI_maximizeWindow)
SYSCALL(GUI_getMessage)
SYSCALL(GUI_updateScreen)
SYSCALL(GUI_turnoffScreen)
SYSCALL(GUI_createPopupWindow)
SYSCALL(GUI_closePopupWindow)
SYSCALL(GUI_getPopupMessage)
SYSCALL(halt)
SYSCALL(reboot)