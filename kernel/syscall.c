#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"
#include "syscall.h"

// Fetch functions (fetchint, fetchstr, argint, argptr, argstr) tetap sama...
int fetchint(uint addr, int *ip) {
  struct proc *curproc = myproc();
  if(addr >= curproc->sz || addr+4 > curproc->sz) return -1;
  *ip = *(int*)(addr);
  return 0;
}

int fetchstr(uint addr, char **pp) {
  char *s, *ep;
  struct proc *curproc = myproc();
  if(addr >= curproc->sz) return -1;
  *pp = (char*)addr;
  ep = (char*)curproc->sz;
  for(s = *pp; s < ep; s++){ if(*s == 0) return s - *pp; }
  return -1;
}

int argint(int n, int *ip) { return fetchint((myproc()->tf->esp) + 4 + 4*n, ip); }

int argptr(int n, char **pp, int size) {
  int i;
  struct proc *curproc = myproc();
  if(argint(n, &i) < 0) return -1;
  if(size < 0 || (uint)i >= curproc->sz || (uint)i+size > curproc->sz) return -1;
  *pp = (char*)i;
  return 0;
}

int argstr(int n, char **pp) {
  int addr;
  if(argint(n, &addr) < 0) return -1;
  return fetchstr(addr, pp);
}

// Prototypes for the functions that handle system calls.
extern int sys_chdir(void);
extern int sys_close(void);
extern int sys_dup(void);
extern int sys_exec(void);
extern int sys_exit(void);
extern int sys_fork(void);
extern int sys_fstat(void);
extern int sys_getpid(void);
extern int sys_kill(void);
extern int sys_link(void);
extern int sys_mkdir(void);
extern int sys_mknod(void);
extern int sys_open(void);
extern int sys_pipe(void);
extern int sys_read(void);
extern int sys_sbrk(void);
extern int sys_sleep(void);
extern int sys_unlink(void);
extern int sys_wait(void);
extern int sys_write(void);
extern int sys_uptime(void);
extern int sys_GUI_createWindow(void);
extern int sys_GUI_closeWindow(void);
extern int sys_GUI_getMessage(void);
extern int sys_GUI_getPopupMessage(void);
extern int sys_GUI_updateScreen(void);
extern int sys_GUI_turnoffScreen(void);
extern int sys_GUI_maximizeWindow(void);
extern int sys_GUI_minimizeWindow(void);
extern int sys_GUI_createPopupWindow(void);
extern int sys_GUI_closePopupWindow(void);
extern int sys_halt(void);
extern int sys_reboot(void);

static int (*syscalls[])(void) = {
[SYS_fork]    sys_fork,
[SYS_exit]    sys_exit,
[SYS_wait]    sys_wait,
[SYS_pipe]    sys_pipe,
[SYS_read]    sys_read,
[SYS_kill]    sys_kill,
[SYS_exec]    sys_exec,
[SYS_fstat]   sys_fstat,
[SYS_chdir]   sys_chdir,
[SYS_dup]     sys_dup,
[SYS_getpid]  sys_getpid,
[SYS_sbrk]    sys_sbrk,
[SYS_sleep]   sys_sleep,
[SYS_uptime]  sys_uptime,
[SYS_open]    sys_open,
[SYS_write]   sys_write,
[SYS_mknod]   sys_mknod,
[SYS_unlink]  sys_unlink,
[SYS_link]    sys_link,
[SYS_mkdir]   sys_mkdir,
[SYS_close]   sys_close,
[SYS_GUI_createWindow]      sys_GUI_createWindow,
[SYS_GUI_closeWindow]       sys_GUI_closeWindow,
[SYS_GUI_getMessage]        sys_GUI_getMessage,
[SYS_GUI_getPopupMessage]   sys_GUI_getPopupMessage,
[SYS_GUI_updateScreen]      sys_GUI_updateScreen,
[SYS_GUI_turnoffScreen]     sys_GUI_turnoffScreen,
[SYS_GUI_maximizeWindow]    sys_GUI_maximizeWindow,
[SYS_GUI_minimizeWindow]    sys_GUI_minimizeWindow,
[SYS_GUI_createPopupWindow] sys_GUI_createPopupWindow,
[SYS_GUI_closePopupWindow]  sys_GUI_closePopupWindow,
[SYS_halt]                  sys_halt,
[SYS_reboot]                sys_reboot,
};

void
syscall(void)
{
  int num;
  struct proc *curproc = myproc();

  num = curproc->tf->eax;
  if(num > 0 && num < NELEM(syscalls) && syscalls[num]) {
    curproc->tf->eax = syscalls[num]();
  } else {
    cprintf("%d %s: unknown sys call %d\n",
            curproc->pid, curproc->name, num);
    curproc->tf->eax = -1;
  }
}