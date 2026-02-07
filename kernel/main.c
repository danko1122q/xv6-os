#include "defs.h"
#include "memlayout.h"
#include "mmu.h"
#include "param.h"
#include "proc.h"
#include "types.h"
#include "x86.h"

static void startothers(void);
static void mpmain(void) __attribute__((noreturn));
extern pde_t *kpgdir;
extern char end[];

int main(void) {
	kinit1(end, P2V(4 * 1024 * 1024));
	kvmalloc();
	mpinit();
	lapicinit();
	seginit();
	picinit();
	ioapicinit();
	consoleinit();
	uartinit();
	mouseinit();
	pinit();
	tvinit();
	binit();
	fileinit();
	ideinit();
	initGUI();
	startothers();
	kinit2(P2V(4 * 1024 * 1024), P2V(PHYSTOP));
	userinit();
	mpmain();
}

static void mpenter(void) {
	switchkvm();
	seginit();
	lapicinit();
	mpmain();
}

static void mpmain(void) {
	cprintf("cpu%d: starting %d\n", cpuid(), cpuid());
	idtinit();
	xchg(&(mycpu()->started), 1);

	// TAMBAHAN: Init RTC di boot processor
	rtc_init();

	scheduler();
}

pde_t entrypgdir[];

static void startothers(void) {
	extern uchar _binary_entryother_start[], _binary_entryother_size[];
	uchar *code;
	struct cpu *c;
	char *stack;

	code = P2V(0x7000);
	memmove(code, _binary_entryother_start, (uint)_binary_entryother_size);

	for (c = cpus; c < cpus + ncpu; c++) {
		if (c == mycpu())
			continue;

		stack = kalloc();
		*(void **)(code - 4) = stack + KSTACKSIZE;
		*(void (**)(void))(code - 8) = mpenter;
		*(int **)(code - 12) = (void *)V2P(entrypgdir);

		lapicstartap(c->apicid, V2P(code));

		while (c->started == 0)
			;
	}
}

__attribute__((__aligned__(PGSIZE))) pde_t entrypgdir[NPDENTRIES] = {
	[0] = (0) | PTE_P | PTE_W | PTE_PS,
	[KERNBASE >> PDXSHIFT] = (0) | PTE_P | PTE_W | PTE_PS,
};