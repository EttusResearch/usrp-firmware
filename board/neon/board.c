/* Copyright (c) 2016,2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.  */
/* National Instruments Neon board configuration */

#include "common.h"
#include "ec_version.h"
#include "gpio.h"
#include "hooks.h"
#include "fan_board.h"
#include "registers.h"
#include "spi.h"
#include "i2c.h"
#include "task.h"
#include "timer.h"
#include "usart-stm32f0.h"
#include "pwm.h"
#include "pwm_chip.h"
#include "util.h"
#include "console.h"
#include "queue.h"
#include "power_button.h"
#include "power.h"
#include "power_led.h"

#include "fan.h"
#include "eeprom.h"

#include "usart_tx_dma.h"
#include "usart_rx_dma.h"
#include "util.h"

#include "usb-stream.h"

#include "temp_sensor.h"
#include "driver/temp_sensor/tmp468.h"

void wdt_reset_event(enum gpio_signal signal);

#include "gpio_list.h"

#define CPUTS(outstr) cputs(CC_CHIPSET, outstr)

/* power signal list.  Must match order of enum power_signal. */
const struct power_signal_info power_signal_list[] = {
	{GPIO_SYS_PS_PWRON, 1, "AP   PGOOD ASSERTED"},
	{GPIO_PWR_1V0_PG,   1, "1V   PGOOD ASSERTED"},
	{GPIO_PWR_1V3_PG,   1, "1.3V   PGOOD ASSERTED"},
	{GPIO_PWR_1V5_PG,   1, "1.5V PGOOD ASSERTED"},
	{GPIO_PWR_1V8_PG,   1, "1.8V PGOOD ASSERTED"},
	{GPIO_PWR_3V3_PG,   1, "3.3V PGOOD ASSERTED"},
	{GPIO_PWR_3V8_PG,   1, "3.8V PGOOD ASSERTED"},
	{GPIO_PWR_MGTVTT_PG,   1, "MGTVTT PGOOD ASSERTED"},
	{GPIO_PWR_MGTVCC_PG,   1, "MGTVCC PGOOD ASSERTED"},
};
BUILD_ASSERT(ARRAY_SIZE(power_signal_list) == POWER_SIGNAL_COUNT);

static void board_init(void)
{
	/*gpio_enable_interrupt(GPIO_SYS_RTC_INT);*/
}
DECLARE_HOOK(HOOK_INIT, board_init, HOOK_PRIO_DEFAULT);

static int led_state;
static void heartbeat_led(void)
{
	if (!(led_state ^= led_state)) {
		gpio_set_level(GPIO_SYS_LED, 1);
		msleep(70);
		gpio_set_level(GPIO_SYS_LED, 0);
		msleep(50);
		gpio_set_level(GPIO_SYS_LED, 1);
		msleep(100);
		gpio_set_level(GPIO_SYS_LED, 0);
	}
}
DECLARE_HOOK(HOOK_SECOND, heartbeat_led, HOOK_PRIO_DEFAULT);


int extpower_is_present(void)
{
	return 0;
}

int lid_is_open(void)
{
	return 1;
}

void board_config_pre_init(void)
{
	/* enable SYSCFG clock */
	STM32_RCC_APB2ENR |= STM32_RCC_SYSCFGEN;
	STM32_SYSCFG_CFGR1 |= (1 << 9) | (1 << 10);
}

#if 0
int board_get_version(void)
{
	/* counting starts at 0 ... */
	return eeprom_get_board_rev() + 1;
}
#endif

/* I2C ports */
const struct i2c_port_t i2c_ports[] = {
	{"master", I2C_PORT_MASTER, 100, GPIO_MASTER_I2C_SCL, GPIO_MASTER_I2C_SDA},
#ifdef CONFIG_HOSTCMD_I2C_SLAVE_ADDR
	{"slave", I2C_PORT_SLAVE, 1000, GPIO_SLAVE_I2C_SCL, GPIO_SLAVE_I2C_SDA},
#endif
};
const unsigned int i2c_ports_used = ARRAY_SIZE(i2c_ports);

#ifdef CONFIG_PWM
const struct pwm_t pwm_channels[] = {
	{STM32_TIM(15), STM32_TIM_CH(1), 0, 25000},              // 15
	{STM32_TIM(3),  STM32_TIM_CH(1), 0, 25000},              // 14
};
BUILD_ASSERT(ARRAY_SIZE(pwm_channels) == PWM_CH_COUNT);
#endif

#ifdef CONFIG_USB_CONSOLE
const void *const usb_strings[] = {
	[USB_STR_DESC]         = usb_string_desc,
	[USB_STR_VENDOR]       = USB_STRING_DESC("National Instruments Inc."),
	[USB_STR_PRODUCT]      = USB_STRING_DESC("Project Neon"),
	[USB_STR_VERSION]      = USB_STRING_DESC(CROS_EC_VERSION32),
	[USB_STR_CONSOLE_NAME] = USB_STRING_DESC("Shell"),
};
BUILD_ASSERT(ARRAY_SIZE(usb_strings) == USB_STR_COUNT);
#endif /* CONFIG_USB_CONSOLE */

#ifdef CONFIG_LED_POLICY_STD
#include "charge_state.h"

enum charge_state charge_get_state(void) { return 0; }

uint32_t charge_get_flags(void)  { return 0; }

int charge_get_percent(void) { return 100; }

#endif

#ifdef CONFIG_TEMP_SENSOR
const struct temp_sensor_t temp_sensors[] = {
	{"TMP464_Internal", TEMP_SENSOR_TYPE_BOARD, tmp468_get_val,
		TMP468_LOCAL},
	{"TMP464_Remote_1", TEMP_SENSOR_TYPE_BOARD, tmp468_get_val,
		TMP468_REMOTE1},
	{"TMP464_Remote_2", TEMP_SENSOR_TYPE_CPU, tmp468_get_val,
		TMP468_REMOTE2},
};
BUILD_ASSERT(ARRAY_SIZE(temp_sensors) == TEMP_SENSOR_COUNT);
#endif /* CONFIG_TEMP_SENSOR */

struct ec_thermal_config thermal_params[] = {
	/* {Twarn, Thigh, Thalt}, fan_off, fan_max */
	{{C_TO_K(50), C_TO_K(65), C_TO_K(75)}, {0,0,0}, C_TO_K(30), C_TO_K(60)},	/* Ambient */
	{{C_TO_K(80), C_TO_K(85), C_TO_K(95)}, {0,0,0}, C_TO_K(50), C_TO_K(80)},	/* AP Diode */
	{{C_TO_K(80), C_TO_K(85), C_TO_K(95)}, {0,0,0}, C_TO_K(50), C_TO_K(80)},	/* FIXME */
};
BUILD_ASSERT(ARRAY_SIZE(thermal_params) == TEMP_SENSOR_COUNT);
