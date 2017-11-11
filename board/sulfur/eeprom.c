/* Copyright (c) 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "console.h"
#include "eeprom.h"
#include "endian.h"
#include "board.h"
#include "hooks.h"
#include "util.h"
#include "i2c.h"

#define htons htobe16
#define htonl htobe32

#define ntohs htons
#define ntohl htonl

#define EEPROM_AUTOBOOT_FLAG (1 << 0)

#define DEFAULT_FAN_MIN 3800
#define DEFAULT_FAN_MAX 13000

#define FAN_GET_MIN(x) ((x) & 0xff) * 100
#define FAN_GET_MAX(x) (((x) >> 8) & 0xff) * 100

static const uint32_t USRP_EEPROM_MAGIC = 0xF008AD10;

static struct usrp_sulfur_eeprom eeprom;

static int eeprom_check_initialized(void)
{
	/* this has to be good enough for now ... */
	if (htonl(eeprom.magic) == USRP_EEPROM_MAGIC)
		return 0;

	return -1;
}

static int command_eeprom_info(int argc, char **argv)
{
	if (eeprom_check_initialized()) {
		ccprintf("EEPROM not initialized\n");
		return EC_ERROR_UNKNOWN;
	}

	ccprintf("Serial:\t\t%s\n", eeprom.serial);
	ccprintf("Pid/Rev:\t%04x,Rev%u\n", ntohs(eeprom.pid),
		 ntohs(eeprom.rev)+1);
	ccprintf("MCU flags:\t%08x\n\t\t%08x\n\t\t%08x\n\t\t%08x\n",
		 eeprom.mcu_flags[0], eeprom.mcu_flags[1],
		 eeprom.mcu_flags[2], eeprom.mcu_flags[3]);
	ccprintf("Eth0 Addr:\t%02x:%02x:%02x:%02x:%02x:%02x\n",
		 eeprom.eth_addr0[0], eeprom.eth_addr0[1],
		 eeprom.eth_addr0[2], eeprom.eth_addr0[3],
		 eeprom.eth_addr0[4], eeprom.eth_addr0[5]);
	ccprintf("Eth1 Addr:\t%02x:%02x:%02x:%02x:%02x:%02x\n",
		 eeprom.eth_addr1[0], eeprom.eth_addr1[1],
		 eeprom.eth_addr1[2], eeprom.eth_addr1[3],
		 eeprom.eth_addr1[4], eeprom.eth_addr1[5]);
	ccprintf("Eth2 Addr:\t%02x:%02x:%02x:%02x:%02x:%02x\n",
		 eeprom.eth_addr2[0], eeprom.eth_addr2[1],
		 eeprom.eth_addr2[2], eeprom.eth_addr2[3],
		 eeprom.eth_addr2[4], eeprom.eth_addr2[5]);
	ccprintf("Fan0\t\tmin:%u RPM\tmax:%u RPM\n",
		 FAN_GET_MIN(htonl(eeprom.mcu_flags[1]) & 0xffff),
		 FAN_GET_MAX(htonl(eeprom.mcu_flags[1]) & 0xffff));
	ccprintf("Fan1\t\tmin:%u RPM\tmax:%u RPM\n",
		 FAN_GET_MIN((htonl(eeprom.mcu_flags[1]) >> 16) & 0xffff),
		 FAN_GET_MAX((htonl(eeprom.mcu_flags[1]) >> 16) & 0xffff));

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(eeinfo, command_eeprom_info,
			NULL, "Print EEPROM info");

int eeprom_get_mcu_flags(uint32_t **mcu_flags)
{
	int ret;

	ret = eeprom_check_initialized();
	if (ret)
		return ret;

	*mcu_flags = eeprom.mcu_flags;

	return 0;
}

int eeprom_get_autoboot(void)
{
	int ret;

	/* if not initialized, don't autoboot */
	ret = eeprom_check_initialized();
	if (ret)
		return 0;

	return ntohl(eeprom.mcu_flags[0]) & EEPROM_AUTOBOOT_FLAG;
}

int eeprom_get_fan_min(int fan)
{
	int ret;
	uint32_t flags;

	/* if not initialized, return sane default */
	ret = eeprom_check_initialized();
	if (ret)
		return DEFAULT_FAN_MIN;

	flags = ntohl(eeprom.mcu_flags[1]);

	if (fan && fan < FAN_CH_COUNT)
		flags >>= 16;

	if (!flags)
		return DEFAULT_FAN_MIN;

	return FAN_GET_MIN(flags);
}

int eeprom_get_fan_max(int fan)
{
	int ret;
	uint32_t flags;

	/* if not initialized, return sane default */
	ret = eeprom_check_initialized();
	if (ret)
		return DEFAULT_FAN_MAX;

	flags = ntohl(eeprom.mcu_flags[1]);

	if (fan && fan < FAN_CH_COUNT)
		flags >>= 16;

	if (!flags)
		return DEFAULT_FAN_MAX;

	return FAN_GET_MAX(flags);
}

int eeprom_get_board_rev(void)
{
	int ret;

	ret = eeprom_check_initialized();
	if (ret)
		return -1;

	return ntohs(eeprom.rev);
}

void eeprom_init(void)
{
	int i, tmp, err;
	uint8_t *eeprom_ptr = (uint8_t *)&eeprom;

	for (i = 0; i < sizeof(eeprom); i++) {
		err = i2c_read8(I2C_PORT_MASTER, 0xa0, i, &tmp);
		if (err != EC_RES_SUCCESS)
			return;
		*eeprom_ptr++ = (uint8_t)tmp;
	}

	return;
}
DECLARE_HOOK(HOOK_INIT, eeprom_init, HOOK_PRIO_INIT_I2C + 1);
