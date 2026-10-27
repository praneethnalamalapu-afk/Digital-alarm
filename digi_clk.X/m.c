#include <xc.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "m.h"
#include "clcd.h"
#include "mkp.h"
#include "i2c.h"
#include "ds.h"
#include "eeprom24.h"
/* Digital Timer for PIC18F4580, Fosc=20 MHz, XC8.
 * Matrix-key mapping: SW1=UP, SW2=DOWN, SW3=RIGHT, SW4=LEFT.
 * Hold UP to enter/accept; hold DOWN to go back/logout.
 * In edit screens UP/DOWN changes a value and LEFT/RIGHT selects a field.
 */
#define EVENT_COUNT 8U
#define IDLE_LIMIT_TICKS 500U       /* 5 s at 10 ms/tick */
#define LONG_PRESS_TICKS 70U        /* 700 ms */
#define RING_TOGGLE_TICKS 50U       /* 500 ms */
#define EEPROM_BASE 0x10U
#define EVENT_MAGIC 0xD7U
#define KEY_NONE 0xFFU
#define KEY_UP 1U
#define KEY_DOWN 2U
#define KEY_RIGHT 3U
#define KEY_LEFT 4U
typedef enum  {
    SCR_DEFAULT, SCR_CONFIG, SCR_EVENT_MENU, SCR_SET_EVENT, SCR_VIEW_EVENT, SCR_SET_TIME, SCR_SET_DATE
}
screen_t;
typedef struct  {
    uint8_t hour, minute, duration, type, weekday, day, month, year, enabled;
}
event_t;
typedef struct  {
    uint8_t sec,min,hour,wday,day,month,year;
}
datetime_t;
static event_t events[EVENT_COUNT];
static screen_t screen = SCR_DEFAULT;
static uint8_t menu_index=0, event_index=0, edit_field=0, view_index=0;
static uint8_t ring_active=0, ring_event=0xFF, ring_silenced=0;
static uint16_t idle_ticks=0, key_hold_ticks=0, tick10=0;
static uint32_t ring_ticks=0;
static uint8_t previous_key=KEY_NONE, key_long_sent=0, last_trigger_min=0xFF, last_trigger_day=0xFF, last_trigger_month=0xFF, last_trigger_year=0xFF;
static datetime_t dt;
static uint8_t min_u8(uint8_t a,uint8_t b) {
    return a<b?a:b;
}
static void print2(uint8_t v,uint8_t addr) {
    clcd_putch((uint8_t)('0'+v/10U),addr);
    clcd_putch((uint8_t)('0'+v%10U),(uint8_t)(addr+1U));
}
static void spaces(uint8_t addr,uint8_t n) {
    while(n--) clcd_putch(' ',addr++);
}
static uint8_t leap_year(uint8_t yy) {
    return ((yy%4U)==0U);
}
static uint8_t days_in_month(uint8_t m,uint8_t y) {
    static const uint8_t d[]= {
        31,28,31,30,31,30,31,31,30,31,30,31
    }
    ;
    if(m<1||m>12)return 31;
    if(m==2 && leap_year(y))return 29;
    return d[m-1U];
}
static uint8_t bcd_valid(uint8_t b,uint8_t max) {
    uint8_t v=bcd_to_bin(b);
    return (v<=max)?v:0;
}
static void rtc_read_all(void) {
    dt.sec=bcd_valid((uint8_t)(rtc_read(RTC_SECONDS)&0x7FU),59);
    dt.min=bcd_valid((uint8_t)(rtc_read(RTC_MINUTES)&0x7FU),59);
    dt.hour=bcd_valid((uint8_t)(rtc_read(RTC_HOURS)&0x3FU),23);
    dt.wday=bcd_valid((uint8_t)(rtc_read(RTC_WEEKDAY)&0x07U),7);
    if(dt.wday<1||dt.wday>7)dt.wday=1;
    dt.day=bcd_valid((uint8_t)(rtc_read(RTC_DATE)&0x3FU),31);
    if(dt.day<1)dt.day=1;
    dt.month=bcd_valid((uint8_t)(rtc_read(RTC_MONTH)&0x1FU),12);
    if(dt.month<1)dt.month=1;
    dt.year=bcd_to_bin(rtc_read(RTC_YEAR));
    if(dt.year>99)dt.year=0;
}
static void rtc_write_all(void) {
    rtc_write(RTC_SECONDS,bin_to_bcd(dt.sec));
    rtc_write(RTC_MINUTES,bin_to_bcd(dt.min));
    rtc_write(RTC_HOURS,bin_to_bcd(dt.hour));
    rtc_write(RTC_WEEKDAY,bin_to_bcd(dt.wday));
    rtc_write(RTC_DATE,bin_to_bcd(dt.day));
    rtc_write(RTC_MONTH,bin_to_bcd(dt.month));
    rtc_write(RTC_YEAR,bin_to_bcd(dt.year));
}
/* Software timebase: keeps the displayed clock advancing even if the RTC
 * oscillator is stopped or its seconds register is not updating. */
