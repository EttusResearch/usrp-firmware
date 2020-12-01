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
#include "pwrsup.h"
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
#include "host_control_gpio.h"
#include "fan.h"
#include "db_pwr.h"

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
	{GPIO_PS_ERR_OUT, POWER_SIGNAL_ACTIVE_LOW, "PS_PWR_GOOD ASSERTED"},
	{GPIO_PS_ERR_STAT, POWER_SIGNAL_ACTIVE_HIGH, "PS_ERR_STAT_ASSERTED"},
	{GPIO_BUT_RESET_L, POWER_SIGNAL_ACTIVE_LOW, "BUT_RESET#_ASSERTED"},
	{GPIO_PS_SHUTDOWN_L, POWER_SIGNAL_ACTIVE_HIGH, "PS_PWR_REQUIRED"},
};
BUILD_ASSERT(ARRAY_SIZE(power_signal_list) == POWER_SIGNAL_COUNT);

#ifdef CONFIG_HOST_CONTROL_GPIO
const struct host_control_gpio host_control_gpios[] = {
	[USER_LED_G] = { .name = "USER_LED_G_L", .signal = IOEX_PWRDB_LED2G_L },
	[USER_LED_R] = { .name = "USER_LED_R_L", .signal = IOEX_PWRDB_LED2R_L },
	[PCIE_LED_G] = { .name = "PCIE_LED_G_L", .signal = IOEX_PWRDB_LED0G_L },
	[PCIE_LED_R] = { .name = "PCIE_LED_R_L", .signal = IOEX_PWRDB_LED0R_L },
	[RFDC_POWERED] = { .name = "RFDC_POWERED", .signal = GPIO_SCPLD_IN },
	[DB0_PWR_EN] = { .name = "DB0_PWR_EN", .signal = 0, .set = db_pwr_ctrl },
	[DB0_PWR_STATUS] = { .name = "DB0_PWR_STATUS", .signal = 0, .get = db_pwr_stat },
	[DB1_PWR_EN] = { .name = "DB1_PWR_EN", .signal = 1, .set = db_pwr_ctrl },
	[DB1_PWR_STATUS] = { .name = "DB1_PWR_STATUS", .signal = 1, .get = db_pwr_stat },
};
BUILD_ASSERT(ARRAY_SIZE(host_control_gpios) == HOST_CONTROL_GPIO_COUNT);
#endif

