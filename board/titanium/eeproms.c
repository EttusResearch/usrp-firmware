#include "common.h"
#include "console.h"
#include "eeprom.h"
#include "endian.h"
#include "board.h"
#include "util.h"
#include "i2c.h"
#include "crc.h"
#include "tlv_eeprom.h"
#include "usrp_eeprom.h"
#include "eeproms.h"

#ifdef DEBUG
#define debug(...) ccprintf(...)
#else
#define debug(...)
#endif

enum eeprom_state {
	EEPROM_STATE_UNINIT,
	EEPROM_STATE_MISSING,
	EEPROM_STATE_INVALID,
	EEPROM_STATE_VALID,
};

struct eeprom_info {
	const char *name;
	int port;
	struct tlv_eeprom contents;
	uint8_t state;
};

static struct eeprom_info eeproms[] = {
	[TLV_EEPROM_MB] = { "mb",  I2C_PORT_RTC },
	[TLV_EEPROM_DB0] = { "db0", I2C_PORT_DB0 },
	[TLV_EEPROM_DB1] = { "db1", I2C_PORT_DB1 },
	[TLV_EEPROM_PWR] = { "pwr", I2C_PORT_PWR },
};

uint32_t tlv_eeprom_crc(const struct tlv_eeprom *eeprom)
{
	const uint8_t *p;
	uint32_t crc;
	size_t size;

	size = sizeof(eeprom->size) + eeprom->size;
	p = (const uint8_t *)&eeprom->size;
	crc = 0;

	while (size--)
		crc32_ctx_hash8(&crc, *p++);

	return crc;
}

static int tlv_eeprom_read(int port, struct tlv_eeprom *eeprom)
{
	uint8_t *eeprom_ptr;
	int i, tmp, err;

	eeprom_ptr = (uint8_t *)eeprom;

	for (i = 0; i < sizeof(struct tlv_eeprom); i++) {
		err = i2c_read8(port, 0x50, i, &tmp);
		if (err != EC_RES_SUCCESS)
			return -1;
		eeprom_ptr[i] = (uint8_t)tmp;
	}

	return 0;
}

static void load_eeprom(struct eeprom_info *eeprom)
{
	if (eeprom->state == EEPROM_STATE_VALID) {
		ccprintf("cache hit %s\n", eeprom->name);
		return;
	}

	if (tlv_eeprom_read(eeprom->port, &eeprom->contents)) {
		eeprom->state = EEPROM_STATE_MISSING;
		goto out;
	}

	if (tlv_eeprom_validate(&eeprom->contents, USRP_EEPROM_MAGIC) == 0)
		eeprom->state = EEPROM_STATE_VALID;
	else
		eeprom->state = EEPROM_STATE_INVALID;
out:
	ccprintf("%s eeprom state: %s\n", eeprom->name,
	      eeprom->state == EEPROM_STATE_VALID ? "valid" :
	      eeprom->state == EEPROM_STATE_MISSING ? "missing" :
	      eeprom->state == EEPROM_STATE_INVALID ? "invalid" : "uninit");
}

static void eeprom_dump_raw(const struct tlv_eeprom *eeprom)
{
	uint8_t *ptr;
	int i;

	ptr = (uint8_t *)eeprom;

	for (i = 0; i < sizeof(*eeprom); i++) {
		if ((i % 16) == 0)
			ccprintf("%s%02x:", i == 0 ? "" : "\n", i);
		if ((i % 8) == 0)
			ccprintf(" ");
		ccprintf("%02x ", ptr[i]);
	}
	ccprintf("\n");
}

static void eeprom_dump(const struct tlv_eeprom *eeprom)
{
	tlv_for_each(eeprom->tlv, eeprom->size, usrp_eeprom_trace);
}

static int command_eepromdump(int argc, char **argv)
{
	struct eeprom_info *eeprom;
	int raw = 0;
	size_t i;

	if (argc != 2 && argc != 3)
		return EC_ERROR_PARAM_COUNT;

	if (argc == 3) {
		if (!strcasecmp(argv[2], "raw"))
			raw = 1;
		else
			return EC_ERROR_PARAM2;
	}

	for (i = 0; i < ARRAY_SIZE(eeproms); i++)  {
		eeprom = eeproms + i;
		load_eeprom(eeprom);

		if (!strcasecmp(argv[1], eeprom->name)) {
			switch (eeprom->state) {
			case EEPROM_STATE_INVALID:
				ccprintf("warning: eeprom contents invalid, raw dump:\n");
				raw = 1;
			case EEPROM_STATE_VALID: /* fall through */
				if (raw)
					eeprom_dump_raw(&eeprom->contents);
				else
					eeprom_dump(&eeprom->contents);
				break;
			case EEPROM_STATE_MISSING:
				ccprintf("eeprom not present\n");
				break;
			}

			break;
		}
	}

	if (i == ARRAY_SIZE(eeproms))
		return EC_ERROR_PARAM1;

	return 0;
}
DECLARE_CONSOLE_COMMAND(eepromdump, command_eepromdump,
			"<mb/db0/db1/pwr> [raw]",
			"dump contents of eeprom");


const void *eeprom_lookup_tag(int which, uint8_t tag)
{
	struct eeprom_info *eeprom;

	if (which >= TLV_EEPROM_LAST)
		return NULL;

	eeprom = eeproms + which;
	load_eeprom(eeprom);
	if (eeprom->state != EEPROM_STATE_VALID)
		return NULL;

	return tlv_lookup(eeprom->contents.tlv, eeprom->contents.size, tag);
}

int is_board_present(int which)
{
	return eeprom_lookup_tag(which, USRP_EEPROM_BOARD_INFO_TAG) != NULL;
}
