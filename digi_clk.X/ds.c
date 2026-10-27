#include <xc.h>
#include "i2c.h"
#include "ds.h"
void rtc_write(unsigned char reg, unsigned char value)
{
    i2c_start();
    i2c_write(DS1307_WRITE);
    i2c_write(reg);
    i2c_write(value);
    i2c_stop();
}
unsigned char rtc_read(unsigned char reg)
{
    unsigned char value;
    i2c_start();
    i2c_write(DS1307_WRITE);
    i2c_write(reg);
    i2c_rep_start();
    i2c_write(DS1307_READ);
    value = i2c_read();
    i2c_stop();
    return value;
}
unsigned char bcd_to_bin(unsigned char value)
{
    return (unsigned char)(((value >> 4) * 10U) + (value & 0x0FU));
}
unsigned char bin_to_bcd(unsigned char value)
{
    return (unsigned char)(((value / 10U) << 4) | (value % 10U));
}
