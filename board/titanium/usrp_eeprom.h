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

#define USRP_EEPROM_MCU_FLAGS (0x20)
#define MCU_FLAGS_AUTOBOOT(flags_) ((flags_)->flags[0] & 0x1)
#define MCU_FLAGS_DISABLE_THERMAL_RECOVERY(flags_) ((flags_)->flags[0] & 0x2)
#define MCU_FLAGS_BOOTMODE(flags_) ((flags_)->flags[1] & 0xF)
struct usrp_eeprom_mcu_flags {
	uint8_t flags[6];
} __attribute__((packed));

#define USRP_EEPROM_FAN_LIMITS (0x21)
struct usrp_eeprom_fan_limits {
	uint16_t min;
	uint16_t start;
	uint16_t max;
} __attribute__((packed));

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
	case USRP_EEPROM_MCU_FLAGS:
	{
		const struct usrp_eeprom_mcu_flags *v = val;
		ccprintf("%s (0x%02x) ", "usrp_eeprom_mcu_flags", tag);
		for (i = 0; i < 6; i++)
			ccprintf("0x%02x ", v->flags[i]);
		ccprintf("\n");
	}
	break;
	case USRP_EEPROM_FAN_LIMITS:
	{
		const struct usrp_eeprom_fan_limits *v = val;
		ccprintf("%s (0x%02x) ", "usrp_eeprom_fan_limits", tag);
		ccprintf("min: %d, start: %d, max: %d", v->min, v->start, v->max);
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
