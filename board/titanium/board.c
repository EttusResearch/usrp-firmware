/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
/* NI Project Titanium Firmware */

#include "adc_chip.h"
#include "board_power.h"
#include "common.h"
#include "console.h"
#include "ec_version.h"
#include "gpio.h"
#include "hooks.h"
#include "i2c.h"
#include "ina2xx.h"
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

#ifdef CONFIG_I2C_MUX
#define TCA954X_I2C_ADDR 0x70
#include "i2c_mux.h"
#include "driver/i2cmux_tca954x.h"
struct i2c_mux_t i2c_muxes[] = {
	{I2C_PORT_DB, TCA954X_I2C_ADDR, -1, tca954x_select_chan },
};
BUILD_ASSERT(ARRAY_SIZE(i2c_muxes) == I2C_MUX_COUNT);

struct i2c_mux_mapping i2c_mux_mappings[] = {
	{ I2C_PORT_DB0, I2C_MUX_MB, 0},
	{ I2C_PORT_DB0_PWR, I2C_MUX_MB, 1},
	{ I2C_PORT_DB1, I2C_MUX_MB, 2},
	{ I2C_PORT_DB1_PWR, I2C_MUX_MB, 3},
	{ I2C_PORT_MON, I2C_MUX_MB, 4},
	{ I2C_PORT_TMP464, I2C_MUX_MB, 5},
	{ I2C_PORT_RTC, I2C_MUX_MB, 6},
	{ I2C_PORT_PWR, I2C_MUX_MB, 7},
};

int i2c_mux_get_cfg(int port, enum i2c_mux_id *id, int *chan, int *parent)
{
	int p;

	for (p = 0; p < ARRAY_SIZE(i2c_mux_mappings); p++)
		if (i2c_mux_mappings[p].port == port) {
			*id = i2c_mux_mappings[p].id;
			*chan = i2c_mux_mappings[p].chan;
			*parent = i2c_mux_get_parent(*id);
			return 0;
		}

	return EC_ERROR_INVAL;
}

int i2c_port_to_controller(int port)
{

	int p;

	if (i2c_port_is_muxed(port))
		for (p = 0; p < ARRAY_SIZE(i2c_mux_mappings); p++)
			if (i2c_mux_mappings[p].port == port)
				return i2c_mux_get_parent(i2c_mux_mappings[p].id);
	return port;
}
#endif

/* PWM channels. Must be in the exactly same order as in enum pwm_channel. */
#ifdef CONFIG_PWM
const struct pwm_t pwm_channels[] = {
	{STM32_TIM(9), STM32_TIM_CH(2), PWM_CONFIG_ACTIVE_LOW, 25000},
	{STM32_TIM(4), STM32_TIM_CH(2), PWM_CONFIG_ACTIVE_LOW, 25000},
};
BUILD_ASSERT(ARRAY_SIZE(pwm_channels) == PWM_CH_COUNT);
#endif

/* Initialize board. */
#include "tca64xx.h"
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
	[VMON_0V6_DDR_VREF]	= {"VMON: 0.6V DDR VREF", 3 * 3300 / 2, 4096, 0, STM32_AIN(2)},
	[VMON_0V925_ADC_DAC]	= {"VMON: 0.925V ADC", 3 * 3300 / 2, 4096, 0, STM32_AIN(3)},
	[VMON_1V2_DDRS]		= {"VMON: 1.2V DDRS", 3300, 4096, 0, STM32_AIN(4)},
	[VMON_1V2_DDRN]		= {"VMON: 1.2V DDRN", 3300, 4096, 0, STM32_AIN(5)},
	[VMON_0V6_DDR_VTT]	= {"VMON: 0.6V DDR VTT", 3 * 3300 / 2, 4096, 0, STM32_AIN(6)},
	[VMON_1V8_ADC_DAC_AUX]	= {"VMON: 1.8V ADC/DAC AUX", 3 * 3300 / 2, 4096, 0, STM32_AIN(7)},
	[VMON_1V8]		= {"VMON: 1.8V", 3300, 4096, 0, STM32_AIN(8)},
	[VMON_2V5]		= {"VMON: 2.5V", 3300, 4096, 0, STM32_AIN(9)},
	[VMON_2V5_DAC_VTT]	= {"VMON: 2.5V DAC VTT", 2 * 3300, 4096, 0, STM32_AIN(10)},
	[VMON_VIN_IMON]		= {"VMON: VIN IMON", 3300, 4096, 0, STM32_AIN(11)},
	[VMON_1V8_CLK]		= {"VMON: 1.8V CLK", 3300, 4096, 0, STM32_AIN(12)},
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
	{"core-pmbus", I2C_PORT_PMBUS, 400, GPIO_CORE_PMB_CLK, GPIO_CORE_PMB_DAT},
	{"db-switch", I2C_PORT_DB, 400, GPIO_DB_SWITCH_I2C_SCL, GPIO_DB_SWITCH_I2C_SDA},
	{"slave", I2C_PORT_SLAVE, 400, GPIO_SLAVE_I2C_SCL, GPIO_SLAVE_I2C_SDA},
};
const unsigned int i2c_ports_used = ARRAY_SIZE(i2c_ports);

