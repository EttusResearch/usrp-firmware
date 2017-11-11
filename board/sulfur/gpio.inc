/* -*- mode:c -*-
 *
 * Copyright (c) 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Declare symbolic names for all the GPIOs that we care about.
 * Note: Those with interrupt handlers must be declared first. */

GPIO_INT(POWER_BUTTON_L,	PIN(B, 12), GPIO_INT_BOTH, power_button_interrupt)

/* power signals, these will get handled in power task */
GPIO_INT(POWER_PG_1V,		PIN(C, 3),  GPIO_INT_BOTH, power_signal_interrupt)
GPIO_INT(POWER_PG_1V5,		PIN(A, 1),  GPIO_INT_BOTH, power_signal_interrupt)
GPIO_INT(POWER_PG_3V7,		PIN(C, 4),  GPIO_INT_BOTH, power_signal_interrupt)
GPIO_INT(POWER_PG_IO,		PIN(A, 6),  GPIO_INT_BOTH, power_signal_interrupt)
GPIO_INT(POWER_PG_MGT,		PIN(B, 0),  GPIO_INT_BOTH, power_signal_interrupt)
GPIO_INT(SYS_RTC_INT,		PIN(A, 15), GPIO_INT_FALLING, wdt_reset_event)

GPIO_INT(SYS_PWRON_33,		PIN(C, 7),  GPIO_INT_BOTH, power_signal_interrupt)

GPIO_INT(SYS_FAN0_TACH,		PIN(B, 8), GPIO_INT_RISING, fan_tach_interrupt)
GPIO_INT(SYS_FAN1_TACH,		PIN(B, 9), GPIO_INT_RISING, fan_tach_interrupt)

/* Outputs */
GPIO(POWER_LED,			PIN(A, 8),  GPIO_OUT_HIGH)
GPIO(MISC_LED,			PIN(B, 2), GPIO_OUT_LOW)

GPIO(SYS_FAN0_PWM,		PIN(A, 2),  GPIO_OUT_HIGH)
GPIO(SYS_FAN1_PWM,		PIN(C, 6),  GPIO_OUT_HIGH)

GPIO(POE_SHDN,			PIN(C, 13), GPIO_OUT_LOW)
GPIO(PS_POR_RESET_L,		PIN(B, 15), GPIO_OUT_LOW)

/* MIO5 -> Default to SDBOOT (HIGH) */
GPIO(PS_BOOTSEL,                PIN(B, 13), GPIO_INPUT)
/* MIO2 -> Default to CASCADE (LOW) */
GPIO(JTAG_SEL,			PIN(B, 14), GPIO_INPUT)

/* EC_INT_L is called SYSCTRL_INTRQ in schematic */
GPIO(EC_INT_L,			PIN(C, 8),  GPIO_OUT_HIGH)

GPIO(SYS_GPIO0,			PIN(C, 10), GPIO_INPUT)
GPIO(SYS_GPIO1,			PIN(C, 11), GPIO_INPUT)
GPIO(SYS_GPIO2,			PIN(C, 12), GPIO_INPUT)
GPIO(SYS_GPIO3,			PIN(B, 3),  GPIO_INPUT)

/* Power enables */
GPIO(POWER_EN_1V_L,		PIN(C, 2), GPIO_OUT_HIGH | GPIO_PULL_UP)
GPIO(POWER_EN_1V5,		PIN(A, 3), GPIO_OUT_LOW | GPIO_PULL_DOWN)
GPIO(POWER_EN_3V7,		PIN(C, 5), GPIO_OUT_LOW | GPIO_PULL_DOWN)
GPIO(POWER_EN_IO,		PIN(C, 1), GPIO_OUT_LOW | GPIO_PULL_DOWN)
GPIO(POWER_EN_5V,		PIN(A, 5), GPIO_OUT_LOW | GPIO_PULL_DOWN)
GPIO(POWER_EN_MGT,		PIN(B, 1), GPIO_OUT_LOW | GPIO_PULL_DOWN)

/* Inputs */
GPIO(WP_L,			PIN(D, 2),  GPIO_INPUT)
GPIO(POE_T2P_L,			PIN(C, 14), GPIO_INPUT)
GPIO(AUXPWR_STAT,		PIN(C, 15), GPIO_INPUT)
GPIO(PWRSW_STAT,		PIN(F, 0),  GPIO_INPUT)
GPIO(PWRSW_CTRL,		PIN(F, 1),  GPIO_INPUT)
GPIO(POWER_PG_5V,		PIN(A, 4),  GPIO_INPUT)

GPIO(RESET_BUTTON_L,		PIN(C, 0),  GPIO_INPUT)

/* Unimplemented signals which we need to emulate for now */
UNIMPLEMENTED(ENTERING_RW)
UNIMPLEMENTED(LID_OPEN)
UNIMPLEMENTED(SUSPEND_L)

/* These can be used for strapping resistors to allow indicating
 * different options */
UNIMPLEMENTED(BOARD_VERSION1)
UNIMPLEMENTED(BOARD_VERSION2)
UNIMPLEMENTED(BOARD_VERSION3)

UNIMPLEMENTED(BAT_LED_RED)
UNIMPLEMENTED(BAT_LED_GREEN)

/*
 * I2C pins should be configured as inputs until I2C module is
 * initialized. This will avoid driving the lines unintentionally.
 */
GPIO(SLAVE_I2C_SCL,		PIN(B, 10), GPIO_INPUT)
GPIO(SLAVE_I2C_SDA,		PIN(B, 11), GPIO_INPUT)
GPIO(MASTER_I2C_SCL,		PIN(B, 6), GPIO_INPUT)
GPIO(MASTER_I2C_SDA,		PIN(B, 7), GPIO_INPUT)

/* Setup alternate functions as follows:
 * I2C1:	(PB6/PB7)	AF1
 * I2C2:	(PB10/PB11)	AF1
 * USART1:	(PA9/PA10)	AF1
 * TIM3:	(PC6)		AF0
 * TIM15:	(PA2)		AF0
 */
ALTERNATE(PIN_MASK(B, 0x00c0), 1, MODULE_I2C, 0)
ALTERNATE(PIN_MASK(B, 0x0c00), 1, MODULE_I2C, 0)
ALTERNATE(PIN_MASK(A, 0x0600), 1, MODULE_UART,0)
ALTERNATE(PIN_MASK(C, 0x0040), 0, MODULE_PWM, 0)
ALTERNATE(PIN_MASK(A, 0x0004), 0, MODULE_PWM, 0)
