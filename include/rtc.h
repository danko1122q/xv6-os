// include/rtc.h

#ifndef __RTC_H__
#define __RTC_H__

void rtc_init(void);
void rtc_read_time(int *hours, int *minutes, int *seconds);
void rtc_read_date(int *day, int *month, int *year);

#endif