#ifdef CONFIG_TEMP_SENSOR
#include "temp_sensor.h"
#include "driver/temp_sensor/ec_adc.h"
#include "driver/temp_sensor/tmp468.h"
const struct temp_sensor_t temp_sensors[] = {
	{"PMBUS-0", TEMP_SENSOR_TYPE_BOARD, pmbus_temp_get_val, PMBUS_ID0},
	{"PMBUS-1", TEMP_SENSOR_TYPE_BOARD, pmbus_temp_get_val, PMBUS_ID1},
	{"EC Internal", TEMP_SENSOR_TYPE_BOARD, ec_adc_get_val, ADC1_18},
	{"TMP464 Internal", TEMP_SENSOR_TYPE_BOARD, tmp468_get_val, TMP468_LOCAL},
	{"TMP464 Remote1", TEMP_SENSOR_TYPE_BOARD, tmp468_get_val, TMP468_REMOTE1},
	{"RFSoC", TEMP_SENSOR_TYPE_CPU, tmp468_get_val, TMP468_REMOTE2},
	{"TMP464 Remote3", TEMP_SENSOR_TYPE_BOARD, tmp468_get_val, TMP468_REMOTE3},
	{"TMP464 Remote4", TEMP_SENSOR_TYPE_BOARD, tmp468_get_val, TMP468_REMOTE4},
};
BUILD_ASSERT(ARRAY_SIZE(temp_sensors) == TEMP_SENSOR_COUNT);
#endif

#ifdef CONFIG_IO_EXPANDER
struct ioexpander_config_t ioex_config[] = {
	{ I2C_PORT_PWR, TCA6416_I2C_ADDR(0), &tca6416_ioexpander_drv },
	{ I2C_PORT_DB0_PWR, TCA6416_I2C_ADDR(0), &tca6416_ioexpander_drv },
	{ I2C_PORT_DB1_PWR, TCA6416_I2C_ADDR(0), &tca6416_ioexpander_drv },
};
BUILD_ASSERT(ARRAY_SIZE(ioex_config) == CONFIG_IO_EXPANDER_PORT_COUNT);
#endif

#ifdef CONFIG_INA219
/* Default value for configuration register is 0x399f which allows for
   continuous bus and shunt voltage measurement along with
   highest voltage ranges and ADC resolution. However since we
   know that the input current is not very high we can configure the PGA gain bits
   to have lower shunt voltage range resulting in higher voltage resolution.
   e.g. If the highest current is 20 A the the highest shunt voltage is
   I * R = 2 mOhm * 20 A = 40 mV.
   Selecting the PGA bits corresponding to a voltage range of +/- 40 mV
   and keeping other bits the same as default results in a 0x219F value.
   Refer http://www.ti.com/lit/ds/symlink/ina219.pdf
   */
const struct ina2xx_t ina2xx_sensors[] = {
	{ "0V9",  I2C_PORT_MON, INA2XX_I2C_ADDR(0, 0), 0x219f, INA2XX_CALIB_1MA(2) },
	{ "1V8",  I2C_PORT_MON, INA2XX_I2C_ADDR(0, 1), 0x219f, INA2XX_CALIB_1MA(2) },
	{ "3V6",  I2C_PORT_MON, INA2XX_I2C_ADDR(0, 2), 0x219f, INA2XX_CALIB_1MA(2) },
	{ "3V3",  I2C_PORT_MON, INA2XX_I2C_ADDR(1, 0), 0x219f, INA2XX_CALIB_1MA(2) },
	{ "2V5",  I2C_PORT_MON, INA2XX_I2C_ADDR(1, 1), 0x219f, INA2XX_CALIB_1MA(2) },
	{ "1V2N", I2C_PORT_MON, INA2XX_I2C_ADDR(2, 0), 0x219f, INA2XX_CALIB_1MA(2) },
	{ "1V2S", I2C_PORT_MON, INA2XX_I2C_ADDR(2, 2), 0x219f, INA2XX_CALIB_1MA(2) },
};
BUILD_ASSERT(ARRAY_SIZE(ina2xx_sensors) == INA2XX_COUNT);
#endif
