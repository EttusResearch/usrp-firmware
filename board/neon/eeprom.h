/* Copyright (c) 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef EEPROM_H
#define EEPROM_H

#define ETH_ALEN 6

struct usrp_neon_eeprom {
	uint32_t magic;
	uint32_t version;
	uint32_t mcu_flags[4];
	uint16_t pid;
	uint16_t rev;
	uint8_t serial[8];
	uint8_t eth_addr0[ETH_ALEN];
	uint16_t dt_compat;
	uint8_t eth_addr1[ETH_ALEN];
	uint16_t mcu_compat;
	uint8_t eth_addr2[ETH_ALEN];
	uint16_t rev_compat;
	uint32_t crc;
} __packed;

int eeprom_get_mcu_flags(uint32_t **mcu_flags);

int eeprom_get_autoboot(void);

int eeprom_get_enclosure(void);

int eeprom_get_board_rev(void);

int eeprom_get_fan_min(int fan);

int eeprom_get_fan_max(int fan);

#endif /* EEPROM_H */