const struct pwrsup_info power_supply_list[] = {
	PWRSUP_INFO(12V,        12V,        IOEX_PWRDB_12V_EN,   PWRSUP_MON_SIG(IOEX_PWRDB_VIN_PG)),
	PWRSUP_INFO(0V85,       12V,        GPIO_CORE_PMB_CNTL,  PWRSUP_MON_ADC(VMON_0V85, 850 * 0.9)),
	PWRSUP_INFO(1V8,        12V,        GPIO_1V8_EN,         PWRSUP_MON_ADC(VMON_1V8, 1800 * 0.9)),
	PWRSUP_INFO(2V5,        12V,        GPIO_2V5_EN,         PWRSUP_MON_ADC(VMON_2V5, 2500 * 0.9)),
	PWRSUP_INFO(3V3,        12V,        GPIO_3V3_EN,         PWRSUP_MON_ADC(VMON_3V3, 3300 * 0.9)),
	PWRSUP_INFO(0V9,        12V,        GPIO_0V9_EN,         PWRSUP_MON_ADC(VMON_0V9,  900 * 0.9)),
	PWRSUP_INFO(MGTAUX,     2V5,        GPIO_MGTAUX_EN_MCU),
	PWRSUP_INFO(DDR4N_VDDQ, 12V,        GPIO_DDR4N_VDDQ_EN,  PWRSUP_MON_ADC(VMON_1V2_DDRN, 1200 * 0.9)),
	PWRSUP_INFO(DDR4N_VTT,  DDR4N_VDDQ, GPIO_DDR4N_VTT_EN),
	PWRSUP_INFO(DDR4S_VDDQ, 12V,        GPIO_DDR4S_VDDQ_EN,  PWRSUP_MON_ADC(VMON_1V2_DDRS, 1200 * 0.9)),
	PWRSUP_INFO(DDR4S_VTT,  DDR4S_VDDQ, GPIO_DDR4S_VTT_EN),
	PWRSUP_INFO(3V6,        12V,        GPIO_3V6_EN,         PWRSUP_MON_ADC(VMON_3V7,     3600 * 0.9)),
	PWRSUP_INFO(3V3CLK,     3V6,        GPIO_3V3_CLK_EN,     PWRSUP_MON_ADC(VMON_3V3_CLK, 3300 * 0.9)),
	PWRSUP_INFO(1V8CLK,     2V5,        GPIO_3V3_CLK_EN,     PWRSUP_MON_ADC(VMON_1V8_CLK, 1800 * 0.9)),
	PWRSUP_INFO(DACVTT,     3V6,        GPIO_DACVTT_EN,      PWRSUP_MON_ADC(VMON_3V0_DAC_VTT, 3000 * 0.9)),
	PWRSUP_INFO(RFDC,       12V,        GPIO_STM_PG_OUT,     PWRSUP_MON_SIG(GPIO_SCPLD_IN)),

	PWRSUP_INFO(ADCVCC,     DDR4N_VDDQ, GPIO_ADCVCC_EN,      PWRSUP_MON_SIG(GPIO_RF_PG_MCU)),
	PWRSUP_INFO(ADCVCCAUX,  2V5,        GPIO_ADC_VCCAUX_EN,  PWRSUP_MON_SIG(GPIO_RF_PG_MCU)),
	PWRSUP_INFO(DACVCC,     DDR4N_VDDQ, GPIO_DACVCC_EN,	 PWRSUP_MON_SIG(GPIO_RF_PG_MCU)),
	PWRSUP_INFO(DACVCCAUX,  2V5,        GPIO_DAC_VCCAUX_EN,  PWRSUP_MON_SIG(GPIO_RF_PG_MCU)),

	PWRSUP_INFO(CLKDB_3V3,  3V3,        IOEX_CLKDB_3V3_EN,   PWRSUP_MON_SIG(IOEX_CLKDB_3V3_PG)),
	PWRSUP_INFO(CLKDB_3V7,  3V6,        IOEX_CLKDB_3V7_EN,   PWRSUP_MON_SIG(IOEX_CLKDB_3V7_PG)),
	PWRSUP_INFO(CLKDB_12V,  12V,        IOEX_CLKDB_12V_EN,   PWRSUP_MON_SIG(IOEX_CLKDB_12V_PG)),

	PWRSUP_INFO(DIO_12V,    12V,        IOEX_DIO_12V_EN,     PWRSUP_MON_SIG(IOEX_DIO_12V_PG)),
	PWRSUP_INFO(DIO_1V2,    DDR4N_VDDQ, IOEX_DIO_1V2_EN,     PWRSUP_MON_SIG(IOEX_DIO_1V2_PG)),
	PWRSUP_INFO(DIO_3V3,    3V3,        IOEX_DIO_3V3_EN,     PWRSUP_MON_SIG(IOEX_DIO_3V3_PG)),

	PWRSUP_INFO(DB0_12V,    12V, IOEX_DB0_12V_EN,    PWRSUP_MON_SIG(IOEX_DB0_12V_PG)),
	PWRSUP_INFO(DB0_3V3,    3V3, IOEX_DB0_3V3_EN,    PWRSUP_MON_SIG(IOEX_DB0_3V3_PG)),
	PWRSUP_INFO(DB0_3V7,    3V6, IOEX_DB0_3V7_EN,    PWRSUP_MON_SIG(IOEX_DB0_3V7_PG)),
	PWRSUP_INFO(DB0_2V5,    2V5, IOEX_DB0_2V5_EN,    PWRSUP_MON_SIG(IOEX_DB0_2V5_PG)),
	PWRSUP_INFO(DB0_1V8,    1V8, IOEX_DB0_1V8_EN,    PWRSUP_MON_SIG(IOEX_DB0_1V8_PG)),
	PWRSUP_INFO(DB0_3V3MCU, 12V, IOEX_DB0_3V3MCU_EN, PWRSUP_MON_SIG(IOEX_DB0_3V3MCU_PG)),

	PWRSUP_INFO(DB1_12V,    12V, IOEX_DB1_12V_EN,    PWRSUP_MON_SIG(IOEX_DB1_12V_PG)),
	PWRSUP_INFO(DB1_3V3,    3V3, IOEX_DB1_3V3_EN,    PWRSUP_MON_SIG(IOEX_DB1_3V3_PG)),
	PWRSUP_INFO(DB1_3V7,    3V6, IOEX_DB1_3V7_EN,    PWRSUP_MON_SIG(IOEX_DB1_3V7_PG)),
	PWRSUP_INFO(DB1_2V5,    2V5, IOEX_DB1_2V5_EN,    PWRSUP_MON_SIG(IOEX_DB1_2V5_PG)),
	PWRSUP_INFO(DB1_1V8,    1V8, IOEX_DB1_1V8_EN,    PWRSUP_MON_SIG(IOEX_DB1_1V8_PG)),
	PWRSUP_INFO(DB1_3V3MCU, 12V, IOEX_DB1_3V3MCU_EN, PWRSUP_MON_SIG(IOEX_DB1_3V3MCU_PG)),
};

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
	{STM32_TIM(4), STM32_TIM_CH(1), PWM_CONFIG_ACTIVE_LOW, 25000},
	{STM32_TIM(4), STM32_TIM_CH(2), PWM_CONFIG_ACTIVE_LOW, 25000},
};
BUILD_ASSERT(ARRAY_SIZE(pwm_channels) == PWM_CH_COUNT);
#endif

