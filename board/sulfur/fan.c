/* Copyright (c) 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "fan.h"
#include "hooks.h"
#include "pwm.h"
#include "common.h"
#include "gpio.h"
#include "timer.h"
#include "console.h"
#include "eeprom.h"

struct fan_speed_t {
	unsigned int flags;

	uint64_t last_irq;
	int fan_mode;

	int rpm_actual;
	int rpm_target;

	enum fan_status sts;
	int enabled;

	int last_diff;
};

static struct fan_speed_t fan_speed_state[2];

int fan_percent_to_rpm(int fan, int pct)
{
	int rpm, max, min;

	if (!pct) {
		rpm = 0;
	} else {
		min = fans[fan].rpm_min;
		max = fans[fan].rpm_max;
		rpm = ((pct - 1) * max + (100 - pct) * min) / 99;
	}

	return rpm;
}

void fan_set_enabled(int ch, int enabled)
{
	const struct fan_t *fan = fans + ch;

	if (enabled) {
		fan_speed_state[ch].sts = FAN_STATUS_CHANGING;
		pwm_enable(fan->ch, enabled);
	} else {
		pwm_set_duty(fan->ch, 0);
	}

	fan_speed_state[ch].enabled = enabled;
}

int fan_get_enabled(int ch)
{
	const struct fan_t *fan = fans + ch;

	return pwm_get_enabled(fan->ch)
		&& fan_speed_state[ch].enabled;
}

void fan_set_duty(int ch, int percent)
{
	const struct fan_t *fan = fans + ch;

	pwm_set_duty(fan->ch, percent);
}

int fan_get_duty(int ch)
{

	const struct fan_t *fan = fans + ch;

	return pwm_get_duty(fan->ch);
}

int fan_get_rpm_mode(int ch)
{
	const struct fan_t *fan = fans + ch;

	return fan_speed_state[fan->ch].fan_mode;
}

void fan_set_rpm_mode(int ch, int rpm_mode)
{
	const struct fan_t *fan = fans + ch;

	fan_speed_state[fan->ch].fan_mode = rpm_mode;
}

int fan_get_rpm_actual(int ch)
{
	const struct fan_t *fan = fans + ch;

	return fan_speed_state[fan->ch].rpm_actual;
}

void fan_check_stall(void)
{
	int i;
	uint64_t time_now = get_time().val;
	uint64_t diff;

	for (i = 0; i < FAN_CH_COUNT; i++) {
		diff = time_now - fan_speed_state[i].last_irq;

		if (diff > 500 * MSEC)
			fan_speed_state[i].rpm_actual = 0;
	}
}
DECLARE_HOOK(HOOK_SECOND, fan_check_stall, HOOK_PRIO_DEFAULT);

int fan_get_rpm_target(int ch)
{
	const struct fan_t *fan = fans + ch;

	if (fan_get_enabled(ch))
		return fan_speed_state[fan->ch].rpm_target;

	return 0;
}

test_mockable void fan_set_rpm_target(int ch, int rpm)
{
	const struct fan_t *fan = fans + ch;

	if (rpm < fan->rpm_min)
		rpm = fan->rpm_min;
	fan_speed_state[fan->ch].rpm_target = rpm;
}

enum fan_status fan_get_status(int ch)
{
	return fan_speed_state[ch].sts;
}

int fan_is_stalled(int ch)
{
	if (!fan_get_enabled(ch) || fan_get_rpm_target(ch) == 0 ||
	    !fan_get_duty(ch))
		return 0;

	return fan_speed_state[ch].sts == FAN_STATUS_STOPPED;
}

void fan_channel_setup(int ch, unsigned int flags)
{
	struct fan_t *fan = fans + ch;

	fan->rpm_min = eeprom_get_fan_min(ch);
	fan->rpm_max = eeprom_get_fan_max(ch);

	pwm_enable(ch, 1);
	pwm_set_duty(ch, 0);

	fan_speed_state[ch].sts = FAN_STATUS_STOPPED;
	fan_speed_state[ch].last_diff = 0;
}

void fan_init(void)
{
	int i;

	for (i = 0; i < FAN_CH_COUNT; i++)
		fan_channel_setup(i, 0);

	msleep(50);

	gpio_enable_interrupt(GPIO_SYS_FAN0_TACH);
	gpio_enable_interrupt(GPIO_SYS_FAN1_TACH);
}
/* need to initialize the PWM before turning on IRQs */
DECLARE_HOOK(HOOK_INIT, fan_init, HOOK_PRIO_INIT_PWM+1);

#define FAN_READJUST 100

void fan_ctrl(void)
{
	int ch, duty, diff, actual, target, last_diff;
	for (ch = 0; ch < FAN_CH_COUNT; ch++)
	{
		if (!fan_get_enabled(ch))
			continue;

		duty = fan_get_duty(ch);
		target = fan_get_rpm_target(ch);
		actual = fan_get_rpm_actual(ch);
		diff = target - actual;

		last_diff = fan_speed_state[ch].last_diff;

		/* once we're locked, don't wiggle around too much */
		if (fan_speed_state[ch].sts == FAN_STATUS_LOCKED)
			diff = (99 * last_diff + diff * 1) / 100;

		fan_speed_state[ch].last_diff = diff;

		/* too slow, speed up ... */
		if (diff > FAN_READJUST) {
			if (duty == 100) {
				fan_speed_state[ch].sts = FAN_STATUS_FRUSTRATED;
				continue;
			} else if(diff > 1000) {
				duty += 10;
			} else if (diff > 500) {
				duty += 5;
			} else if (diff > 100)
				duty += 1;

			duty = duty > 100 ? 100 : duty;

			fan_speed_state[ch].sts = FAN_STATUS_CHANGING;
			fan_set_duty(ch, duty);
		/* too fast, slow down ... */
		} else if (diff < -FAN_READJUST) {
			if (!duty) {
				fan_speed_state[ch].sts = FAN_STATUS_FRUSTRATED;
				continue;
			} else if (diff < -1000) {
				duty-=10;
			} else if (diff < -500) {
				duty-=5;
			} else if (diff < -100) {
				duty-=5;
			}
			duty = duty < 0 ? 0 : duty;

			fan_speed_state[ch].sts = FAN_STATUS_CHANGING;
			fan_set_duty(ch, duty);
		} else {
			fan_speed_state[ch].sts = FAN_STATUS_LOCKED;
		}
       }
}
DECLARE_HOOK(HOOK_SECOND, fan_ctrl, HOOK_PRIO_DEFAULT);

void fan_tach_interrupt(enum gpio_signal sig)
{
	uint64_t diff;
	uint64_t time_now = get_time().val;
	int fan = sig - GPIO_SYS_FAN0_TACH;

	diff = time_now - fan_speed_state[fan].last_irq;
	fan_speed_state[fan].last_irq = time_now;

	/* be careful here, to make sure diff is legit */
	fan_speed_state[fan].rpm_actual = diff ? (300000 / (diff / 100)) : 0;
}
