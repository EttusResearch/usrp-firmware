/* Copyright (c) 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* ZynqMP chipset power control module for Chrome EC */

#include "adc.h"
#include "board_power.h"
#include "charge_state.h"
#include "chipset.h"
#include "common.h"
#include "console.h"
#include "ec_commands.h"
#include "gpio.h"
#include "hooks.h"
#include "ioexpander.h"
#include "lid_switch.h"
#include "pmbus.h"
#include "power.h"
#include "power_button.h"
#include "system.h"
#include "task.h"
#include "timer.h"
#include "util.h"
#include "pwrsup.h"
#include "mcu_flags.h"

/* Long power key press to force shutdown in S0 */
#define FORCED_SHUTDOWN_DELAY  (8 * SECOND)

/* Console output macros */
#define CPUTS(outstr) cputs(CC_CHIPSET, outstr)
#define CPRINTS(format, args...) cprints(CC_CHIPSET, format, ## args)

#define IN_S0_PWR_REQUIRED (POWER_SIGNAL_MASK(PS_PWR_REQUIRED))

/* ZynqMP bootmode; default set in power_chipset_init */
static uint8_t bootmode;

static const struct pwrsup_seq s3s0_ps_seq[] = {
	{ POWER_SUPPLY_0V85,		5 },
	{ POWER_SUPPLY_1V8,		5 },
	{ POWER_SUPPLY_2V5,		5 },
	{ POWER_SUPPLY_3V3,		5 },
	{ POWER_SUPPLY_0V9,		0 },
	{ POWER_SUPPLY_MGTAUX,		5 },
	{ POWER_SUPPLY_DDR4N_VDDQ,	5 },
	{ POWER_SUPPLY_DDR4N_VTT,	5 },
	{ POWER_SUPPLY_DDR4S_VDDQ,	5 },
	{ POWER_SUPPLY_DDR4S_VTT,	5 },
	{ POWER_SUPPLY_3V6,		5 },
	{ POWER_SUPPLY_3V3CLK,		5 },
	{ POWER_SUPPLY_1V8CLK,		0 },
	{ POWER_SUPPLY_DACVTT,		5 },
};

static const struct pwrsup_seq adcdac_seq[] = {
	{ POWER_SUPPLY_RFDC,            5 },
	{ POWER_SUPPLY_ADCVCC,		10 },
	{ POWER_SUPPLY_ADCVCCAUX,	5 },
	{ POWER_SUPPLY_DACVCC,		5 },
	{ POWER_SUPPLY_DACVCCAUX,	5 },
};

static const struct pwrsup_seq dioaux_seq[] = {
	{ POWER_SUPPLY_DIO_12V,		0 },
	{ POWER_SUPPLY_DIO_3V3,		0 },
	{ POWER_SUPPLY_DIO_1V2,		0 },
};

static const struct pwrsup_seq clkaux_seq[] = {
	{ POWER_SUPPLY_CLKDB_12V,	0 },
	{ POWER_SUPPLY_CLKDB_3V7,	0 },
	{ POWER_SUPPLY_CLKDB_3V3,	0 },
};

static void zynqmp_s0_por(void)
{
	ccprintf("ZynqMP: Resetting (POR) ... \n");
	gpio_set_level(GPIO_PS_POR_L, 0);
	msleep(65);
	gpio_set_level(GPIO_PS_POR_L, 1);
}

static void zynqmp_s0_srst(void)
{
	ccprintf("ZynqMP: Resetting (SRST) ... \n");
	gpio_set_level(GPIO_PS_SRST_L, 0);
	msleep(5);
	gpio_set_level(GPIO_PS_SRST_L, 1);
}

static void configure_bootmode(uint8_t mode)
{
	gpio_set_level(GPIO_PS_MODE_0, !!(mode & 0x1));
	gpio_set_level(GPIO_PS_MODE_1, !!(mode & 0x2));
	gpio_set_level(GPIO_PS_MODE_2, !!(mode & 0x4));
	gpio_set_level(GPIO_PS_MODE_3, !!(mode & 0x8));
}

static int forcing_shutdown;
static int power_error;

static int should_power_off(void)
{
	return forcing_shutdown || power_error;
}

