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
#include "clock.h"

#include "console.h"
#include "hwtimer.h"
#include "task.h"
#include "registers.h"
#include "system.h"
#include "util.h"


#define IRQ_TIM(n) CONCAT2(STM32_IRQ_TIM, n)

struct fan_speed_t {
	int fan_mode;

	int rpm_target;

	enum fan_status sts;
	int enabled;

	int last_diff;

	uint32_t ccr_irq;
};

/* The prescaler is calculated as follows:
 * F_CNT = F_CLK / PSC is the counter freq
 *
 * We got a 16 bit counter so 0x10000 is max + 1,
 * therefore we need a prescaler of:
 *
 * PSC = F_CLK / F_MIN / 0x10000 where F_MIN is ~50Hz
 * Additionally we only trigger TI1 every 8th pulse
 * So we need to decimate by another 8
 *
 * In theory since the TACH signal gives us two pulses
 * per rotation a 4 would be sufficient here.
 */
#define F_CNT_PSC (15 * 8)

void fans_configure(void)
{
	gpio_config_module(MODULE_FAN, 1);

#if defined (TIM_CAPTURE_FAN0)
	/* Start with Fan0 */
	__hw_timer_enable_clock(TIM_CAPTURE_FAN0, 1);

	STM32_TIM_PSC(TIM_CAPTURE_FAN0) = F_CNT_PSC;

	STM32_TIM_CCMR1(TIM_CAPTURE_FAN0) = STM32_TIM_CCMR_CC1S_0
		|  STM32_TIM_CCMR_ICF1F_1 | STM32_TIM_CCMR_ICF1F_0
		| STM32_TIM_CCMR_IC1_PSC_0 | STM32_TIM_CCMR_IC1_PSC_1;
	STM32_TIM_CCER(TIM_CAPTURE_FAN0) = STM32_TIM_CCER_CC1E | STM32_TIM_CCER_CC1NP;
	STM32_TIM_CR1(TIM_CAPTURE_FAN0) = STM32_TIM_CR1_CEN;
	STM32_TIM_DIER(TIM_CAPTURE_FAN0) = STM32_TIM_DIER_CC1IE | STM32_TIM_DIER_CC1OF;

	task_enable_irq(IRQ_TIM(TIM_CAPTURE_FAN0));
#endif

#if defined (TIM_CAPTURE_FAN1)
	/* Then with Fan1 */
	__hw_timer_enable_clock(TIM_CAPTURE_FAN1, 1);

	STM32_TIM_PSC(TIM_CAPTURE_FAN1) = F_CNT_PSC;

	STM32_TIM_CCMR1(TIM_CAPTURE_FAN1) = STM32_TIM_CCMR_CC1S_0
		|  STM32_TIM_CCMR_ICF1F_1 | STM32_TIM_CCMR_ICF1F_0
		| STM32_TIM_CCMR_IC1_PSC_0 | STM32_TIM_CCMR_IC1_PSC_1;
	STM32_TIM_CCER(TIM_CAPTURE_FAN1) = STM32_TIM_CCER_CC1E | STM32_TIM_CCER_CC1NP;
	STM32_TIM_CR1(TIM_CAPTURE_FAN1) = STM32_TIM_CR1_CEN;
	STM32_TIM_DIER(TIM_CAPTURE_FAN1) = STM32_TIM_DIER_CC1IE | STM32_TIM_DIER_CC1OF;

	task_enable_irq(IRQ_TIM(TIM_CAPTURE_FAN1));
#endif
}

static struct fan_speed_t fan_speed_state[FAN_CH_COUNT];

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

	if (!percent)
		percent = 1;

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
	uint32_t freq, meas;

	freq = clock_get_freq();

	meas = fan_speed_state[ch].ccr_irq;
	if (!meas)
		return 0;

	/* The formula here would be:
	 * RPM = F_CNT * 60 * 8 / meas / 2,
	 * since we trigger only every 8th input and
	 * the fan gives two pulses per revolution
	 *
	 * F_CNT is given by MCU_FREQ / (PSC + 1)
	 */
	return freq / (F_CNT_PSC + 1) / meas * 30 * 8;
}

