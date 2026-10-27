/* 
 * File:   m.h
 * Author: prane
 *
 * Created on 26 October 2026, 12:41
 */
#ifndef MAIN_H
#define MAIN_H
#include <xc.h>
#define _XTAL_FREQ 20000000UL
/* RhydoLABZ / training-board assumptions:
 * CLCD: data bus PORTD, RS=RC1, RW=RC0, EN=RC2
 * I2C RTC DS1307: SCL=RC3, SDA=RC4
 * Matrix keypad: existing 3-row x 4-column driver on PORTB
 * Buzzer: active buzzer on RA5 (active HIGH)
 * Status LED: RD0 (active HIGH)
 */
// #define BUZZER_LAT      LATAbits.LATA5
#define BUZZER_LAT      RE0
#define BUZZER_TRIS     TRISE0
// #define BUZZER_TRIS     TRISAbits.TRISA5
#define STATUS_LED      LATDbits.LATD0
#define STATUS_LED_TRIS TRISDbits.TRISD0
#endif