enum power_state power_handle_state(enum power_state state)
{
	switch (state) {
	case POWER_G3:
		break;
	case POWER_S5:
		if (should_power_off())
			return POWER_S5G3;
		return POWER_S5S3;
	case POWER_S3:
		if (should_power_off())
			return POWER_S3S5;
		return POWER_S3S0;
	case POWER_S0:
		if (should_power_off())
			return POWER_S0S3;

		if (!pwrsup_check_supplies(s3s0_ps_seq, ARRAY_SIZE(s3s0_ps_seq))) {
			power_error = 1;
			return POWER_S0S3;
		}

		if (!power_has_signals(IN_S0_PWR_REQUIRED)) {
			/* power no longer needed, shut it down */
			forcing_shutdown = 1;
			return POWER_S0S3;
		}

		return state;
	case POWER_G3S5:
		forcing_shutdown = 0;
		power_error = 0;
		return POWER_S5;
	case POWER_S5S3:
		/* LTC4234 max turn-on delay is 72ms, give it far longer */
		if (pwrsup_power_on(POWER_SUPPLY_12V, 0, 200)) {
			ccprintf("failed to enable 12v rail\n");
			set_board_power_status(POWER_INPUT_BAD);
			return POWER_S3S5;
		}

		/* wait to ensure PMBUS devices are up */
		msleep(5);

		/* Set core supply to 850 mV */
		if (pmbus_set_volt_out(PMBUS_ID0, 850)) {
			ccprintf("failed to set pmbus output voltage\n");
			set_board_power_status(POWER_INPUT_BAD);
			return POWER_S3S5;
		}

		hook_notify(HOOK_CHIPSET_STARTUP);
		return POWER_S3;
	case POWER_S3S0:
		configure_bootmode(bootmode);

		gpio_set_level(GPIO_PS_POR_L, 0);
		msleep(65);

		if (pwrsup_seq_power_on(s3s0_ps_seq, ARRAY_SIZE(s3s0_ps_seq))) {
			ccprintf("failed to run power seq\n");
			power_error = 1;
			return POWER_S0S3;
		}

		gpio_set_level(GPIO_PS_POR_L, 1);
		gpio_set_level(GPIO_PS_SRST_L, 1);

		if (pwrsup_seq_power_on(adcdac_seq, ARRAY_SIZE(adcdac_seq)))
			ccprintf("failed to sequence adc/dac supplies\n");

		if (pwrsup_seq_power_on(clkaux_seq, ARRAY_SIZE(clkaux_seq)))
			ccprintf("failed to sequence clkaux\n");

		if (pwrsup_seq_power_on(dioaux_seq, ARRAY_SIZE(dioaux_seq)))
			ccprintf("failed to sequence dioaux\n");

		if (power_wait_signals(IN_S0_PWR_REQUIRED)) {
			ccprintf("power required signal unexpectedly low...");
			power_error = 1;
			return POWER_S0S3;
		}

		set_board_power_status(POWER_GOOD);
		hook_notify(HOOK_CHIPSET_RESUME);
		disable_sleep(SLEEP_MASK_AP_RUN);
		return POWER_S0;
	case POWER_S0S3:
		hook_notify(HOOK_CHIPSET_SUSPEND);

		pwrsup_seq_power_off(adcdac_seq, ARRAY_SIZE(adcdac_seq));
		pwrsup_seq_power_off(dioaux_seq, ARRAY_SIZE(dioaux_seq));
		pwrsup_seq_power_off(clkaux_seq, ARRAY_SIZE(clkaux_seq));

		pwrsup_seq_power_off(s3s0_ps_seq, ARRAY_SIZE(s3s0_ps_seq));
		set_board_power_status(power_error ? POWER_BAD : POWER_INPUT_GOOD);
		enable_sleep(SLEEP_MASK_AP_RUN);
		return POWER_S3;
	case POWER_S3S5:
		hook_notify(HOOK_CHIPSET_SHUTDOWN);
		return POWER_S5;
	case POWER_S5G3:
		pwrsup_power_off(POWER_SUPPLY_12V);
		return POWER_G3;
	}

	return state;
}

void chipset_force_shutdown(enum chipset_shutdown_reason reason)
{
	CPRINTS("%s(%d)", __func__, reason);
	report_ap_reset(reason);

	/*
	 * Force power off. This condition will reset once the state machine
	 * transitions to G3.
	 */
	forcing_shutdown = 1;
	task_wake(TASK_ID_CHIPSET);
}

void chipset_reset(enum chipset_reset_reason reason)
{
#ifdef CONFIG_CMD_RTC
	/* Print out the RTC to help correlate resets in logs. */
	print_system_rtc(CC_CHIPSET);
#endif
	CPRINTS("%s(%d)", __func__, reason);
	report_ap_reset(reason);

	zynqmp_s0_por();
}

