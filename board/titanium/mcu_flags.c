
#include "common.h"
#include "console.h"
#include "usrp_eeprom.h"
#include "eeproms.h"

static const struct usrp_eeprom_mcu_flags *mcu_flags;

static int mcu_flags_present(void)
{
	static uint8_t missing;

	if (mcu_flags || missing)
		return !missing;

	mcu_flags = eeprom_lookup_tag(TLV_EEPROM_MB, USRP_EEPROM_MCU_FLAGS);
	if (!mcu_flags)  {
		ccprintf("mcu_flags missing from eeprom; using defaults\n");
		missing = 1;
	}

	return !missing;
}

uint8_t mcu_flags_get_bootmode(void)
{
	if (mcu_flags_present())
		return MCU_FLAGS_BOOTMODE(mcu_flags);

	return 0b1110; /* SD1LS by default */
}

uint8_t mcu_flags_get_autoboot(void)
{
	if (mcu_flags_present())
		return MCU_FLAGS_AUTOBOOT(mcu_flags);

	return 0;
}

uint8_t mcu_flags_get_thermal_recovery(void)
{
	if (mcu_flags_present())
		return !MCU_FLAGS_DISABLE_THERMAL_RECOVERY(mcu_flags);

	/* Enable thermal recovery by default */
	return 1;
}
