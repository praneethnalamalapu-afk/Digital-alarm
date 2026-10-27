#ifndef EEPROM24_H
#define EEPROM24_H
#include <stdint.h>
void eeprom24_init(void);
uint8_t eeprom24_read(uint8_t address);
void eeprom24_write(uint8_t address, uint8_t value);
#endif