enum power_state power_chipset_init(void)
{
	if (system_jumped_to_this_image()) {
		if (pwrsup_check_supplies(s3s0_ps_seq, ARRAY_SIZE(s3s0_ps_seq))) {
			disable_sleep(SLEEP_MASK_AP_RUN);
			CPRINTS("already in S0");
			return POWER_S0;
		}
	} else {
		bootmode = mcu_flags_get_bootmode();
		if (mcu_flags_get_autoboot())
			chipset_exit_hard_off();
	}

	return POWER_G3;
}


static void force_shutdown(void)
{
	if (power_button_is_pressed())
		chipset_force_shutdown(CHIPSET_SHUTDOWN_BUTTON);
}
DECLARE_DEFERRED(force_shutdown);

static void power_button_changed(void)
{
	if (power_button_is_pressed()) {
		if (chipset_in_state(CHIPSET_STATE_ANY_OFF))
			chipset_exit_hard_off();

		/* Delayed power down from S0/S3, cancel on PB release */
		hook_call_deferred(&force_shutdown_data,
				   FORCED_SHUTDOWN_DELAY);
	} else {
		/* Power button released, cancel deferred shutdown */
		hook_call_deferred(&force_shutdown_data, -1);
	}
}
DECLARE_HOOK(HOOK_POWER_BUTTON_CHANGE, power_button_changed, HOOK_PRIO_DEFAULT);

#ifdef CONFIG_CMD_ZYNQMP

static const char *bootmodes[] = {
	[0b0000] = "jtag",
	[0b0001] = "qspi24",
	[0b0010] = "qspi32",
	[0b0011] = "sd0",
	[0b0100] = "nand",
	[0b0101] = "sd1",
	[0b0110] = "emmc",
	[0b0111] = "usb",
	[0b1000] = "pjtag0",
	[0b1001] = "pjtag1",
	[0b1110] = "sd1ls",
};

static int str_to_bootmode(const char *boot_mode)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(bootmodes); i++)
		if (bootmodes[i] && !strcasecmp(bootmodes[i], boot_mode))
			return i;

	return -1;
}

static const char *bootmode_to_str(int bm)
{
	const char *str = NULL;

	if (bm < ARRAY_SIZE(bootmodes))
	    str = bootmodes[bm];

	if (!str)
		return "unknown";

	return str;
}

static int command_zynqmp(int argc, char **argv)
{
	int rv = 0;

	if (argc < 2)
		return EC_ERROR_PARAM_COUNT;

	if (!strcasecmp(argv[1], "status")) {
		ccprintf("ZynqMP status:\nPS_DONE:\t%d\nPS_INIT_L:\t%d\nPS_PROG_L:\t%d\nPS_ERR_OUT:\t%d\nPS_STAT:\t%d\n",
			 gpio_get_level(GPIO_PS_DONE), gpio_get_level(GPIO_PS_INIT_L),
			 gpio_get_level(GPIO_PS_PROG_L), gpio_get_level(GPIO_PS_ERR_OUT),
			 gpio_get_level(GPIO_PS_ERR_STAT));
	} else if (!strcasecmp(argv[1], "por")) {
		zynqmp_s0_por();
	} else if (!strcasecmp(argv[1], "srst")) {
		zynqmp_s0_srst();
	} else if (!strcasecmp(argv[1], "bootmode")) {
		if (argc > 2) {
			if (str_to_bootmode(argv[2]) < 0) {
				ccprintf("valid bootmodes: ");
				for (int i = 0; i < ARRAY_SIZE(bootmodes); i++)
					ccprintf("%s ", bootmodes[i] ?: "\b");
				ccprintf("\n");
				return EC_ERROR_PARAM2;
			}
			ccprintf("ZynqMP: Setting 'bootmode' to '%s'\n", argv[2]);
			bootmode = str_to_bootmode(argv[2]);
		} else {
			ccprintf("ZynqMP: 'bootmode' is '%s'\n", bootmode_to_str(bootmode));
		}
	} else {
		return EC_ERROR_PARAM1;
	}

	return rv;
}
DECLARE_CONSOLE_COMMAND(zynqmp, command_zynqmp,
			"bootmode/status/por/srst",
			"Misc commands for Xilinx ZynqMP based boards");
#endif
