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
#include "power.h"
#include "power_button.h"
#include "pmbus.h"
#include "gpio_list.h"

static int led_state;
static void board_second(void)
{
	gpio_set_level(GPIO_MCU_LED_L, !!led_state);

	led_state = !led_state;
}
DECLARE_HOOK(HOOK_SECOND, board_second, HOOK_PRIO_DEFAULT);

/* power signal list.  Must match order of enum power_signal. */
const struct power_signal_info power_signal_list[] = {
	{GPIO_MASTER_PG_MCU, POWER_SIGNAL_ACTIVE_HIGH, "MASTER_POWER_GOOD"},
	{GPIO_PS_DONE, POWER_SIGNAL_ACTIVE_HIGH, "PS_DONE_ASSERTED"},
	{GPIO_PS_INIT_L, POWER_SIGNAL_ACTIVE_LOW, "PS_INIT#_ASSERTED"},
	{GPIO_PS_ERR_OUT, POWER_SIGNAL_ACTIVE_HIGH, "PS_ERR_OUT_ASSERTED"},
	{GPIO_PS_ERR_STAT, POWER_SIGNAL_ACTIVE_HIGH, "PS_ERR_STAT_ASSERTED"},
	{GPIO_BUT_RESET_L, POWER_SIGNAL_ACTIVE_LOW, "BUT_RESET#_ASSERTED"},
};
BUILD_ASSERT(ARRAY_SIZE(power_signal_list) == POWER_SIGNAL_COUNT);


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

int extpower_is_present(void)
{
	/* There is no internal power on this board */
	return 1;
}

#ifdef CONFIG_ADC
/* ADC channels */
const struct adc_t adc_channels[] = {
	[ADC1_18]		= {"VSense", 1, 1, 0, STM32_AIN(18)},
	[ADC1_17]		= {"VRef", 3300, 4096, 0, STM32_AIN(17)},
	[VMON_0V9]		= {"VMON: 0.9V", 3300, 4096, 0, STM32_AIN(0)},
	[VMON_0V85]		= {"VMON: 0.85V", 3300,	4096, 0, STM32_AIN(1)},
	[VMON_0V925_DAC]	= {"VMON: 0.925V DAC", 3300, 4096, 0, STM32_AIN(2)},
	[VMON_0V925_ADC]	= {"VMON: 0.925V ADC", 3300, 4096, 0, STM32_AIN(3)},
	[VMON_DDRS_VMON_1V2]	= {"VMON: DDRS 1.2V", 3300, 4096, 0, STM32_AIN(4)},
	[VMON_DDRN_VMON_1V2]	= {"VMON: DDRN 1.2V", 3300, 4096, 0, STM32_AIN(5)},
	[VMON_1V8_ADC_AVCCAUX]	= {"VMON: 1.8V ADCVCCAUX", 3300, 4096, 0, STM32_AIN(6)},
	[VMON_1V8_DAC_AVCCAUX]	= {"VMON: 1.8V DACVCCAUX", 3300, 4096, 0, STM32_AIN(7)},
	[VMON_1V8]		= {"VMON: 1.8V", 3300, 4096, 0, STM32_AIN(8)},
	[VMON_2V5]		= {"VMON: 2.5V", 3300, 4096, 0, STM32_AIN(9)},
	[VMON_2V5_DAC_VTT]	= {"VMON: 2.5V DAC VTT", 3300, 4096, 0, STM32_AIN(10)},
	[VMON_VIN_IMON]		= {"VMON: VIN IMON", 3300, 4096, 0, STM32_AIN(11)},
	[VMON_1V8_CLK]		= {"VMON: 1V8 CLK", 2 * 3300, 4096, 0, STM32_AIN(12)},
	[VMON_3V3]		= {"VMON: 3.3V", 2 * 3300, 4096, 0, STM32_AIN(13)},
	[VMON_3V3_CLK]		= {"VMON: 3.3V CLK", 4950, 4096, 0, STM32_AIN(14)},
	[VMON_3V7]		= {"VMON: 3.6V", 2 * 3300, 4096, 0, STM32_AIN(15)},
};
BUILD_ASSERT(ARRAY_SIZE(adc_channels) == ADC_CH_COUNT);
#endif

#ifdef CONFIG_PMBUS
const struct pmbus_dev pmbus_devs[] = {
	{"TPSM846C23-Master", 0x36, I2C_PORT_PMBUS, PMBUS_VOUT_EXPONENT_DYNAMIC, GPIO_CORE_PMB_CNTL},
	{"TPSM846C23-Slave",  0x35, I2C_PORT_PMBUS, -9, GPIO_CORE_PMB_CNTL},
};
#endif

/* I2C ports */
const struct i2c_port_t i2c_ports[] = {
	{"core-pmbus", I2C_PORT_PMBUS, 400,
	 GPIO_CORE_PMB_CLK, GPIO_CORE_PMB_DAT},
	{"db-switch", I2C_PORT_DB, 400,
	 GPIO_DB_SWITCH_I2C_SCL, GPIO_DB_SWITCH_I2C_SDA},
};
const unsigned int i2c_ports_used = ARRAY_SIZE(i2c_ports);

#ifdef CONFIG_TEMP_SENSOR
#include "temp_sensor.h"
#include "driver/temp_sensor/ec_adc.h"
const struct temp_sensor_t temp_sensors[] = {
	{"PMBUS-0", TEMP_SENSOR_TYPE_BOARD, pmbus_temp_get_val, PMBUS_ID0},
	{"PMBUS-1", TEMP_SENSOR_TYPE_BOARD, pmbus_temp_get_val, PMBUS_ID1},
};
BUILD_ASSERT(ARRAY_SIZE(temp_sensors) == TEMP_SENSOR_COUNT);
#endif