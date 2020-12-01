/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
/* NI Project Titanium Firmware */

#include "adc.h"
#include "adc_chip.h"
#include "board_power.h"
#include "common.h"
#include "console.h"
#include "gpio.h"
#include "hooks.h"
#include "ina2xx.h"
#include "ioexpander.h"
#include "led.h"
#include "power.h"
#include "pmbus.h"
#include "system.h"
#include "temp_sensor.h"
#include "timer.h"
#include "util.h"

struct rail_monitor {
	uint8_t adc_channel; /* voltage monitor */
	int (*get_power)(uint8_t);
	uint8_t priv; /* arg to get_power */
};

static int get_0v85_power(uint8_t ignored)
{
	int val, v, curr = 0;

	for (size_t i = 0; i < PMBUS_DEV_COUNT; i++) {
		val = 0;
		pmbus_read_curr_out(i, &val);
		curr += val;
	}

	v = adc_read_channel(VMON_0V85);

	return (curr * v) / 1000 /*mW*/;
}

static const struct rail_monitor rail_monitors[] = {
	{ VMON_VBATT },
	{ ADC1_17 },
	{ VMON_0V9, ina2xx_get_power, INA2XX_0V9},
	{ VMON_0V85, get_0v85_power },
	{ VMON_0V6_DDR_VREF },
	{ VMON_0V925_ADC_DAC },
	{ VMON_1V2_DDRS, ina2xx_get_power, INA2XX_1V2S},
	{ VMON_1V2_DDRN, ina2xx_get_power, INA2XX_1V2N},
	{ VMON_0V6_DDR_VTT },
	{ VMON_1V8_ADC_DAC_AUX },
	{ VMON_1V8, ina2xx_get_power, INA2XX_1V8 },
	{ VMON_2V5, ina2xx_get_power, INA2XX_2V5 },
	{ VMON_3V0_DAC_VTT },
	{ VMON_1V8_CLK },
	{ VMON_3V3, ina2xx_get_power, INA2XX_3V3 },
	{ VMON_3V3_CLK, },
	{ VMON_3V7, ina2xx_get_power, INA2XX_3V6},
};
#define RAIL_MONITOR_COUNT 17
BUILD_ASSERT(ARRAY_SIZE(rail_monitors) == RAIL_MONITOR_COUNT);

struct pmbus_measurement {
	int voltage;
	int current;
};

struct ina_measurement {
	int shunt_voltage;
	int bus_voltage;
	int power;
	int current;
};

