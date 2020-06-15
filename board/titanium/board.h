/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* NI Project Titanium Firmware configuration file */

#ifndef __BOARD_H
#define __BOARD_H

/* 96 MHz CPU/AHB clock frequency (APB1/APB2 = 48 Mhz) */
#define CPU_CLOCK 96000000
#define CONFIG_FLASH_WRITE_SIZE STM32_FLASH_WRITE_SIZE_3300

/* the UART console is on USART1 (PA9/PA10) */
#undef CONFIG_UART_CONSOLE
#define CONFIG_UART_CONSOLE 1

#undef CONFIG_UART_TX_BUF_SIZE
#define CONFIG_UART_TX_BUF_SIZE 4096
#define CONFIG_UART_TX_REQ_CH STM32_REQ_USART1_TX
#define CONFIG_UART_RX_REQ_CH STM32_REQ_USART1_RX

/* Optional features */
#undef CONFIG_LID_SWITCH
#undef CONFIG_HIBERNATE
#define CONFIG_STM_HWTIMER32
#define CONFIG_WATCHDOG_HELP
#define CONFIG_TASK_PROFILING

#define CONFIG_AUX_TIMER_RATE_KHZ 10

#define CONFIG_I2C
#define CONFIG_PMBUS
#define CONFIG_CMD_PMBUS

#define CONFIG_TEMP_SENSOR
#define CONFIG_TEMP_SENSOR_PMBUS
#define CONFIG_CMD_TEMP_SENSOR

#define CONFIG_CMD_FLASH

#define CONFIG_CRC8
#define CONFIG_SW_CRC

/* I2C ports configuration */
#define CONFIG_I2C_MASTER
#define I2C_PORT_PMBUS 1
#define I2C_PORT_DB 2
#define I2C_PORT_THERMAL 15
#define I2C_PORT_SLAVE 0        /* needed for DMAC macros (ugh) */

/* I2C ports downstream of mux */
#define I2C_PORT_DB0 10
#define I2C_PORT_DB0_PWR 11
#define I2C_PORT_DB1 12
#define I2C_PORT_DB1_PWR 13
#define I2C_PORT_MON 14
#define I2C_PORT_TMP464 15
#define I2C_PORT_RTC 16 /* RTC and EEPROM */
#define I2C_PORT_PWR 17

#define CONFIG_I2C_MUX
#define CONFIG_I2C_MUX_TCA954X
#define CONFIG_CMD_I2C_MUX

#define CONFIG_I2C_SLAVE
#define CONFIG_HOSTCMD_EVENTS
#define CONFIG_HOSTCMD_I2C_SLAVE_ADDR_FLAGS 0x3c
#define I2C_PORT_EC I2C_PORT_SLAVE

#undef CONFIG_HOSTCMD_DEBUG_MODE
#define CONFIG_HOSTCMD_DEBUG_MODE HCDEBUG_OFF

#define CONFIG_CMD_FLASHINFO
#define CONFIG_CONSOLE_ENABLE_READ_V1

#define CONFIG_I2C_NO_PASSTHRU_PROTECTION

#define CONFIG_SWITCH
#define CONFIG_POWER_BUTTON
#define CONFIG_POWER_BUTTON_IGNORE_LID
#define CONFIG_CMD_POWERSTATS

/* Timer selection */
#define TIM_CLOCK32 2
#define TIM_WATCHDOG 5
#define TIM_CAPTURE_FAN0_1 9

#define CONFIG_WP_ALWAYS

#define CONFIG_PWM
#define CONFIG_ADC
#define CONFIG_CMD_ADC
#define CONFIG_CMD_ADC_READ

#define CONFIG_INA219
#define CONFIG_CMD_INA

#define I2C_PORT_MASTER 14

#define CONFIG_POWER_COMMON
#define CONFIG_CHIPSET_ZYNQMP
#define CONFIG_CMD_ZYNQMP

