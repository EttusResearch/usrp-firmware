/* -*- mode:c -*-
 *
 * Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

GPIO_INT(POWER_BUTTON_L,   PIN(D, 10), GPIO_INT_BOTH, power_button_interrupt)

GPIO_INT(MASTER_PG_MCU,    PIN(B, 5),  GPIO_INT_BOTH, power_signal_interrupt)
GPIO_INT(PS_DONE,          PIN(C, 13), GPIO_INT_BOTH, power_signal_interrupt)
GPIO_INT(PS_INIT_L,        PIN(D, 4),  GPIO_INT_BOTH, power_signal_interrupt)
GPIO_INT(PS_ERR_OUT,       PIN(D, 6),  GPIO_INT_BOTH, power_signal_interrupt)
GPIO_INT(PS_ERR_STAT,      PIN(D, 7),  GPIO_INT_BOTH, power_signal_interrupt)
GPIO_INT(BUT_RESET_L,      PIN(D, 9),  GPIO_INT_BOTH, power_signal_interrupt)



/* Analog pins */
GPIO(0V9_VMON,             PIN(A, 0),  GPIO_ANALOG)
GPIO(CORE_VMON_0V85,       PIN(A, 1),  GPIO_ANALOG)
GPIO(0V925_DAC_AVCC_VMON,  PIN(A, 2),  GPIO_ANALOG)
GPIO(0V925_ADC_AVCC_VMON,  PIN(A, 3),  GPIO_ANALOG)
GPIO(DDRS_VMON_1V2,        PIN(A, 4),  GPIO_ANALOG)
GPIO(DDRN_VMON_1V2,        PIN(A, 5),  GPIO_ANALOG)
GPIO(1V8_ADC_AVCCAUX_VMON, PIN(A, 6),  GPIO_ANALOG)
GPIO(1V8_DAC_AVCCAUX_VMON, PIN(A, 7),  GPIO_ANALOG)
/* Make input till actually set by I2C code */
GPIO(DB_SWITCH_I2C_SCL,    PIN(A, 8),  GPIO_INPUT)
GPIO(UART_RX_SCU,          PIN(A, 9),  GPIO_INPUT)
GPIO(UART_TX_SCU,          PIN(A, 10), GPIO_OUTPUT)
/* TODO: CAN0/1 */
GPIO(M3_BUS_0_CAN_TX,      PIN(A, 11), GPIO_OUTPUT)
GPIO(M3_BUS_0_CAN_RX,      PIN(A, 12), GPIO_INPUT)
/* PA13, PA14, PA15 are JTAG */

GPIO(VMON_1V8,             PIN(B, 0),  GPIO_ANALOG)
GPIO(VMON_2V5,             PIN(B, 1),  GPIO_ANALOG)
GPIO(DDR4S_VDDQ_EN,        PIN(B, 2),  GPIO_OUT_LOW)
/* PB3, PB4 are JTAG */
/* TODO: This should be an IRQ */
/*GPIO(MASTER_PG_MCU,        PIN(B, 5),  GPIO_INPUT) */
GPIO(FAN1_TACH,            PIN(B, 6),  GPIO_INPUT)
GPIO(FAN1_PWM,             PIN(B, 7),  GPIO_OUTPUT)
/* These are called PS_STM_I2C_SCL/SCA in schematic */
GPIO(SLAVE_I2C_SCL,        PIN(B, 8),  GPIO_INPUT)
GPIO(SLAVE_I2C_SDA,        PIN(B, 9),  GPIO_INPUT)
/* These are called CORE_PMB_CLK/DAT in schematic */
GPIO(CORE_PMB_CLK,         PIN(B, 10), GPIO_INPUT)
GPIO(CORE_PMB_DAT,         PIN(B, 11), GPIO_INPUT)
/* TODO: This should be an IRQ? */
GPIO(CORE_PMB_ALERT,       PIN(B, 12), GPIO_INPUT)
GPIO(3V3_EN,               PIN(B, 13), GPIO_OUT_LOW)
GPIO(2V5_EN,               PIN(B, 14), GPIO_OUT_LOW)
GPIO(3V6_EN,               PIN(B, 15), GPIO_OUT_LOW)

GPIO(2V5_DAC_VTT_VMON_s,   PIN(C, 0),  GPIO_ANALOG)
GPIO(VIN_IMON_s,           PIN(C, 1),  GPIO_ANALOG)
GPIO(1V8_CLK_VMON_DIV2,    PIN(C, 2),  GPIO_ANALOG)
GPIO(3V3_VMON_DIV2,        PIN(C, 3),  GPIO_ANALOG)
GPIO(3V3_CLK_VMON_DIV2,    PIN(C, 4),  GPIO_ANALOG)
GPIO(3V6_VMON_DIV2,        PIN(C, 5),  GPIO_ANALOG)
GPIO(FAN_0_EN,             PIN(C, 6),  GPIO_OUT_LOW)
GPIO(FAN_1_EN,             PIN(C, 7),  GPIO_OUT_LOW)
GPIO(DACVCC_EN,            PIN(C, 8),  GPIO_OUT_LOW)
/* Make input till actually set by I2C code */
GPIO(DB_SWITCH_I2C_SDA,    PIN(C, 9),  GPIO_INPUT)
GPIO(DDR4N_VTT_EN,         PIN(C, 10), GPIO_OUT_LOW)
GPIO(ADC_VCCAUX_EN,        PIN(C, 11), GPIO_OUT_LOW)
GPIO(3V3_CLK_EN,           PIN(C, 12), GPIO_OUT_LOW)
/* This should be an IRQ? */
//GPIO(PS_DONE,              PIN(C, 13), GPIO_INPUT)//
/* PC14 and PC15 are OSC in/out */

