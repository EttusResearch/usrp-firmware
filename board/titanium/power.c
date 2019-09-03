/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
/* NI Project Titanium Firmware */

#include "adc.h"
#include "adc_chip.h"
#include "common.h"
#include "console.h"
#include "ina2xx.h"
#include "power.h"
#include "pmbus.h"
#include "temp_sensor.h"
#include "util.h"

struct rail_monitor {
	uint8_t adc_channel; /* voltage monitor */
	int ina_idx; /* current monitor */
};

static const struct rail_monitor rail_monitors[] = {
	{ADC1_18, -1},
	{ADC1_17, -1},
	{VMON_0V9, INA2XX_0V9},
	{VMON_0V85, -1},
	{VMON_0V6_DDR_VREF, -1},
	{VMON_0V925_ADC_DAC, -1},
	{VMON_1V2_DDRS, INA2XX_1V2S},
	{VMON_1V2_DDRN, INA2XX_1V2N},
	{VMON_0V6_DDR_VTT, -1},
	{VMON_1V8_ADC_DAC_AUX, -1},
	{VMON_1V8, INA2XX_1V8},
	{VMON_2V5, INA2XX_2V5},
	{VMON_2V5_DAC_VTT, -1},
	{VMON_1V8_CLK, -1},
	{VMON_3V3, INA2XX_3V3},
	{VMON_3V3_CLK, -1},
	{VMON_3V7, INA2XX_3V6},
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
	int v, curr, show_details = 0, dump_all = 0, input_power, rail_0V85_power;
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

	v = adc_read_channel(VMON_0V85);
	/* Total current = Master Current + Slave Current */
	rail_0V85_power = ((pmbus_measurements[0].current + pmbus_measurements[1].current) * v) / 1000 /*mW*/;

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
			ccprintf("%s Voltage,%dmV\n",
				adc_channels[rail_monitors[i].adc_channel].name,
				adc_measurements[i]);
			if (rail_monitors[i].ina_idx == -1)
				if (rail_monitors[i].adc_channel == VMON_0V85)
					ccprintf("%s Power,%dmW\n",
						adc_channels[rail_monitors[i].adc_channel].name,
						rail_0V85_power);
				else
					ccprintf("%s Power,%s\n",
						adc_channels[rail_monitors[i].adc_channel].name,
						"NA");
			else
				ccprintf("%s Power,%dmW\n",
					adc_channels[rail_monitors[i].adc_channel].name,
					ina2xx_get_power(rail_monitors[i].ina_idx));
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
		if (rail_monitors[i].ina_idx == -1)
			if (rail_monitors[i].adc_channel == VMON_0V85)
				ccprintf("%-25s%-20d%-20d\n",
					adc_channels[rail_monitors[i].adc_channel].name,
					adc_measurements[i],
					rail_0V85_power);
			else
				ccprintf("%-25s%-20d%-20s\n",
					adc_channels[rail_monitors[i].adc_channel].name,
					adc_measurements[i],
					"NA");
		else
			ccprintf("%-25s%-20d%-20d\n",
				adc_channels[rail_monitors[i].adc_channel].name,
				adc_measurements[i],
				ina2xx_get_power(rail_monitors[i].ina_idx));
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
