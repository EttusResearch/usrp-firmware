
#include "common.h"
#include "console.h"
#include "usrp_eeprom.h"
#include "eeproms.h"
#include "zynqmp.h"

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
	uint8_t bootmode = zynqmp_str_to_bootmode("emmc");
	if (mcu_flags_present())
		bootmode = MCU_FLAGS_BOOTMODE(mcu_flags);

	ccprintf("bootmode flag: 0x%x (%s)\n", bootmode,
		 zynqmp_bootmode_to_str(bootmode));
	return bootmode;
}

uint8_t mcu_flags_get_autoboot(void)
{
	uint8_t autoboot = 0;
	if (mcu_flags_present())
		autoboot = MCU_FLAGS_AUTOBOOT(mcu_flags);

	ccprintf("autoboot flag: %u\n", autoboot);
	return autoboot;
}

uint8_t mcu_flags_get_thermal_recovery(void)
{
	if (mcu_flags_present())
		return !MCU_FLAGS_DISABLE_THERMAL_RECOVERY(mcu_flags);

	/* Enable thermal recovery by default */
	return 1;
}
