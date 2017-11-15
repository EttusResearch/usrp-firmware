/* Copyright (c) 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Sulfur board configuration */

#ifndef __CROS_EC_BOARD_H
#define __CROS_EC_BOARD_H

/* 48 MHz SYSCLK clock frequency */
#define CPU_CLOCK 48000000

#define CONFIG_AP_HANG_DETECT

#define CONFIG_BOARD_VERSION
#define CONFIG_BOARD_SPECIFIC_VERSION

#define CONFIG_BOARD_PRE_INIT
#define CONFIG_CMD_FLASH
#define CONFIG_FORCE_CONSOLE_RESUME
#undef CONFIG_HOST_COMMAND_STATUS
#define CONFIG_I2C

#define CONFIG_SULFUR_5V_WORKAROUND

#define CONFIG_FAN_RPM_CUSTOM

#define CONFIG_POWER_BUTTON
#define CONFIG_POWER_COMMON
#define CONFIG_POWER_BUTTON_ACTIVE_STATE 0
#define CONFIG_POWERLED
#define CONFIG_LED_COMMON
#define CONFIG_LED_POLICY_STD

#define CONFIG_LED_POWER_ACTIVE_LOW

#define CONFIG_STM_HWTIMER32
#define CONFIG_SWITCH
#define CONFIG_PWM

#define CONFIG_THROTTLE_AP

#define CONFIG_HW_CRC

#undef CONFIG_UART_CONSOLE
#define CONFIG_UART_CONSOLE 1

#undef CONFIG_HIBERNATE

#define CONFIG_MKBP_EVENT
#define CONFIG_HOSTCMD_EVENTS

#define CONFIG_INPUT_CAPTURE

#undef CONFIG_LID_SWITCH

#undef CONFIG_WATCHDOG_HELP

#define CONFIG_TEMP_SENSOR_TMP431

#define CONFIG_TEMP_SENSOR
#define CONFIG_CMD_TEMP_SENSOR

/* Single I2C port, where the EC is the master. */
#define CONFIG_I2C
#define CONFIG_I2C_SLAVE
#define CONFIG_I2C_MASTER

#ifdef HAS_TASK_HOSTCMD
#define CONFIG_HOSTCMD_I2C_SLAVE_ADDR 0x3c
#endif

#define I2C_PORT_SLAVE  1
#define I2C_PORT_MASTER  0
#define I2C_PORT_EC I2C_PORT_SLAVE
#define I2C_PORT_THERMAL I2C_PORT_MASTER
#define CONFIG_HOSTCMD_ALIGNED

/* Timer selection */
#define TIM_CLOCK32 2
#define TIM_WATCHDOG 4

#define CONFIG_FANS 2

#ifndef __ASSEMBLER__

#define EEPROM_BLOCK_START_PSTORE 0
#define EEPROM_BLOCK_COUNT_PSTORE 1024

#include "gpio_signal.h"

enum pwm_channel {
	PWM_CH_FAN0 = 0,
	PWM_CH_FAN1,

	/* Number of PWM channels */
	PWM_CH_COUNT
};

enum power_signal {
	SYS_PWRON_33 = 0,
	POWER_PG_1V,
	POWER_PG_1V5,
	POWER_PG_IO,
	POWER_PG_3V7,
#ifndef CONFIG_SULFUR_5V_WORKAROUND
	POWER_PG_5V,
#endif
	POWER_PG_MGT,
	RESET_BUTTON_L,

	/* Number of power signals */
	POWER_SIGNAL_COUNT
};

enum temp_sensor_id {
	TEMP_SENSOR_LOCAL = 0,
	TEMP_SENSOR_REMOTE,

	TEMP_SENSOR_COUNT
};

enum fan_channel {
	FAN_CH_0,
	FAN_CH_1,

	/* Number of FAN channels */
	FAN_CH_COUNT
};

/* Board specific handlers */
int board_get_version(void);

#endif /* !__ASSEMBLER__ */
#endif /* __CROS_EC_BOARD_H */
