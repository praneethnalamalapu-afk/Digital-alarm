#include <xc.h>
#include "m.h"
#include "i2c.h"
/* PIC18F4580 MSSP master, Fosc=20 MHz, SCL approximately 100 kHz. */
static void wait_ssp(void)
{
    unsigned int timeout = 60000U;
    while (!SSPIF && timeout != 0U) timeout--;
    SSPIF = 0;
}
void init_i2c(void)
{
    TRISCbits.TRISC3 = 1;
    TRISCbits.TRISC4 = 1;
    SSPCON1 = 0x28;
    /* I2C Master mode */
    SSPCON2 = 0x00;
    SSPADD = 49;
    /* 100 kHz at 20 MHz: Fosc/(4*(SSPADD+1)) */
    CKE = 0;
    SMP = 1;
    SSPIF = 0;
    BCLIF = 0;
}
void i2c_start(void)  {
    SSPIF = 0;
    SEN = 1;
    wait_ssp();
}
void i2c_rep_start(void)  {
    SSPIF = 0;
    RSEN = 1;
    wait_ssp();
}
void i2c_stop(void)  {
    SSPIF = 0;
    PEN = 1;
    wait_ssp();
}
void i2c_write(unsigned char data)
{
    SSPIF = 0;
    SSPBUF = data;
    wait_ssp();
    /* ACKSTAT is intentionally exposed for debugging; a missing RTC
       will read as invalid data, rather than silently being "fixed". */
}
unsigned char i2c_read(void)
{
    unsigned char data;
    SSPIF = 0;
    RCEN = 1;
    wait_ssp();
    data = SSPBUF;
    ACKDT = 1;
    /* NACK: final byte */
    SSPIF = 0;
    ACKEN = 1;
    wait_ssp();
    return data;
}
