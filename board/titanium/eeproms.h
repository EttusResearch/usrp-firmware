#pragma once

enum {
       TLV_EEPROM_MB,
       TLV_EEPROM_DB0,
       TLV_EEPROM_DB1,
       TLV_EEPROM_PWR,

       TLV_EEPROM_LAST,
};

const void *eeprom_lookup_tag(int which, uint8_t tag);
