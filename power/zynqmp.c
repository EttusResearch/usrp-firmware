/* Copyright (c) 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* ZynqMP chipset power control module for Chrome EC */

#include "charge_state.h"
#include "chipset.h"
#include "common.h"
#include "console.h"
#include "ec_commands.h"
#include "gpio.h"
#include "hooks.h"
#include "lid_switch.h"
#include "power.h"
#include "power_button.h"
#include "system.h"
#include "task.h"
#include "timer.h"
#include "util.h"

/* Long power key press to force shutdown in S0 */
#define FORCED_SHUTDOWN_DELAY  (8 * SECOND)

/* Console output macros */
#define CPUTS(outstr) cputs(CC_CHIPSET, outstr)
#define CPRINTS(format, args...) cprints(CC_CHIPSET, format, ## args)

/* ZynqMP bootmode */
static uint8_t bootmode;

/* Data structure for a GPIO operation for power sequencing */
struct power_seq_op {
	/* enum gpio_signal in 8 bits */
	uint8_t signal;
	uint8_t level;
	/* Number of milliseconds to delay after setting signal to level */
	uint8_t delay;
};
BUILD_ASSERT(GPIO_COUNT < 256);


static const struct power_seq_op g3s0_seq[] = {
	{ GPIO_PS_POR_L, 0, 65 },
	{ GPIO_CORE_PMB_CNTL, 1, 5 },
	{ GPIO_1V8_EN, 1, 5 },
	{ GPIO_3V3_EN, 1, 5 },
	{ GPIO_PS_POR_L, 1, 0 },
	{ GPIO_0V9_EN, 1, 0 },
	{ GPIO_DDR4N_VDDQ_EN, 1, 5 },
	{ GPIO_DDR4S_VDDQ_EN, 1, 5 },
	{ GPIO_3V6_EN, 1, 5 },
	{ GPIO_2V5_EN, 1, 5 },
	{ GPIO_DDR4S_VTT_EN, 1, 5 },
	{ GPIO_DDR4N_VTT_EN, 1, 5 },
	{ GPIO_3V3_CLK_EN, 1, 5 },
	{ GPIO_PS_SRST_L, 1, 0 },
};

/**
 * Step through the power sequence table and do corresponding GPIO operations.
 *
 * @param	power_seq_ops	The pointer to the power sequence table.
 * @param	op_count	The number of entries of power_seq_ops.
 * @return	non-zero if error, 0 otherwise.
 */
static int power_seq_run(const struct power_seq_op *power_seq_ops, int op_count)
{
	int i;

	for (i = 0; i < op_count; i++) {
		gpio_set_level(power_seq_ops[i].signal,
			       power_seq_ops[i].level);
		if (!power_seq_ops[i].delay)
			continue;
		msleep(power_seq_ops[i].delay);
	}
	return 0;
}

enum power_state power_handle_state(enum power_state state)
{
	return POWER_G3;
}

static int forcing_shutdown;

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
}

enum power_state power_chipset_init(void)
{
	if (system_jumped_to_this_image()) {
		if ((power_get_signals() /*& IN_ALL_S0*/)/* == IN_ALL_S0*/) {
			disable_sleep(SLEEP_MASK_AP_RUN);
			CPRINTS("already in S0");
			return POWER_S0;
		}
	} else if (!(system_get_reset_flags() & EC_RESET_FLAG_AP_OFF))
		/* Auto-power on */
		chipset_exit_hard_off();

	return POWER_G3;
}


static void force_shutdown(void)
{
	forcing_shutdown = 1;
	task_wake(TASK_ID_CHIPSET);
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

struct boot_mode {
	const char *name;
	int val;
};

static const struct boot_mode modes_lookup[] =  { { "jtag", 0}, { "emmc", 6 } };

static int str_to_bootmode(const char *boot_mode)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(modes_lookup); i++)
		if (!strcasecmp(modes_lookup[i].name, boot_mode))
			return modes_lookup[i].val;

	/* default to jtag */
	return 0;
}

static const char *bootmode_to_str(int bm)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(modes_lookup); i++)
		if (modes_lookup[i].val == bm)
			return modes_lookup[i].name;

	/* default to jtag */
	return 0;
}

static int command_zynqmp(int argc, char **argv)
{
	int rv = 0;

	if (argc < 2)
		return EC_ERROR_PARAM_COUNT;

	if (!strcasecmp(argv[1], "status")) {
		// gpioget() ...
		ccprintf("ZynqMP status:\nPS_DONE:\t%d\nPS_INIT_L:\t%d\nPS_PROG_L:\t%d\nPS_ERR_OUT:\t%d\nPS_STAT:\t%d\n",
			 gpio_get_level(GPIO_PS_DONE), gpio_get_level(GPIO_PS_INIT_L),
			 gpio_get_level(GPIO_PS_PROG_L), gpio_get_level(GPIO_PS_ERR_OUT),
			 gpio_get_level(GPIO_PS_ERR_STAT));
	} else if (!strcasecmp(argv[1], "boot")) {
		ccprintf("ZynqMP: Booting using bootmode: %s\n", bootmode_to_str(bootmode));
		power_seq_run(&g3s0_seq[0], ARRAY_SIZE(g3s0_seq));
	} else if (!strcasecmp(argv[1], "bootmode")) {
		if (argc > 2) {
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
			"bootmode/status/boot idx [jtag|emmc] ",
			"Misc commands for Xilinx ZynqMP based boards");
#endif