#ifdef CONFIG_CMD_POWERSTATS
static int command_powerstats(int argc, char **argv)
{
	int rv;
	int v, curr, show_details = 0, dump_all = 0, input_power;
	int temps[TEMP_SENSOR_COUNT] = {0};
	int adc_measurements[RAIL_MONITOR_COUNT];
	struct ina_measurement ina_measurements[INA2XX_COUNT];
	struct pmbus_measurement pmbus_measurements[PMBUS_DEV_COUNT];
	static const char temp_error[] = "Temp Error";

	if (argc == 1) {
	/* NOP */
	} else if (argc == 2) {
		if (!strcasecmp(argv[1], "details"))
			show_details = 1;
		else if (!strcasecmp(argv[1], "dump"))
			dump_all = 1;
		else
			return EC_ERROR_PARAM1;
	} else {
		return EC_ERROR_PARAM_COUNT;
	}

	/* Store measurements */
	for (size_t i = 0; i < TEMP_SENSOR_COUNT; i++) {
		rv = temp_sensor_read(i, &temps[i] /* deg K */);

		switch (rv) {
		case EC_SUCCESS:
			break;
		case EC_ERROR_NOT_POWERED:
		case EC_ERROR_NOT_CALIBRATED:
		default:
			temps[i] = -1; /* Store error condition */
		}
	}

	for (size_t i = 0; i < RAIL_MONITOR_COUNT; i++)
		adc_measurements[i] = adc_read_channel(rail_monitors[i].adc_channel);

	for (size_t i = 0; i < INA2XX_COUNT; i++) {
		ina_measurements[i].shunt_voltage = ina2xx_get_shunt_voltage(i);
		ina_measurements[i].bus_voltage = ina2xx_get_voltage(i);
		ina_measurements[i].power = ina2xx_get_power(i);
		ina_measurements[i].current = ina2xx_get_current(i);
	}

	for (size_t i = 0; i < PMBUS_DEV_COUNT; i++) {
		v = 0;
		pmbus_read_volt_out(i, &v);
		curr = 0;
		pmbus_read_curr_out(i, &curr);

		pmbus_measurements[i].voltage = v;
		pmbus_measurements[i].current = curr;
	}

	/* LTC4234 Monitor Current (IMON) Calculation */
	/* imon current = ((voltage across 20k resistor) / (full voltage range 2000 mV)) * (full scale current 20000 mA) */
	curr = adc_read_channel(VMON_VIN_IMON) * 10;
	/* Use the bus voltage from the INA219 sensor which is physically
	closest to the power daughter board connector */
	v = ina2xx_get_voltage(INA2XX_3V3);
	input_power = (v * curr) / 1000/*mW*/;

	if (dump_all) {

		/* Dump them all! */
		ccprintf("\n**** All Metrics ****\n");
		for (size_t i = 0; i < RAIL_MONITOR_COUNT; i++) {
			const struct rail_monitor *rail_mon = rail_monitors + i;

			ccprintf("%s Voltage,%dmV\n",
				adc_channels[rail_mon->adc_channel].name,
				adc_measurements[i]);

			if (rail_mon->get_power)
				ccprintf("%s Power,%dmW\n",
					adc_channels[rail_mon->adc_channel].name,
					rail_mon->get_power(rail_mon->priv));
		}

		ccprintf("Input Power,%dmW\n", input_power);

		for (size_t i = 0; i < INA2XX_COUNT; i++) {
			ccprintf("%s Shunt Voltage,%duV\n", ina2xx_sensors[i].name,
				ina_measurements[i].shunt_voltage);
			ccprintf("%s Bus Voltage,%dmV\n", ina2xx_sensors[i].name,
				ina_measurements[i].bus_voltage);
			ccprintf("%s Power,%dmW\n", ina2xx_sensors[i].name,
				ina_measurements[i].power);
			ccprintf("%s Current,%dmA\n", ina2xx_sensors[i].name,
				ina_measurements[i].current);
		}

		for (size_t i = 0; i < TEMP_SENSOR_COUNT; i++) {
			ccprintf("%s Temperature,", temp_sensors[i].name);
			if (temps[i] != -1)
				ccprintf("%dC\n", K_TO_C(temps[i]));
			else
				ccprintf("%s\n", temp_error);
		}

		for (size_t i = 0; i < PMBUS_DEV_COUNT; i++) {
			ccprintf("PMBUS %s Voltage,%dmV\n", pmbus_devs[i].name,
					pmbus_measurements[i].voltage);
			ccprintf("PMBUS %s Current,%dmA\n", pmbus_devs[i].name,
				pmbus_measurements[i].current);
		}

		return EC_SUCCESS;
	}

	/* Tabulated information for power rails */
	ccprintf("\n**** Summary ****\n");
	ccprintf("%-25s%-20s%-20s\n", "Name", "Voltage (mV)", "Power (mW)");
	for (size_t i = 0; i < RAIL_MONITOR_COUNT; i++) {
		const struct rail_monitor *rail_mon = rail_monitors + i;

		ccprintf("%-25s%-20d",
			adc_channels[rail_mon->adc_channel].name,
			adc_measurements[i]);

		if (rail_mon->get_power)
			ccprintf("%-20d\n", rail_mon->get_power(rail_mon->priv));
		else
			ccprintf("%-20s\n", "NA");
	}

	ccprintf("%-25s%-20d%-20d\n", "Input Power", v, input_power);

	ccprintf("\n**** Temperatures ****\n");
	for (size_t i = 0; i < TEMP_SENSOR_COUNT; i++) {
		ccprintf("%-20s: ", temp_sensors[i].name);
		if (temps[i] != -1)
			ccprintf("%dC\n", K_TO_C(temps[i]));
		else
			ccprintf("%s\n", temp_error);
	}

	if (show_details) {
		ccprintf("\n**** INA Current Monitoring Metrics ****\n");
		ccprintf("%-25s%-20s%-20s%-20s%-20s\n", "Name", "Shunt Voltage (uV)", "Bus Voltage (mV)", "Power (mW)", "Current (mA)");
		for (size_t i = 0; i < INA2XX_COUNT; i++)
			ccprintf("%-25s%-20d%-20d%-20d%-20d\n",
				ina2xx_sensors[i].name,
				ina_measurements[i].shunt_voltage,
				ina_measurements[i].bus_voltage,
				ina_measurements[i].power,
				ina_measurements[i].current);

		ccprintf("\n**** PM Bus Metrics ****\n");
		for (size_t i = 0; i < PMBUS_DEV_COUNT; i++)
			ccprintf("%-20s: %d mV %d mA\n", pmbus_devs[i].name,
				pmbus_measurements[i].voltage,
				pmbus_measurements[i].current);

	}

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(powerstats, command_powerstats,
			"[details|dump]",
			"Get motherboard power metrics.");
#endif

static enum power_status board_power_status = POWER_OFF;
static void pwr_status(void)
{
	switch (board_power_status) {
	case POWER_OFF:
		set_pwrdb_led_color(LED_ID_PWR, LED_OFF, 0);
		set_pwrdb_led_color(LED_ID_PWR_BUTTON, LED_OFF, 0);
		break;
	case POWER_INPUT_GOOD:
		set_pwrdb_led_color(LED_ID_PWR, LED_AMBER, 0);
		set_pwrdb_led_color(LED_ID_PWR_BUTTON, LED_OFF, 0);
		break;
	case POWER_INPUT_BAD:
		/* TODO: Add blinking red support? */
		set_pwrdb_led_color(LED_ID_PWR, LED_RED, 0);
		set_pwrdb_led_color(LED_ID_PWR_BUTTON, LED_RED, 0);
		break;
	case POWER_GOOD:
		set_pwrdb_led_color(LED_ID_PWR, LED_GREEN, 0);
		set_pwrdb_led_color(LED_ID_PWR_BUTTON, LED_GREEN, 0);
		break;
	case POWER_BAD:
		set_pwrdb_led_color(LED_ID_PWR, LED_RED, 0);
		set_pwrdb_led_color(LED_ID_PWR_BUTTON, LED_RED, 0);
		break;
	}
}

void set_board_power_status(enum power_status status)
{
	board_power_status = status;
	pwr_status();
}

enum power_status get_board_power_status(void)
{
	return board_power_status;
}

void power_signal_changed(void)
{
	int is_jumped = system_jumped_to_this_image();
	if (is_jumped == 0) {
		int v;
		v = gpio_get_level(GPIO_BUT_RESET_L);

		if (v)
			set_board_power_status(POWER_INPUT_GOOD);
		else
			set_board_power_status(POWER_OFF);
	}
}
DECLARE_HOOK(HOOK_INIT, power_signal_changed, HOOK_PRIO_DEFAULT);
