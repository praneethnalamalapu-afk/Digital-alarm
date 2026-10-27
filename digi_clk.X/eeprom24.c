#include <xc.h>
#include "i2c.h"
#include "m.h"
#include "eeprom24.h"
#define AT24C02_WRITE 0xA0U
#define AT24C02_READ  0xA1U
void eeprom24_init(void)  {
    /* Uses the shared PIC MSSP I2C peripheral. */
}
uint8_t eeprom24_read(uint8_t address)  {
    uint8_t value;
    i2c_start();
    i2c_write(AT24C02_WRITE);
    i2c_write(address);
    i2c_rep_start();
    i2c_write(AT24C02_READ);
    value=i2c_read();
    i2c_stop();
    return value;
}
void eeprom24_write(uint8_t address, uint8_t value)  {
    i2c_start();
    i2c_write(AT24C02_WRITE);
    i2c_write(address);
    i2c_write(value);
    i2c_stop();
    __delay_ms(6);
    /* AT24C02 internal write cycle; use address 0x50 with A2:A0 grounded. */
}