/* Initialize board. */
#include "tca64xx.h"
static void board_init(void)
{
	gpio_enable_interrupt(GPIO_DB_PWR_INT);
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
	[VMON_VBATT]		= {"Vbatt", 4 * 3300, 4096, 0, STM32_ADC_CHANNEL_VBATT},
	[ADC1_TEMPERATURE]	= {"Vtemp", 1, 1, 0, STM32_ADC_CHANNEL_TEMPERATURE},
	[ADC1_17]		= {"VRef", 3300, 4096, 0, STM32_AIN(17)},
	[VMON_0V9]		= {"0V9", 3300, 4096, 0, STM32_AIN(0)},
	[VMON_0V85]		= {"0V85", 3300,	4096, 0, STM32_AIN(1)},
	[VMON_0V6_DDR_VREF]	= {"0V6_DDR_VREF", 3 * 3300 / 2, 4096, 0, STM32_AIN(2)},
	[VMON_0V925_ADC_DAC]	= {"0V925_ADC_DAC", 3 * 3300 / 2, 4096, 0, STM32_AIN(3)},
	[VMON_1V2_DDRS]		= {"1V2_DDRS", 3300, 4096, 0, STM32_AIN(4)},
	[VMON_1V2_DDRN]		= {"1V2_DDRN", 3300, 4096, 0, STM32_AIN(5)},
	[VMON_0V6_DDR_VTT]	= {"0V6_DDR_VTT", 3 * 3300 / 2, 4096, 0, STM32_AIN(6)},
	[VMON_1V8_ADC_DAC_AUX]	= {"1V8_ADC_DAC_AUX", 3 * 3300 / 2, 4096, 0, STM32_AIN(7)},
	[VMON_1V8]		= {"1V8", 3300, 4096, 0, STM32_AIN(8)},
	[VMON_2V5]		= {"2V5", 3300, 4096, 0, STM32_AIN(9)},
	[VMON_3V0_DAC_VTT]	= {"3V0_DAC_VTT", 2 * 3300, 4096, 0, STM32_AIN(10)},
	[VMON_VIN_IMON]		= {"VIN_IMON", 3300, 4096, 0, STM32_AIN(11)},
	[VMON_1V8_CLK]		= {"1V8_CLK", 3300, 4096, 0, STM32_AIN(12)},
	[VMON_3V3]		= {"3V3", 2 * 3300, 4096, 0, STM32_AIN(13)},
	[VMON_3V3_CLK]		= {"3V3_CLK", 4950, 4096, 0, STM32_AIN(14)},
	[VMON_3V7]		= {"3V6", 2 * 3300, 4096, 0, STM32_AIN(15)},
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
#include "driver/temp_sensor/tmp112.h"

const struct tmp112_t tmp112_sensors[] = {
	{ I2C_PORT_DB0, TMP112_I2C_ADDR(0) },
	{ I2C_PORT_DB0, TMP112_I2C_ADDR(1) },
	{ I2C_PORT_DB1, TMP112_I2C_ADDR(0) },
	{ I2C_PORT_DB1, TMP112_I2C_ADDR(1) },
};
BUILD_ASSERT(ARRAY_SIZE(tmp112_sensors) == TMP112_COUNT);

const struct temp_sensor_t temp_sensors[] = {
	{"PMBUS-0", TEMP_SENSOR_TYPE_BOARD, pmbus_temp_get_val, PMBUS_ID0},
	{"PMBUS-1", TEMP_SENSOR_TYPE_BOARD, pmbus_temp_get_val, PMBUS_ID1},
	{"EC Internal", TEMP_SENSOR_TYPE_BOARD, ec_adc_get_val, ADC1_TEMPERATURE},
	{"TMP464 Internal", TEMP_SENSOR_TYPE_BOARD, tmp468_get_val, TMP468_LOCAL},
	{"Sample Clock PCB", TEMP_SENSOR_TYPE_BOARD, tmp468_get_val, TMP468_REMOTE1},
	{"RFSoC", TEMP_SENSOR_TYPE_CPU, tmp468_get_val, TMP468_REMOTE2},
	{"DRAM PCB", TEMP_SENSOR_TYPE_BOARD, tmp468_get_val, TMP468_REMOTE3},
	{"Power Supply PCB", TEMP_SENSOR_TYPE_BOARD, tmp468_get_val, TMP468_REMOTE4},
	{"TMP112 DB0 Top", TEMP_SENSOR_TYPE_BOARD, tmp112_get_val, 0},
	{"TMP112 DB0 Bottom", TEMP_SENSOR_TYPE_BOARD, tmp112_get_val, 1},
	{"TMP112 DB1 Top", TEMP_SENSOR_TYPE_BOARD, tmp112_get_val, 2},
	{"TMP112 DB1 Bottom", TEMP_SENSOR_TYPE_BOARD, tmp112_get_val, 3},
};
BUILD_ASSERT(ARRAY_SIZE(temp_sensors) == TEMP_SENSOR_COUNT);

static void tmp468_sensor_init(void)
{
	int ret;

	/*
	 * Refer TMP468 datasheet and Xilinx DS926 document
	 * https://www.xilinx.com/support/documentation/data_sheets/ds926-zynq-ultrascale-plus-rfsoc.pdf
	 * The DS926 mentions that the ideality factor (n-factor) for the
	 * temperature diode inside the RFSoC is 1.026.
	 * Use the formula from the TMP468 datasheet to convert this number to
	 * the value to be written to n-factor correction register.
	 */
	ret = tmp468_set_nfactor(2 /* RFSoC */, -37 /* NFACTOR CORRECTION */);

	ret |= tmp468_set_offset(2 /* RFSoC */, -1 /* deg C */);

	if (ret != EC_SUCCESS)
		ccprintf("warning! TMP468 init failed!"
			"Temp values may not be accurate!\n");

}
DECLARE_HOOK(HOOK_INIT, tmp468_sensor_init, HOOK_PRIO_TEMP_SENSOR);
#endif

#ifdef CONFIG_IO_EXPANDER
struct ioexpander_config_t ioex_config[] = {
	{ I2C_PORT_PWR, TCA6416_I2C_ADDR(0), &tca6416_ioexpander_drv },
	{ I2C_PORT_DB0_PWR, TCA6416_I2C_ADDR(0), &tca6416_ioexpander_drv },
	{ I2C_PORT_DB1_PWR, TCA6416_I2C_ADDR(0), &tca6416_ioexpander_drv },
	{ I2C_PORT_RTC, TCA6408_I2C_ADDR(1), &tca6408_ioexpander_drv },
#ifdef TITANIUM_ENABLE_RFCHAR_GPIO
	{ I2C_PORT_DB0, TCA6416_I2C_ADDR(0), &tca6416_ioexpander_drv },
	{ I2C_PORT_DB1, TCA6416_I2C_ADDR(0), &tca6416_ioexpander_drv },
#endif
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

/* Max and min rpm defaults are from the fan datasheet*/
/* Fan Configuration */
const struct fan_conf fan_conf_0 = {
	.flags = FAN_USE_RPM_MODE,
	.ch = 0,
	.pgood_gpio = -1,
	.enable_gpio = GPIO_FAN_0_EN,
};

const struct fan_conf fan_conf_1 = {
	.flags = FAN_USE_RPM_MODE,
	.ch = 1,
	.pgood_gpio = -1,
	.enable_gpio = GPIO_FAN_1_EN,
};

struct fan_rpm fan_rpm_0 = {
	.rpm_min = 4000,
	.rpm_start = 8000,
	.rpm_max = 16000,
};

struct fan_rpm fan_rpm_1 = {
	.rpm_min = 4000,
	.rpm_start = 8000,
	.rpm_max = 16000,
};

struct fan_t fans[] = {
	[FAN_CH_0] = { .conf = &fan_conf_0, .rpm = &fan_rpm_0, },
	[FAN_CH_1] = { .conf = &fan_conf_1, .rpm = &fan_rpm_1, },
};
BUILD_ASSERT(ARRAY_SIZE(fans) == FAN_CH_COUNT);