static void advance_clock_one_second(void)
{
    if (++dt.sec >= 60U)  {
        dt.sec = 0;
        if (++dt.min >= 60U)  {
            dt.min = 0;
            if (++dt.hour >= 24U)  {
                dt.hour = 0;
                dt.wday = (uint8_t)(dt.wday % 7U + 1U);
                if (++dt.day > days_in_month(dt.month, dt.year))  {
                    dt.day = 1;
                    if (++dt.month > 12U)  {
                        dt.month = 1;
                        dt.year = (uint8_t)((dt.year + 1U) % 100U);
                    }
                }
            }
        }
    }
}
static void default_events(void) {
    uint8_t i;
    for(i=0;i<EVENT_COUNT;i++) {
        events[i].hour=7;
        events[i].minute=0;
        events[i].duration=1;
        events[i].type=1;
        events[i].weekday=1;
        events[i].day=1;
        events[i].month=1;
        events[i].year=0;
        events[i].enabled=0;
    }
}
static void save_events(void) {
    uint8_t i,a=EEPROM_BASE;
    eeprom24_write(0,EVENT_MAGIC);
    for(i=0;i<EVENT_COUNT;i++) {
        eeprom24_write(a++,events[i].hour);
        eeprom24_write(a++,events[i].minute);
        eeprom24_write(a++,events[i].duration);
        eeprom24_write(a++,events[i].type);
        eeprom24_write(a++,events[i].weekday);
        eeprom24_write(a++,events[i].day);
        eeprom24_write(a++,events[i].month);
        eeprom24_write(a++,events[i].year);
        eeprom24_write(a++,events[i].enabled);
    }
}
static void load_events(void) {
    uint8_t i,a=EEPROM_BASE;
    if(eeprom24_read(0)!=EVENT_MAGIC) {
        default_events();
        save_events();
        return;
    }
    for(i=0;i<EVENT_COUNT;i++) {
        events[i].hour=eeprom24_read(a++);
        events[i].minute=eeprom24_read(a++);
        events[i].duration=eeprom24_read(a++);
        events[i].type=eeprom24_read(a++);
        events[i].weekday=eeprom24_read(a++);
        events[i].day=eeprom24_read(a++);
        events[i].month=eeprom24_read(a++);
        events[i].year=eeprom24_read(a++);
        events[i].enabled=eeprom24_read(a++);
        if(events[i].hour>23||events[i].minute>59||events[i].duration<1||events[i].duration>99||events[i].type>2||events[i].weekday<1||events[i].weekday>7||events[i].day<1||events[i].day>31||events[i].month<1||events[i].month>12||events[i].enabled>1) {
            default_events();
            save_events();
            return;
        }
    }
}
static uint8_t rtc_has_valid_time(void)
{
    uint8_t sec=rtc_read(RTC_SECONDS);
    uint8_t min=rtc_read(RTC_MINUTES);
    uint8_t hour=rtc_read(RTC_HOURS);
    uint8_t day=rtc_read(RTC_DATE);
    uint8_t month=rtc_read(RTC_MONTH);
    uint8_t year=rtc_read(RTC_YEAR);
    /* Reject oscillator-stop, invalid BCD, and unset calendar fields. */
    if(sec & 0x80U) return 0;
    sec &= 0x7FU;
    min &= 0x7FU;
    hour &= 0x3FU;
    if((sec & 0x0FU)>9U || (sec>>4)>5U || bcd_to_bin(sec)>59U) return 0;
    if((min & 0x0FU)>9U || (min>>4)>5U || bcd_to_bin(min)>59U) return 0;
    if((hour & 0x0FU)>9U || (hour>>4)>2U || bcd_to_bin(hour)>23U) return 0;
    if(bcd_to_bin(day)<1U || bcd_to_bin(day)>31U) return 0;
    if(bcd_to_bin(month)<1U || bcd_to_bin(month)>12U) return 0;
    if((year & 0x0FU)>9U || (year>>4)>9U) return 0;
    return 1;
}
static void init_hw(void) {
    ADCON1=0x0F;
    CMCON=0x07;
    TRISA=0xFF;
    TRISEbits.TRISE0=0;
    BUZZER_LAT=0;
    STATUS_LED_TRIS=0;
    STATUS_LED=0;
    init_clcd();
    init_matrix_keypad();
    init_i2c();
    eeprom24_init();
    /* Preserve valid RTC time; initialize unset RTC to 00:00:00, 01/01/26. */
    if(!rtc_has_valid_time()) {
        dt.sec=0;
        dt.min=0;
        dt.hour=0;
        dt.wday=5;
        dt.day=1;
        dt.month=1;
        dt.year=26;
        rtc_write_all();
    }
    else  {
        /* Clear DS1307 CH (clock-halt) bit and normalize to 24-hour mode. */
        uint8_t s=rtc_read(RTC_SECONDS), h=rtc_read(RTC_HOURS);
        rtc_write(RTC_SECONDS,(uint8_t)(s&0x7FU));
        if(h&0x40U) {
            uint8_t h12=bcd_to_bin((uint8_t)(h&0x1FU));
            uint8_t pm=(h&0x20U)?1U:0U, h24;
            if(h12==12U) h24=pm?12U:0U;
            else h24=(uint8_t)(h12+(pm?12U:0U));
            rtc_write(RTC_HOURS,bin_to_bcd(h24));
        }
    }
    rtc_read_all();
    load_events();
}
static const char *weekday_name(uint8_t d) {
    static const char *n[]= {
        "???","SUN","MON","TUE","WED","THU","FRI","SAT"
    }
    ;
    return n[(d>=1&&d<=7)?d:1];
}
static char type_char(uint8_t t) {
    return t==0?'O':(t==1?'D':'W');
}
static uint8_t event_matches_today(event_t *e) {
    if(!e->enabled)return 0;
    if(e->type==0)return (e->day==dt.day&&e->month==dt.month&&e->year==dt.year);
    if(e->type==1)return 1;
    return e->weekday==dt.wday;
}
static uint8_t event_is_future_today(event_t *e) {
    if(!event_matches_today(e))return 0;
    if(e->hour>dt.hour)return 1;
    if(e->hour==dt.hour&&e->minute>=dt.min)return 1;
    return 0;
}
static int8_t next_event_index(void) {
    uint8_t i;
    int8_t best=-1;
    uint16_t bestmins=65535U;
    for(i=0;i<EVENT_COUNT;i++) {
        event_t *e=&events[i];
        uint16_t mins;
        if(!event_matches_today(e))continue;
        if(!event_is_future_today(e))continue;
        mins=(uint16_t)e->hour*60U+e->minute;
        if(mins<bestmins) {
            bestmins=mins;
            best=(int8_t)i;
        }
    }
    if(best>=0)return best;
    /* no later event today: find earliest recurring event */for(i=0;i<EVENT_COUNT;i++) {
        event_t *e=&events[i];
        uint16_t mins;
        if(!e->enabled||e->type==0)continue;
        if(e->type==2 && e->weekday!=((dt.wday%7U)+1U))continue;
        mins=(uint16_t)e->hour*60U+e->minute;
        if(mins<bestmins) {
            bestmins=mins;
            best=(int8_t)i;
        }
    }
    return best;
}
static void display_default(void)
{
    int8_t ni;
    uint16_t phase = (uint16_t)(tick10 % 500U);
    /* 5-second display cycle */
    /* Clock remains visible at all times. */
    clcd_print("                ",LINE1(0));
    print2(dt.hour,LINE1(0));
    clcd_putch(':',LINE1(2));
    print2(dt.min,LINE1(3));
    clcd_putch(':',LINE1(5));
    print2(dt.sec,LINE1(6));
    /* Requirements: event details appear for 2 seconds every 5 seconds.
     * During the other 3 seconds show the date; clock keeps advancing. */
    clcd_print("                ",LINE2(0));
    if (phase < 200U)  {
        ni=next_event_index();
        if(ni<0)  {
            clcd_print("NO EVENTS SET   ",LINE2(0));
        }
        else  {
            event_t *e=&events[(uint8_t)ni];
            clcd_print("NEXT #",LINE2(0));
            clcd_putch((uint8_t)('1'+ni),LINE2(6));
            clcd_putch(' ',LINE2(7));
            print2(e->hour,LINE2(8));
            clcd_putch(':',LINE2(10));
            print2(e->minute,LINE2(11));
            clcd_putch(type_char(e->type),LINE2(13));
            clcd_putch(e->enabled?'*':'-',LINE2(14));
        }
    }
    else  {
        print2(dt.day,LINE2(0));
        clcd_putch('/',LINE2(2));
        print2(dt.month,LINE2(3));
        clcd_putch('/',LINE2(5));
        print2(dt.year,LINE2(6));
        clcd_print(weekday_name(dt.wday),LINE2(10));
    }
}
static void display_config(void) {
    clcd_print("CONFIG MENU     ",LINE1(0));
    clcd_print(menu_index==0?"> EVENTS/ALARMS ":"> SET TIME/DATE ",LINE2(0));
}
static void display_event_menu(void) {
    clcd_print("EVENTS MENU     ",LINE1(0));
    clcd_print(menu_index==0?"> SET EVENT     ":"> VIEW EVENTS   ",LINE2(0));
}
static void display_set_event(void) {
    event_t *e=&events[event_index];
    static const char *labels[]= {
        "HOUR","MINUTE","DURATION","TYPE O/D/W","WEEKDAY","DAY","MONTH","YEAR","ENABLED"
    }
    ;
    clcd_print("EDIT EVENT ",LINE1(0));
    clcd_putch((uint8_t)('1'+event_index),LINE1(11));
    clcd_print(labels[edit_field],LINE1(0));
    clcd_putch((uint8_t)('1'+event_index),LINE1(15));
    clcd_print("                ",LINE2(0));
    print2(e->hour,LINE2(0));
    clcd_putch(':',LINE2(2));
    print2(e->minute,LINE2(3));
    clcd_print(" D",LINE2(5));
    print2(e->duration,LINE2(7));
    clcd_putch(type_char(e->type),LINE2(9));
    clcd_putch(e->enabled?'*':'-',LINE2(10));
    if(edit_field==4) {
        clcd_print(weekday_name(e->weekday),LINE2(12));
    }
    else if(edit_field==5) {
        print2(e->day,LINE2(12));
    }
    else if(edit_field==6) {
        print2(e->month,LINE2(12));
    }
    else if(edit_field==7) {
        print2(e->year,LINE2(12));
    }
}
static void display_view_event(void) {
    event_t *e=&events[view_index];
    clcd_print("VIEW EVENT ",LINE1(0));
    clcd_putch((uint8_t)('1'+view_index),LINE1(11));
    clcd_putch(type_char(e->type),LINE1(13));
    clcd_print("                ",LINE2(0));
    if(!e->enabled) {
        clcd_print("<EMPTY / DISABLE",LINE2(0));
        return;
    }
    print2(e->hour,LINE2(0));
    clcd_putch(':',LINE2(2));
    print2(e->minute,LINE2(3));
    clcd_print(" D",LINE2(5));
    print2(e->duration,LINE2(7));
    clcd_print(e->type==0?" ONCE ":(e->type==1?" DAILY":" WEEK "),LINE2(10));
}
static void display_set_time(void) {
    clcd_print("SET TIME        ",LINE1(0));
    clcd_print("                ",LINE2(0));
    print2(dt.hour,LINE2(0));
    clcd_putch(':',LINE2(2));
    print2(dt.min,LINE2(3));
    clcd_putch(':',LINE2(5));
    print2(dt.sec,LINE2(6));
    if(edit_field==0)clcd_putch(((tick10/50U)&1U)?' ':'^',LINE2(0));
    if(edit_field==1)clcd_putch(((tick10/50U)&1U)?' ':'^',LINE2(3));
    if(edit_field==2)clcd_putch(((tick10/50U)&1U)?' ':'^',LINE2(6));
}
static void display_set_date(void) {
    clcd_print("SET DATE        ",LINE1(0));
    clcd_print("                ",LINE2(0));
    print2(dt.day,LINE2(0));
    clcd_putch('/',LINE2(2));
    print2(dt.month,LINE2(3));
    clcd_putch('/',LINE2(5));
    print2(dt.year,LINE2(6));
    clcd_print(weekday_name(dt.wday),LINE2(10));
    if(edit_field==0)clcd_putch(((tick10/50U)&1U)?' ':'^',LINE2(0));
    if(edit_field==1)clcd_putch(((tick10/50U)&1U)?' ':'^',LINE2(3));
    if(edit_field==2)clcd_putch(((tick10/50U)&1U)?' ':'^',LINE2(6));
}
static void display_screen(void) {
    switch(screen) {
        case SCR_DEFAULT:display_default();
        break;
        case SCR_CONFIG:display_config();
        break;
        case SCR_EVENT_MENU:display_event_menu();
        break;
        case SCR_SET_EVENT:display_set_event();
        break;
        case SCR_VIEW_EVENT:display_view_event();
        break;
        case SCR_SET_TIME:display_set_time();
        break;
        case SCR_SET_DATE:display_set_date();
        break;
    }
}
static void stop_alarm(void) {
    ring_active=0;
    ring_event=0xFF;
    BUZZER_LAT=0;
    STATUS_LED=0;
}
static void check_events(void) {
    uint8_t i;
    for(i=0;i<EVENT_COUNT;i++) {
        event_t *e=&events[i];
        if(!event_matches_today(e)||e->hour!=dt.hour||e->minute!=dt.min||dt.sec>1U)continue;
        if(last_trigger_min==dt.min&&last_trigger_day==dt.day&&last_trigger_month==dt.month&&last_trigger_year==dt.year)continue;
        last_trigger_min=dt.min;
        last_trigger_day=dt.day;
        last_trigger_month=dt.month;
        last_trigger_year=dt.year;
        ring_active=1;
        ring_silenced=0;
        ring_event=i;
        ring_ticks=(uint16_t)e->duration*6000U;
        if(e->type==0) {
            e->enabled=0;
            save_events();
        }
        break;
    }
}
static void inc_event_field(int8_t delta) {
    event_t *e=&events[event_index];
    switch(edit_field) {
        case 0:e->hour=(uint8_t)((e->hour+(delta>0?1U:23U))%24U);
        break;
        case 1:e->minute=(uint8_t)((e->minute+(delta>0?1U:59U))%60U);
        break;
        case 2:if(delta>0)e->duration=(uint8_t)(e->duration>=99?1:e->duration+1U);
        else e->duration=(uint8_t)(e->duration<=1?99:e->duration-1U);
        break;
        case 3:e->type=(uint8_t)((e->type+(delta>0?1U:2U))%3U);
        break;
        case 4:e->weekday=(uint8_t)((e->weekday-1U+(delta>0?1U:6U))%7U+1U);
        break;
        case 5: {
            uint8_t max=days_in_month(e->month,e->year);
            if(delta>0)e->day=(e->day>=max)?1:(uint8_t)(e->day+1U);
            else e->day=(e->day<=1)?max:(uint8_t)(e->day-1U);
            break;
        }
        case 6:e->month=(uint8_t)((e->month-1U+(delta>0?1U:11U))%12U+1U);
        if(e->day>days_in_month(e->month,e->year))e->day=days_in_month(e->month,e->year);
        break;
        case 7:e->year=(uint8_t)((e->year+(delta>0?1U:99U))%100U);
        if(e->day>days_in_month(e->month,e->year))e->day=days_in_month(e->month,e->year);
        break;
        case 8:e->enabled=(uint8_t)!e->enabled;
        break;
    }
}
static void inc_datetime_field(int8_t delta,uint8_t date_mode) {
    if(!date_mode) {
        if(edit_field==0)dt.hour=(uint8_t)((dt.hour+(delta>0?1U:23U))%24U);
        else if(edit_field==1)dt.min=(uint8_t)((dt.min+(delta>0?1U:59U))%60U);
        else dt.sec=0;
    }
    else {
        if(edit_field==0) {
            uint8_t max=days_in_month(dt.month,dt.year);
            if(delta>0)dt.day=(dt.day>=max)?1:(uint8_t)(dt.day+1U);
            else dt.day=(dt.day<=1)?max:(uint8_t)(dt.day-1U);
            dt.wday=(uint8_t)(((dt.wday-1U+(delta>0?1U:6U))%7U)+1U);
        }
        else if(edit_field==1)dt.month=(uint8_t)((dt.month-1U+(delta>0?1U:11U))%12U+1U);
        else dt.year=(uint8_t)((dt.year+(delta>0?1U:99U))%100U);
        if(dt.day>days_in_month(dt.month,dt.year))dt.day=days_in_month(dt.month,dt.year);
    }
}
static void short_key(uint8_t key) {
    idle_ticks=0;
    if(screen==SCR_DEFAULT) {
        if(key==KEY_UP||key==KEY_DOWN) {
            screen=SCR_CONFIG;
            menu_index=0;
        }
        return;
    }
    if(screen==SCR_CONFIG) {
        if(key==KEY_UP||key==KEY_DOWN)menu_index^=1U;
        return;
    }
    if(screen==SCR_EVENT_MENU) {
        if(key==KEY_UP||key==KEY_DOWN)menu_index^=1U;
        return;
    }
    if(screen==SCR_SET_EVENT) {
        if(key==KEY_LEFT)edit_field=(uint8_t)((edit_field+8U)%9U);
        else if(key==KEY_RIGHT)edit_field=(uint8_t)((edit_field+1U)%9U);
        else if(key==KEY_UP)inc_event_field(1);
        else if(key==KEY_DOWN)inc_event_field(-1);
        else if(key==5U)event_index=(uint8_t)((event_index+EVENT_COUNT-1U)%EVENT_COUNT);
        else if(key==6U)event_index=(uint8_t)((event_index+1U)%EVENT_COUNT);
        return;
    }
    if(screen==SCR_VIEW_EVENT) {
        if(key==KEY_UP)view_index=(uint8_t)((view_index+1U)%EVENT_COUNT);
        else if(key==KEY_DOWN)view_index=(uint8_t)((view_index+EVENT_COUNT-1U)%EVENT_COUNT);
        else if(key==KEY_LEFT) {
            events[view_index].enabled=0;
            save_events();
        }
        return;
    }
    if(screen==SCR_SET_TIME) {
        if(key==KEY_LEFT)edit_field=(uint8_t)((edit_field+2U)%3U);
        else if(key==KEY_RIGHT)edit_field=(uint8_t)((edit_field+1U)%3U);
        else if(key==KEY_UP)inc_datetime_field(1,0);
        else if(key==KEY_DOWN)inc_datetime_field(-1,0);
        return;
    }
    if(screen==SCR_SET_DATE) {
        if(key==KEY_LEFT)edit_field=(uint8_t)((edit_field+2U)%3U);
        else if(key==KEY_RIGHT)edit_field=(uint8_t)((edit_field+1U)%3U);
        else if(key==KEY_UP)inc_datetime_field(1,1);
        else if(key==KEY_DOWN)inc_datetime_field(-1,1);
    }
}
static void long_key(uint8_t key) {
    idle_ticks=0;
    if(key==KEY_DOWN) {
        if(screen==SCR_DEFAULT)return;
        if(screen==SCR_CONFIG||screen==SCR_EVENT_MENU)screen=(screen==SCR_CONFIG)?SCR_DEFAULT:SCR_CONFIG;
        else if(screen==SCR_SET_EVENT||screen==SCR_VIEW_EVENT)screen=SCR_EVENT_MENU;
        else if(screen==SCR_SET_TIME||screen==SCR_SET_DATE)screen=SCR_CONFIG;
        return;
    }
    if(key==KEY_RIGHT && screen==SCR_VIEW_EVENT) {
        screen=SCR_EVENT_MENU;
        menu_index=1;
        return;
    }
    if(key!=KEY_UP)return;
    switch(screen) {
        case SCR_DEFAULT:screen=SCR_CONFIG;
        menu_index=0;
        break;
        case SCR_CONFIG:if(menu_index==0) {
            screen=SCR_EVENT_MENU;
            menu_index=0;
        }
        else {
            rtc_read_all();
            screen=SCR_SET_TIME;
            edit_field=0;
        }
        break;
        case SCR_EVENT_MENU:if(menu_index==0) {
            screen=SCR_SET_EVENT;
            edit_field=0;
        }
        else {
            screen=SCR_VIEW_EVENT;
            view_index=0;
        }
        break;
        case SCR_SET_EVENT:save_events();
        screen=SCR_EVENT_MENU;
        menu_index=0;
        break;
        case SCR_VIEW_EVENT:screen=SCR_EVENT_MENU;
        menu_index=1;
        break;
        case SCR_SET_TIME:rtc_write_all();
        screen=SCR_SET_DATE;
        edit_field=0;
        break;
        case SCR_SET_DATE:rtc_write_all();
        screen=SCR_CONFIG;
        menu_index=1;
        break;
    }
}
static uint8_t get_key(void) {
    uint8_t k=read_switches(LEVEL_CHANGE);
    if(k==0xFFU)return KEY_NONE;
    /* SW1..SW4 are directions; SW5/SW6 select event slots while editing. */if(k>=1U&&k<=6U)return k;
    return KEY_NONE;
}
static void process_key(void) {
    uint8_t k=get_key();
    if(k==KEY_NONE) {
        if(previous_key!=KEY_NONE) {
            if(!key_long_sent)short_key(previous_key);
            previous_key=KEY_NONE;
            key_hold_ticks=0;
            key_long_sent=0;
        }
        return;
    }
    if(k!=previous_key) {
        previous_key=k;
        key_hold_ticks=0;
        key_long_sent=0;
        if(ring_active)stop_alarm();
        return;
    }
    if(key_hold_ticks<65535U)key_hold_ticks++;
    if(!key_long_sent&&key_hold_ticks>=LONG_PRESS_TICKS) {
        long_key(k);
        key_long_sent=1;
    }
}
void main(void)
{
    init_hw();
    tick10=0;
    /* Show clock immediately after initialization; no long startup splash. */
    display_screen();
    while(1) {
        process_key();
        if(idle_ticks<65535U) idle_ticks++;
        if(screen!=SCR_DEFAULT && idle_ticks>IDLE_LIMIT_TICKS) {
            screen=SCR_DEFAULT;
            idle_ticks=0;
        }
        /* Advance the displayed clock every second from the 10 ms task tick.
         * This avoids a frozen display if the DS1307 oscillator is stopped.
         * The RTC is initialized/updated when time/date is set by the user. */
        if((tick10%100U)==0U) {
            if(tick10!=0U) advance_clock_one_second();
            check_events();
            if(screen==SCR_DEFAULT) display_default();
            else display_screen();
        }
        if(ring_active) {
            if(!ring_silenced && (tick10%50U)==0U) {
                BUZZER_LAT=(uint8_t)!BUZZER_LAT;
                STATUS_LED=BUZZER_LAT;
            }
            if(ring_ticks>0) ring_ticks--;
            else stop_alarm();
        }
        tick10++;
        __delay_ms(10);
    }
}
