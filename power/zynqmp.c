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

/* Long power key press to force shutdown in S0 */
#define FORCED_SHUTDOWN_DELAY  (8 * SECOND)

#define IN_ALL_S0 (POWER_SIGNAL_MASK(MASTER_PG_MCU) | POWER_SIGNAL_MASK(PS_PWR_GOOD))

/* Console output macros */
#define CPUTS(outstr) cputs(CC_CHIPSET, outstr)
#define CPRINTS(format, args...) cprints(CC_CHIPSET, format, ## args)

/* ZynqMP bootmode */
static uint8_t bootmode = 0x06; /*eMMC mode as default*/

/* Data structure for a GPIO operation for power sequencing */
struct power_seq_op {
	/* enum gpio_signal in 8 bits */
	uint8_t signal;
	uint8_t level;
	/* Number of milliseconds to delay after setting signal to level */
	uint8_t delay;
	uint8_t feedback_channel;
	unsigned int feedback_mv;

};
BUILD_ASSERT(GPIO_COUNT < 256);
BUILD_ASSERT(ADC_CH_COUNT < 256);

static const struct power_seq_op s3s0_seq[] = {
	{ GPIO_PS_POR_L,          0, 65, ADC_CH_COUNT,         0 },
	{ GPIO_CORE_PMB_CNTL,     1,  5, VMON_0V85,            850 * 0.9 },
	{ GPIO_1V8_EN,            1,  5, VMON_1V8,             1800 * 0.9 },
	{ GPIO_2V5_EN,            1,  5, VMON_2V5,             2500 * 0.9 },
	{ GPIO_3V3_EN,            1,  5, VMON_3V3,             3300 * 0.9 },
	{ GPIO_0V9_EN,            1,  0, VMON_0V9,             900 * 0.9 },
	{ GPIO_MGTAUX_EN_MCU,     1,  5, ADC_CH_COUNT,         0 },
	{ GPIO_DDR4N_VDDQ_EN,     1,  5, VMON_1V2_DDRN,        1200 * 0.9 },
	{ GPIO_DDR4N_VTT_EN,      1,  5, ADC_CH_COUNT,         0 },
	{ GPIO_DDR4S_VDDQ_EN,     1,  5, VMON_1V2_DDRS,        1200 * 0.9 },
	{ GPIO_DDR4S_VTT_EN,      1,  5, ADC_CH_COUNT,         0 },
	{ GPIO_3V6_EN,            1,  5, VMON_3V7,             3600 * 0.9 },
	{ GPIO_3V3_CLK_EN,        1,  5, ADC_CH_COUNT,         0 },
	{ GPIO_ADCVCC_EN,         1, 10, ADC_CH_COUNT,         0 },
	{ GPIO_ADC_VCCAUX_EN,     1,  5, ADC_CH_COUNT,         0 },
	{ GPIO_DACVCC_EN,         1,  5, VMON_0V925_ADC_DAC,   925 * 0.9 },
	{ GPIO_DAC_VCCAUX_EN,     1,  5, VMON_1V8_ADC_DAC_AUX, 1800 * 0.9 },
	{ GPIO_DACVTT_EN,         1,  5, VMON_2V5_DAC_VTT,     2500 * 0.9 },
	{ GPIO_CLK_DIO_DB_PWR_EN, 1,  5, ADC_CH_COUNT,         0 },
	{ GPIO_PS_POR_L,          1,  0, ADC_CH_COUNT,         0 },
	{ GPIO_PS_SRST_L,         1,  0, ADC_CH_COUNT,         0 },
};

