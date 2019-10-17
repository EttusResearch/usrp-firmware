#pragma once

#include <stdint.h>

#define USRP_EEPROM_MAGIC 0x55535250

#define USRP_EEPROM_BOARD_INFO_TAG (0x10)
struct usrp_eeprom_board_info {
	uint16_t pid;
	uint16_t rev;
	uint16_t compat_rev;
	char serial[8];
} __attribute__((packed));

#define USRP_EEPROM_CCA_INFO_TAG (0x11)
struct usrp_eeprom_cca_info {
	uint16_t pid;
	uint16_t rev;
	char serial[8];
} __attribute__((packed));

#define USRP_EEPROM_ETH0_ADDR_TAG  (0xA0)
#define USRP_EEPROM_QSFP0_ADDR_TAG (0xA1)
#define USRP_EEPROM_QSFP1_ADDR_TAG (0xA2)
struct usrp_eeprom_mac_addr {
	uint8_t addr[6];
} __attribute__((packed));

#define USRP_EEPROM_DB_PWR_SEQ_TAG (0x12)
struct usrp_eeprom_db_pwr_seq {
	uint8_t nsteps;
	struct {
		uint16_t delay;
		uint8_t supply_mask;
	} steps[8];
};


//#include <stdio.h>
#include <assert.h>

static void __maybe_unused usrp_eeprom_trace(uint8_t tag, uint8_t len, const void *val)
{
	uint8_t i;

	switch (tag) {
	case USRP_EEPROM_BOARD_INFO_TAG:
	{
		const struct usrp_eeprom_board_info *v = val;
		assert(sizeof(*v) == len);
		ccprintf("%s (0x%02x) ", "usrp_eeprom_board_info", tag);
		ccprintf("pid: 0x%04x, rev: 0x%04x, compat_rev: 0x%04x, serial: %s\n",
		       v->pid, v->rev, v->compat_rev, v->serial);
	}
	break;
	case USRP_EEPROM_CCA_INFO_TAG:
	{
		const struct usrp_eeprom_cca_info *v = val;
		assert(sizeof(*v) == len);
		ccprintf("%s (0x%02x) ", "usrp_eeprom_cca_info", tag);
		ccprintf("pid: 0x%04x, rev: 0x%04x, serial: %s\n",
		       v->pid, v->rev, v->serial);
	}
	break;
	case USRP_EEPROM_ETH0_ADDR_TAG:
	case USRP_EEPROM_QSFP0_ADDR_TAG:
	case USRP_EEPROM_QSFP1_ADDR_TAG:
	{
		const struct usrp_eeprom_mac_addr *v = val;
		static const char *macs[] = {"eth0", "qsfp0", "qsfp1"};
		assert(sizeof(*v) == len);
		ccprintf("%s %s (0x%02x) ", "usrp_eeprom_mac_addr",
		       macs[tag - USRP_EEPROM_ETH0_ADDR_TAG], tag);
		for (i = 0; i < 6; i++)
			ccprintf("%02x%c", v->addr[i], i == 5 ? ' ' : ':');
		ccprintf("\n");
	}
	break;
	case USRP_EEPROM_DB_PWR_SEQ_TAG:
	{
		const struct usrp_eeprom_db_pwr_seq *v = val;
		assert(sizeof(*v) == len);
		ccprintf("%s (%02x) ", "usrp_eeprom_db_pwr_seq", tag);
		for (i = 0; i < 8; i++)
			ccprintf("(%u, %02x) ", v->steps[i].delay, v->steps[i].supply_mask);
		ccprintf("\n");
	}
	break;
	default:
	{
		const uint8_t *ptr = val;
		ccprintf("%s (0x%02x) len: %hhu, val: ", "unknown", tag, len);
		for (i = 0; i < len; i++)
			ccprintf("%02x ", ptr[i]);
		ccprintf("\n");
		break;
	}
	}
}