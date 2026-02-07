// kernel/rtc.c - Real-Time Clock Driver

#include "types.h"
#include "x86.h"
#include "defs.h"

#define RTC_PORT_ADDR 0x70
#define RTC_PORT_DATA 0x71

// CMOS RTC registers
#define RTC_SECONDS 0x00
#define RTC_MINUTES 0x02
#define RTC_HOURS 0x04
#define RTC_DAY 0x07
#define RTC_MONTH 0x08
#define RTC_YEAR 0x09
#define RTC_STATUS_A 0x0A
#define RTC_STATUS_B 0x0B

static int bcd_to_bin(int bcd) { return ((bcd >> 4) * 10) + (bcd & 0x0F); }

static int is_update_in_progress() {
	outb(RTC_PORT_ADDR, RTC_STATUS_A);
	return (inb(RTC_PORT_DATA) & 0x80) != 0;
}

void rtc_read_time(int *hours, int *minutes, int *seconds) {
	// Tunggu sampai tidak ada update yang berjalan
	while (is_update_in_progress())
		;

	outb(RTC_PORT_ADDR, RTC_HOURS);
	*hours = bcd_to_bin(inb(RTC_PORT_DATA));

	outb(RTC_PORT_ADDR, RTC_MINUTES);
	*minutes = bcd_to_bin(inb(RTC_PORT_DATA));

	outb(RTC_PORT_ADDR, RTC_SECONDS);
	*seconds = bcd_to_bin(inb(RTC_PORT_DATA));
}

void rtc_read_date(int *day, int *month, int *year) {
	while (is_update_in_progress())
		;

	outb(RTC_PORT_ADDR, RTC_DAY);
	*day = bcd_to_bin(inb(RTC_PORT_DATA));

	outb(RTC_PORT_ADDR, RTC_MONTH);
	*month = bcd_to_bin(inb(RTC_PORT_DATA));

	outb(RTC_PORT_ADDR, RTC_YEAR);
	*year = bcd_to_bin(inb(RTC_PORT_DATA)) + 2000; // Asumsi 2000+
}

void rtc_init() { cprintf("RTC: Real-Time Clock initialized\n"); }