int fan_get_rpm_target(int ch)
{
	if (fan_get_enabled(ch))
		return fan_speed_state[ch].rpm_target;

	return 0;
}

test_mockable void fan_set_rpm_target(int ch, int rpm)
{
	const struct fan_t *fan = fans + ch;

	if (rpm < fan->rpm_min)
		rpm = fan->rpm_min;
	fan_speed_state[ch].rpm_target = rpm;
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

	return !fan_get_rpm_actual(ch);
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

	fans_configure();
}
/* need to initialize the PWM before turning on IRQs */
DECLARE_HOOK(HOOK_INIT, fan_init, HOOK_PRIO_INIT_FAN);

#define FAN_READJUST 100

void fan_ctrl(void)
{
	int32_t ch, duty, diff, actual, target, last_diff;
	for (ch = 0; ch < FAN_CH_COUNT; ch++)
	{
		if (!fan_get_enabled(ch) && !fan_get_duty(ch))
			continue;

		duty = fan_get_duty(ch);
		target = (int32_t) fan_get_rpm_target(ch);
		actual = (int32_t) fan_get_rpm_actual(ch);
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

#if defined(TIM_CAPTURE_FAN0)
void __fan0_capture_irq(void)
{
	static uint32_t counter0, counter1;
	static int saw_first_edge;

	uint32_t sr = STM32_TIM_SR(TIM_CAPTURE_FAN0);

	/* if no capture event, this was unexpected,
	 * return early
	 */
	if (!(sr & STM32_TIM_SR_CC1IF))
		return;

	/* got an overflow, clear flag, say we didn't see no edge */
	if (sr & STM32_TIM_SR_CC1OF) {
		saw_first_edge = 0;
		STM32_TIM_SR(TIM_CAPTURE_FAN0) &= ~(STM32_TIM_SR_CC1OF);
		return;
	}

	/* this is the first edge */
	if (!saw_first_edge) {
		counter0 = (uint32_t) STM32_TIM_CCR1(TIM_CAPTURE_FAN0);
		saw_first_edge = 1;
		return;
	}

	counter1 = (uint32_t) STM32_TIM_CCR1(TIM_CAPTURE_FAN0);
	if (counter1 > counter0)
		fan_speed_state[0].ccr_irq = counter1 - counter0;
	else
		fan_speed_state[0].ccr_irq = counter1 + 0xffff - counter0 + 1;

	counter0 = counter1;
}
DECLARE_IRQ(IRQ_TIM(TIM_CAPTURE_FAN0), __fan0_capture_irq, 2);
#endif /* TIM_CAPTURE_FAN0 */

#if defined(TIM_CAPTURE_FAN1)
void __fan1_capture_irq(void)
{
	static uint32_t counter0, counter1;
	static int saw_first_edge;

	uint32_t sr = STM32_TIM_SR(TIM_CAPTURE_FAN1);

	/* if no capture event, this was unexpected,
	 * return early
	 */
	if (!(sr & STM32_TIM_SR_CC1IF))
		return;

	/* got an overflow, clear flag, say we didn't see no edge */
	if (sr & STM32_TIM_SR_CC1OF) {
		saw_first_edge = 0;
		STM32_TIM_SR(TIM_CAPTURE_FAN1) &= ~(STM32_TIM_SR_CC1OF);
		return;
	}

	/* this is the first edge */
	if (!saw_first_edge) {
		counter0 = (uint32_t) STM32_TIM_CCR1(TIM_CAPTURE_FAN1);
		saw_first_edge = 1;
		return;
	}

	counter1 = (uint32_t) STM32_TIM_CCR1(TIM_CAPTURE_FAN1);
	if (counter1 > counter0)
		fan_speed_state[1].ccr_irq = counter1 - counter0;
	else
		fan_speed_state[1].ccr_irq = counter1 + 0xffff - counter0 + 1;

	counter0 = counter1;
}
DECLARE_IRQ(IRQ_TIM(TIM_CAPTURE_FAN1), __fan1_capture_irq, 2);
#endif /* TIM_CAPTURE_FAN1 */