/* TODO: Use non-zero delays here like s3s0_seq? */
static const struct power_seq_op s0s3_seq[] = {
	{ GPIO_CLK_DIO_DB_PWR_EN, 0,  0, ADC_CH_COUNT, 0 },
	{ GPIO_DACVTT_EN,         0,  0, ADC_CH_COUNT, 0 },
	{ GPIO_DAC_VCCAUX_EN,     0,  0, ADC_CH_COUNT, 0 },
	{ GPIO_DACVCC_EN,         0,  0, ADC_CH_COUNT, 0 },
	{ GPIO_ADC_VCCAUX_EN,     0,  0, ADC_CH_COUNT, 0 },
	{ GPIO_ADCVCC_EN,         0,  0, ADC_CH_COUNT, 0 },
	{ GPIO_3V3_CLK_EN,        0,  0, ADC_CH_COUNT, 0 },
	{ GPIO_3V6_EN,            0,  0, ADC_CH_COUNT, 0 },
	{ GPIO_DDR4S_VTT_EN,      0,  0, ADC_CH_COUNT, 0 },
	{ GPIO_DDR4S_VDDQ_EN,     0,  0, ADC_CH_COUNT, 0 },
	{ GPIO_DDR4N_VTT_EN,      0,  0, ADC_CH_COUNT, 0 },
	{ GPIO_DDR4N_VDDQ_EN,     0,  0, ADC_CH_COUNT, 0 },
	{ GPIO_MGTAUX_EN_MCU,     0,  0, ADC_CH_COUNT, 0 },
	{ GPIO_0V9_EN,            0,  0, ADC_CH_COUNT, 0 },
	{ GPIO_3V3_EN,            0,  0, ADC_CH_COUNT, 0 },
	{ GPIO_2V5_EN,            0,  0, ADC_CH_COUNT, 0 },
	{ GPIO_1V8_EN,            0,  0, ADC_CH_COUNT, 0 },
	{ GPIO_CORE_PMB_CNTL,     0,  0, ADC_CH_COUNT, 0 },
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

static int power_seq_adc_wait(enum adc_channel chan, int min_mv)
{
	int mv, timeout = 50;

	for (;;) {
		mv = adc_read_channel(chan);
		if (mv >= min_mv)
			break;

		if (!timeout--) {
			ccprintf("Failed to bringup zynqmp\n");
			return EC_ERROR_TIMEOUT;
		}

		msleep(1);
	}

	return 0;
}

/**
 * Step through the power sequence table and do corresponding GPIO operations.
 *
 * @param	op		The pointer to the power sequence table.
 * @param	op_count	The number of entries of power_seq_ops.
 * @return	non-zero if error, 0 otherwise.
 */
static int power_seq_run(const struct power_seq_op *op, int op_count)
{
	int err;

	while (op_count--) {

		gpio_set_level(op->signal,
			       op->level);

		if (op->delay)
			msleep(op->delay);

		if (op->feedback_channel < ADC_CH_COUNT) {
			err = power_seq_adc_wait(op->feedback_channel,
						 op->feedback_mv);
			if (err)
				return err;
		}

		op++;
	}

	return 0;
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

enum power_state power_handle_state(enum power_state state)
{
	int v, timeout;

	switch (state) {
	case POWER_G3:
		break;
	case POWER_S5:
		if (forcing_shutdown || power_error)
			return POWER_S5G3;
		return POWER_S5S3;
	case POWER_S3:
		if (forcing_shutdown || power_error)
			return POWER_S3S5;
		return POWER_S3S0;
	case POWER_S0:
		if (forcing_shutdown)
			return POWER_S0S3;
		if ((power_get_signals() & IN_ALL_S0) != IN_ALL_S0) {
			power_error = 1;
			return POWER_S0S3;
		}
		return state;
	case POWER_G3S5:
		forcing_shutdown = 0;
		power_error = 0;
		return POWER_S5;
	case POWER_S5S3:
		if (ioex_set_level(IOEX_PWRDB_12V_EN_L, 0)) {
			ccprintf("failed to enable 12v rail\n");
			set_board_power_status(POWER_INPUT_BAD);
			return POWER_S3S5;
		}

		/* LTC4234 max turn-on delay is 72ms, give it far longer */
		timeout = 200;
		do {
			if (ioex_get_level(IOEX_PWRDB_VIN_PG, &v) == 0 && v)
				break;
			msleep(1);
		} while (timeout--);

		if (ioex_get_level(IOEX_PWRDB_VIN_PG, &v) || !v) {
			ccprintf("vin power good is low\n");
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

		if (power_seq_run(&s3s0_seq[0], ARRAY_SIZE(s3s0_seq))) {
			ccprintf("failed to run power seq\n");
			power_error = 1;
			return POWER_S0S3;
		}

		if (power_wait_signals_timeout(IN_ALL_S0, 100 * MSEC)
						== EC_ERROR_TIMEOUT)  {
			ccprintf("master pg not good\n");
			power_error = 1;
			return POWER_S0S3;
		}

		set_board_power_status(POWER_GOOD);
		hook_notify(HOOK_CHIPSET_RESUME);
		disable_sleep(SLEEP_MASK_AP_RUN);
		return POWER_S0;
	case POWER_S0S3:
		hook_notify(HOOK_CHIPSET_SUSPEND);
		power_seq_run(&s0s3_seq[0], ARRAY_SIZE(s0s3_seq));
		set_board_power_status(power_error ? POWER_BAD : POWER_INPUT_GOOD);
		enable_sleep(SLEEP_MASK_AP_RUN);
		return POWER_S3;
	case POWER_S3S5:
		hook_notify(HOOK_CHIPSET_SHUTDOWN);
		return POWER_S5;
	case POWER_S5G3:
		ioex_set_level(IOEX_PWRDB_12V_EN_L, 1);
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
		if ((power_get_signals() & IN_ALL_S0) == IN_ALL_S0) {
			disable_sleep(SLEEP_MASK_AP_RUN);
			CPRINTS("already in S0");
			return POWER_S0;
		}
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
