/* 
 * File:   ds.h
 * Author: prane
 *
 * Created on 26 October 2026, 12:40
 */
#ifndef DS1307_H
#define DS1307_H
#include <xc.h>
#define DS1307_WRITE 0xD0
#define DS1307_READ  0xD1
#define RTC_SECONDS  0x00
#define RTC_MINUTES  0x01
#define RTC_HOURS    0x02
#define RTC_WEEKDAY  0x03
#define RTC_DATE     0x04
#define RTC_MONTH    0x05
#define RTC_YEAR     0x06
unsigned char rtc_read(unsigned char reg);
void rtc_write(unsigned char reg, unsigned char value);
unsigned char bcd_to_bin(unsigned char value);
unsigned char bin_to_bcd(unsigned char value);
#endif
