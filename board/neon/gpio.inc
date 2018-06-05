/* -*- mode:c -*-
 *
 * Copyright (c) 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Declare symbolic names for all the GPIOs that we care about.
 * Note: Those with interrupt handlers must be declared first. */

GPIO_INT(POWER_BUTTON_L,	PIN(B, 13), GPIO_INT_BOTH, power_button_interrupt)

/* power signals, these will get handled in power task */
GPIO_INT(PWR_1V0_PG,		PIN(C, 8), GPIO_INT_BOTH, power_signal_interrupt)
GPIO_INT(PWR_1V3_PG,		PIN(C, 9), GPIO_INT_BOTH, power_signal_interrupt)
GPIO_INT(PWR_1V5_PG,		PIN(C, 7), GPIO_INT_BOTH, power_signal_interrupt)
GPIO_INT(PWR_1V8_PG,		PIN(C, 5), GPIO_INT_BOTH, power_signal_interrupt)
GPIO_INT(PWR_3V3_PG,		PIN(C, 3), GPIO_INT_BOTH, power_signal_interrupt)
GPIO_INT(PWR_3V8_PG,		PIN(C, 11), GPIO_INT_BOTH, power_signal_interrupt)
GPIO_INT(PWR_MGTVTT_PG,		PIN(C, 10), GPIO_INT_BOTH, power_signal_interrupt)
GPIO_INT(PWR_MGTVCC_PG,		PIN(C, 12), GPIO_INT_BOTH, power_signal_interrupt)
GPIO_INT(SYS_PS_PWRON,		PIN(B, 0),  GPIO_INT_BOTH, power_signal_interrupt)

/* Outputs */
GPIO(POWER_LED,			PIN(A, 8),  GPIO_OUT_HIGH)
GPIO(SYS_LED,			PIN(B, 8),  GPIO_OUT_LOW)

GPIO(SYS_FAN_SENSE_BUFF,	PIN(A, 2),  GPIO_OUT_HIGH)

GPIO(PS_POR_RESET_L,		PIN(B, 14), GPIO_OUT_LOW)
GPIO(PWR_CLK_EN,		PIN(B, 9),  GPIO_OUT_LOW)

/* EC_INT_L is called SYS_PS_INTRQ in schematic */
GPIO(EC_INT_L,			PIN(B, 1),  GPIO_OUT_HIGH)

/* Power enables */
GPIO(PWR_1V8_EN,		PIN(A, 0), GPIO_OUT_LOW | GPIO_PULL_DOWN)
GPIO(PWR_1V5_EN,		PIN(A, 1), GPIO_OUT_LOW | GPIO_PULL_DOWN)
GPIO(PWR_3V8_EN,		PIN(A, 3), GPIO_OUT_LOW | GPIO_PULL_DOWN)
GPIO(PWR_MGTVTT_EN,		PIN(A, 5), GPIO_OUT_LOW | GPIO_PULL_DOWN)
GPIO(PWR_1V3_EN,		PIN(A, 6), GPIO_OUT_LOW | GPIO_PULL_DOWN)
GPIO(PWR_1V0_EN_L,		PIN(A, 7), GPIO_OUT_HIGH | GPIO_PULL_UP)
GPIO(PWR_MGTVCC_EN,		PIN(B, 2), GPIO_OUT_LOW | GPIO_PULL_DOWN)
GPIO(PWR_3V3_EN,		PIN(C, 0), GPIO_OUT_LOW | GPIO_PULL_DOWN)

/* Fan PWM output */
GPIO(SYS_FAN_CTRL_RAW,		PIN(C, 6), GPIO_INPUT)
GPIO(FAN_EN,			PIN(A, 15), GPIO_OUT_LOW | GPIO_PULL_UP)

GPIO(SYS_UART_RST_L,		PIN(C, 4),  GPIO_OUT_HIGH | GPIO_PULL_UP)
GPIO(SYS_HUB_RST_L,		PIN(C, 1),  GPIO_OUT_HIGH | GPIO_PULL_UP)

/* Inputs */
GPIO(WP_L,			PIN(D, 2),  GPIO_INPUT)
GPIO(SYS_TEMP_ALT1,		PIN(B, 4), GPIO_INPUT)
GPIO(SYS_TEMP_ALT2,		PIN(B, 5), GPIO_INPUT)

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
 * TIM16:	(PB8)		AF2
 * TIM17:	(PB9)		AF2
 */
ALTERNATE(PIN_MASK(B, 0x00c0), 1, MODULE_I2C, 0)
ALTERNATE(PIN_MASK(B, 0x0c00), 1, MODULE_I2C, 0)
ALTERNATE(PIN_MASK(A, 0x0600), 1, MODULE_UART,0)
ALTERNATE(PIN_MASK(C, 0x0040), 0, MODULE_PWM, 0)
ALTERNATE(PIN_MASK(A, 0x0004), 0, MODULE_FAN, 0)