#define CONFIG_TEMP_SENSOR_EC_ADC
#define CONFIG_TEMP_SENSOR_TMP468
#define CONFIG_TEMP_SENSOR_TMP112
#define TMP112_COUNT 4
#define CONFIG_STM32_INTERNAL_TEMP
#define CONFIG_ADC_SAMPLE_TIME 7

#define CONFIG_IO_EXPANDER_TCA64XX
#define CONFIG_IO_EXPANDER
#define CONFIG_CMD_LED

#define CONFIG_FANS 2
#define CONFIG_FAN_RPM_CUSTOM
#define CONFIG_FAN_DYNAMIC
/* Percentage to which all fans are set at initiation */
#undef CONFIG_FAN_INIT_SPEED
#define CONFIG_FAN_INIT_SPEED 30
#define CONFIG_CMD_FANTEST
#define CONFIG_FAN_UPDATE_PERIOD 3
#define CONFIG_FAN_RPM_MUTABLE

#define CONFIG_KEYBOARD_PROTOCOL_MKBP
#define CONFIG_MKBP_EVENT
#define CONFIG_MKBP_USE_GPIO_AND_HOST_EVENT
/* Define the host events which are allowed to wakeup AP in S3. */
#define CONFIG_MKBP_HOST_EVENT_WAKEUP_MASK \
	EC_HOST_EVENT_MASK(EC_HOST_EVENT_POWER_BUTTON)

#define CONFIG_FPU

#ifndef __ASSEMBLER__

#undef TITANIUM_ENABLE_RFCHAR_GPIO

#ifdef CONFIG_IO_EXPANDER
enum ioex_ports {
	IOEX_PWR_DB,
	IOEX_DB0_PWR,
	IOEX_DB1_PWR,
	IOEX_CLK_DB,
#ifdef TITANIUM_ENABLE_RFCHAR_GPIO
    IOEX_RFCHAR_DB0,
    IOEX_RFCHAR_DB1,
#endif
	CONFIG_IO_EXPANDER_PORT_COUNT,
};
#endif

#define CONFIG_HOST_CONTROL_GPIO
#ifdef CONFIG_HOST_CONTROL_GPIO
enum host_control_gpios {
	USER_LED_G,
	USER_LED_R,
	PCIE_LED_G,
	PCIE_LED_R,
	RFDC_POWERED,

	HOST_CONTROL_GPIO_COUNT,
};
#endif

#define CONFIG_PWRSUP
enum pwrsup_id {
	POWER_SUPPLY_12V,
	POWER_SUPPLY_0V85,
	POWER_SUPPLY_1V8,
	POWER_SUPPLY_2V5,
	POWER_SUPPLY_3V3,
	POWER_SUPPLY_0V9,
	POWER_SUPPLY_MGTAUX,
	POWER_SUPPLY_DDR4N_VDDQ,
	POWER_SUPPLY_DDR4N_VTT,
	POWER_SUPPLY_DDR4S_VDDQ,
	POWER_SUPPLY_DDR4S_VTT,
	POWER_SUPPLY_3V6,
	POWER_SUPPLY_3V3CLK,
	POWER_SUPPLY_1V8CLK,
	POWER_SUPPLY_ADCVCC,
	POWER_SUPPLY_ADCVCCAUX,
	POWER_SUPPLY_DACVCC,
	POWER_SUPPLY_DACVCCAUX,
	POWER_SUPPLY_DACVTT,
	POWER_SUPPLY_RFDC,

	POWER_SUPPLY_CLKDB_3V3,
	POWER_SUPPLY_CLKDB_3V7,
	POWER_SUPPLY_CLKDB_12V,

	POWER_SUPPLY_DIO_12V,
	POWER_SUPPLY_DIO_1V2,
	POWER_SUPPLY_DIO_3V3,