GPIO(PS_MODE_0,            PIN(D, 0),  GPIO_OUTPUT)
GPIO(PS_MODE_1,            PIN(D, 1),  GPIO_OUTPUT)
GPIO(PS_MODE_2,            PIN(D, 2),  GPIO_OUTPUT)
UNIMPLEMENTED(PS_MODE_3)

GPIO(MCU_LED_L,            PIN(D, 3),  GPIO_OUTPUT)
/* Should these be IRQs? */
/*GPIO(PS_INIT_L,            PIN(D, 4),  GPIO_INPUT) */
GPIO(PS_PROG_L,            PIN(D, 5),  GPIO_INPUT)
/*GPIO(PS_ERR_OUT,           PIN(D, 6),  GPIO_INPUT) */
/*GPIO(PS_ERR_STAT,          PIN(D, 7),  GPIO_INPUT) */
/* D8 is missing in our package */
/*GPIO(BUT_RESET_L,          PIN(D, 9),  GPIO_INPUT)*/
/* see above for PD10 with IRQs */
GPIO(DDR4N_VDDQ_EN,        PIN(D, 11), GPIO_OUT_LOW)
GPIO(CORE_PMB_CNTL,        PIN(D, 12), GPIO_OUT_LOW)
GPIO(0V9_EN,               PIN(D, 13), GPIO_OUT_LOW)
GPIO(1V8_EN,               PIN(D, 14), GPIO_OUT_LOW)
GPIO(ADCVCC_EN,            PIN(D, 15), GPIO_OUT_LOW)

GPIO(EC_INT_L,             PIN(E, 0),  GPIO_OUT_LOW)
/* TODO: Is this in our output? */
GPIO(RTC_CHARGE_L,         PIN(E, 1),  GPIO_INPUT)
/* TODO: Should default to high? */
GPIO(DB_SWITCH_I2C_RST_L,  PIN(E, 2),  GPIO_OUT_HIGH)
GPIO(MGTAUX_EN,            PIN(E, 3),  GPIO_OUT_LOW)
GPIO(CLK_DIO_DB_PWR_EN,    PIN(E, 4),  GPIO_OUT_LOW)
GPIO(FAN0_TACH,            PIN(E, 5),  GPIO_INPUT)
GPIO(FAN0_PWM,             PIN(E, 6),  GPIO_OUTPUT)
GPIO(DAC_VCCAUX_EN,        PIN(E, 7),  GPIO_OUT_LOW)
GPIO(DACVTT_EN,            PIN(E, 8),  GPIO_OUT_LOW)
GPIO(DDR4S_VTT_EN,         PIN(E, 9),  GPIO_OUT_LOW)
GPIO(M3_BUS_0_ALRT,        PIN(E, 10), GPIO_OUTPUT)
/* TODO: Should this be an IRQ ?*/
GPIO(PS_STM_CS_L,          PIN(E, 11), GPIO_INPUT)
GPIO(PS_STM_SCLK,          PIN(E, 12), GPIO_INPUT)
GPIO(PS_STM_MISO,          PIN(E, 13), GPIO_OUTPUT)
GPIO(PS_STM_MOSI,          PIN(E, 14), GPIO_INPUT)
GPIO(PS_SRST_L,            PIN(E, 15), GPIO_OUTPUT)

/* TODO: Are these right? */
GPIO(PS_POR_L,             PIN(H, 0),  GPIO_OUT_LOW)
GPIO(MGTAUX_EN_MCU,        PIN(H, 1),  GPIO_OUT_LOW)

UNIMPLEMENTED(ENTERING_RW)
UNIMPLEMENTED(WP_L)

ALTERNATE(PIN_MASK(A, 0x0600), GPIO_ALT_USART,   MODULE_UART, 0)    /* USART1: PA9/PA10 */
ALTERNATE(PIN_MASK(B, 0x0c00), GPIO_ALT_I2C,     MODULE_I2C,  GPIO_OPEN_DRAIN) /* I2C2:   PB10/PB11 */
ALTERNATE(PIN_MASK(A, 0x0100), GPIO_ALT_I2C,     MODULE_I2C,  GPIO_OPEN_DRAIN) /* I2C3 PA8 */
ALTERNATE(PIN_MASK(C, 0x0200), GPIO_ALT_I2C,     MODULE_I2C,  GPIO_OPEN_DRAIN) /* I2C3 PC9 */
ALTERNATE(PIN_MASK(B, 0x0300), GPIO_ALT_I2C,     MODULE_I2C,  GPIO_OPEN_DRAIN) /* I2C1:   PB8/PB9 */
ALTERNATE(PIN_MASK(B, 0x0080), GPIO_ALT_TIM3_4,  MODULE_PWM,  0) /* PWM: TIM4, Channel 1*/
ALTERNATE(PIN_MASK(E, 0x7800), GPIO_ALT_SPI,     MODULE_SPI,  0) /* SPI4: PE10, PE11, PE12, PE13 */
ALTERNATE(PIN_MASK(E, 0x0040), GPIO_ALT_TIM9_11, MODULE_PWM,  0) /* PWM  TIM9, Channel 2*/
