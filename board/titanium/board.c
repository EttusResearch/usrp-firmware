/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
/* NI Project Titanium Firmware */

#include "adc_chip.h"
#include "common.h"
#include "console.h"
#include "ec_version.h"
#include "gpio.h"
#include "hooks.h"
#include "i2c.h"
#include "pwm.h"
#include "pwm_chip.h"
#include "gpio.h"
#include "registers.h"
#include "task.h"
#include "timer.h"
#include "util.h"

#include "power_button.h"
#include "gpio_list.h"

/* PWM channels. Must be in the exactly same order as in enum pwm_channel. */
#ifdef CONFIG_PWM
const struct pwm_t pwm_channels[] = {
	{STM32_TIM(9), STM32_TIM_CH(2), PWM_CONFIG_ACTIVE_LOW, 25000},
	{STM32_TIM(4), STM32_TIM_CH(2), PWM_CONFIG_ACTIVE_LOW, 25000},
};
BUILD_ASSERT(ARRAY_SIZE(pwm_channels) == PWM_CH_COUNT);
#endif

/* Initialize board. */
static void board_init(void)
{
	/* No power control yet */
	/* Go to S3 state */
	hook_notify(HOOK_CHIPSET_STARTUP);

	/* Go to S0 state */
	hook_notify(HOOK_CHIPSET_RESUME);
}
DECLARE_HOOK(HOOK_INIT, board_init, HOOK_PRIO_DEFAULT);

/* I2C ports */
const struct i2c_port_t i2c_ports[] = {
	{"core-pmbus", I2C_PORT_PMBUS, 400,
	 GPIO_CORE_PMB_CLK, GPIO_CORE_PMB_DAT},
	{"db-switch", I2C_PORT_DB, 400,
	 GPIO_DB_SWITCH_I2C_SCL, GPIO_DB_SWITCH_I2C_SDA},
};
const unsigned int i2c_ports_used = ARRAY_SIZE(i2c_ports);