	POWER_SUPPLY_DB0_12V,
	POWER_SUPPLY_DB0_3V3,
	POWER_SUPPLY_DB0_3V7,
	POWER_SUPPLY_DB0_2V5,
	POWER_SUPPLY_DB0_1V8,
	POWER_SUPPLY_DB0_3V3MCU,

	POWER_SUPPLY_DB1_12V,
	POWER_SUPPLY_DB1_3V3,
	POWER_SUPPLY_DB1_3V7,
	POWER_SUPPLY_DB1_2V5,
	POWER_SUPPLY_DB1_1V8,
	POWER_SUPPLY_DB1_3V3MCU,

	POWER_SUPPLY_COUNT,
};

#ifdef CONFIG_ADC
enum adc_channel {
	VMON_VBATT = 0,
	ADC1_TEMPERATURE,
	ADC1_17,
	VMON_0V9,
	VMON_0V85,
	VMON_0V6_DDR_VREF,
	VMON_0V925_ADC_DAC,
	VMON_1V2_DDRS,
	VMON_1V2_DDRN,
	VMON_0V6_DDR_VTT,
	VMON_1V8_ADC_DAC_AUX,
	VMON_1V8,
	VMON_2V5,
	VMON_2V5_DAC_VTT,
	VMON_VIN_IMON,
	VMON_1V8_CLK,
	VMON_3V3,
	VMON_3V3_CLK,
	VMON_3V7,

	/* Number of ADC Channels */
	ADC_CH_COUNT
};
#endif

enum power_signal {
	MASTER_PG_MCU = 0,
	PS_DONE,
	PS_INIT_L,
	PS_PWR_GOOD,
	PS_ERR_STAT,
	BUT_RESET_L,
	PS_PWR_REQUIRED,
	/* Number of power signals */
	POWER_SIGNAL_COUNT
};

#ifdef CONFIG_PWM
enum pwm_channel {
	PWM_CH_FAN0 = 0,
	PWM_CH_FAN1,
	/* Number of PWM Channels */
	PWM_CH_COUNT
};
#endif

#ifdef CONFIG_PMBUS
enum pmbus_id {
	PMBUS_ID0 = 0,
	PMBUS_ID1,
	/* Number of PMBUS devices */
	PMBUS_DEV_COUNT,
};
#endif

#ifdef CONFIG_TEMP_SENSOR
enum temp_sensor_id {
	TEMP_SENSOR_PMBUS_0 = 0,
	TEMP_SENSOR_PMBUS_1,
	TEMP_SENSOR_INTERNAL,
	TEMP_SENSOR_TMP464_INTERNAL,
	TEMP_SENSOR_SAMPLE_CLOCK_PCB,
	TEMP_SENSOR_RFSOC_DIE,
	TEMP_SENSOR_DRAM_PCB,
	TEMP_SENSOR_POWER_SUPPLY_PCB,
	TEMP_SENSOR_DB0_TOP,
	TEMP_SENSOR_DB0_BOTTOM,
	TEMP_SENSOR_DB1_TOP,
	TEMP_SENSOR_DB1_BOTTOM,

	/* Number of temperature sensors */
	TEMP_SENSOR_COUNT,
};
#endif

#ifdef CONFIG_I2C_MUX
enum i2c_mux_id {
	I2C_MUX_MB = 0,

	/* Number of I2C muxes */
	I2C_MUX_COUNT
};
#endif

#ifdef CONFIG_INA219
enum ina2xx_id {
	INA2XX_0V9,
	INA2XX_1V8,
	INA2XX_3V6,
	INA2XX_3V3,
	INA2XX_2V5,
	INA2XX_1V2N,
	INA2XX_1V2S,
};
#define INA2XX_COUNT 7
#endif

enum fan_channel {
	FAN_CH_0 = 0,
	FAN_CH_1,

	/* Number of FAN channels */
	FAN_CH_COUNT
};

#include "gpio_signal.h"

#endif /* !__ASSEMBLER__ */

#endif /* __BOARD_H */
