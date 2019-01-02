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

/* Optional features */
#undef CONFIG_LID_SWITCH
#undef CONFIG_HIBERNATE
#define CONFIG_STM_HWTIMER32
#define CONFIG_WATCHDOG_HELP
#define CONFIG_TASK_PROFILING

#define CONFIG_I2C

#undef CONFIG_UART_RX_DMA
#define CONFIG_UART_TX_DMA_CH STM32_DMAS_USART2_TX
#define CONFIG_UART_RX_DMA_CH STM32_DMAS_USART2_RX
#define CONFIG_UART_TX_REQ_CH STM32_REQ_USART2_TX
#define CONFIG_UART_RX_REQ_CH STM32_REQ_USART2_RX

#define CONFIG_CMD_FLASH
#define CONFIG_CMD_RTC
#define CONFIG_HOSTCMD_RTC

#define CONFIG_CRC8

/* I2C ports configuration */
#define CONFIG_I2C_MASTER
#define CONFIG_I2C_DEBUG
#define I2C_PORT_PMBUS 1
#define I2C_PORT_DB 2
#define I2C_PORT_SLAVE 0        /* needed for DMAC macros (ugh) */

#define CONFIG_SWITCH
#define CONFIG_POWER_BUTTON
#define CONFIG_POWER_BUTTON_IGNORE_LID

#ifndef __ASSEMBLER__

/* Timer selection */
#define TIM_CLOCK32 2
#define TIM_WATCHDOG 11

#define CONFIG_WP_ALWAYS

#define CONFIG_PWM

enum pwm_channel {
	PWM_CH_FAN0 = 0,
	PWM_CH_FAN1,
	/* Number of PWM Channels */
	PWM_CH_COUNT
};

#include "gpio_signal.h"

#endif /* !__ASSEMBLER__ */

#endif /* __BOARD_H */
