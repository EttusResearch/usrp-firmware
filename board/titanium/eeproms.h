#pragma once

enum {
       TLV_EEPROM_MB,
       TLV_EEPROM_DB0,
       TLV_EEPROM_DB1,
       TLV_EEPROM_PWR,

       TLV_EEPROM_LAST,
};

const void *eeprom_lookup_tag(int which, uint8_t tag);

/*
 * All boards on x4xx are expected to have an EEPROM populated with at least the
 * board_info tag; if this is not present, we assume that there is no board in
 * the slot.
 */
int is_board_present(int which);
