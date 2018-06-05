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
#include "endian.h"
#include "hwtimer.h"
#include "task.h"
#include "registers.h"
#include "system.h"
#include "util.h"


#define IRQ_TIM(n) CONCAT2(STM32_IRQ_TIM, n)
#define EEPROM_FAN_PRESENT_FLAG (1<<1)
#define INTERNAL_FAN_PRESENT_FLAG (1<<2)

#define htonl htobe32
#define ntohl htonl

struct fan_speed_t {
	int fan_mode;

	int rpm_target;

	enum fan_status sts;
	int enabled;

	int last_diff;

	uint32_t ccr_irq;
	int last_seen;
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

/* The fan PWM is inverted, so we need to convert from the desired duty cycle (ie 100% is fully on)
 * to the inverted duty cycle (100% is the minimum fan speed).
 */
int fan_pwm_convert_duty(const int desired_duty) {
	int ret = 100 - desired_duty;
	if (ret < 0)
		return 0;
	return ret;
}

/* Set the 'fan present' flag in the fan_t struct */
void fan_set_present(int ch, int present)
{
	struct fan_t *fan = fans + ch;

	if (present) {
		fan->flags |= INTERNAL_FAN_PRESENT_FLAG;
	} else {
		fan->flags &= (0xFFFFFFFF ^ INTERNAL_FAN_PRESENT_FLAG);
	}
}

/* Check to see if the fan is present */
int fan_get_present(int ch)
{
	const struct fan_t *fan = fans + ch;

	return fan->flags & INTERNAL_FAN_PRESENT_FLAG;
}

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

	/* Check and update the status
	   If we're already in the correct state, return early */
	if ((enabled && fan_get_status(ch) == FAN_STATUS_LOCKED) ||
		(!enabled && fan_get_status(ch) == FAN_STATUS_STOPPED)) {
		gpio_set_level(GPIO_FAN_EN, enabled);
		return;
	}

	/* If we're trying to enable a fan that is not present, don't try to enable it anymore */
	if (enabled && !fan_get_present(ch)) {
		enabled = 0;
	}

	fan_speed_state[ch].sts = enabled ? FAN_STATUS_CHANGING : FAN_STATUS_STOPPED;
	pwm_enable(fan->ch, enabled);
	fan_speed_state[ch].enabled = enabled;
	gpio_set_level(GPIO_FAN_EN, enabled);
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

	/* The duty cycle is inverted, so handle that here */
	percent = fan_pwm_convert_duty(percent);

	if (!percent)
		percent = 1;

	pwm_set_duty(fan->ch, percent);
}

int fan_get_duty(int ch)
{

	const struct fan_t *fan = fans + ch;

	/* The duty cycle is inverted, so handle that here */
	return fan_pwm_convert_duty(pwm_get_duty(fan->ch));
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
	if (!fan_get_present(ch) || !fan_get_enabled(ch) ||
	    fan_get_rpm_target(ch) == 0 || !fan_get_duty(ch))
		return 0;

	return !fan_get_rpm_actual(ch);
}

void fan_channel_setup(int ch, unsigned int flags)
{
	struct fan_t *fan = fans + ch;

	fan->rpm_min = eeprom_get_fan_min(ch);
	fan->rpm_max = eeprom_get_fan_max(ch);

	pwm_enable(ch, 1);
	/* Start with the fans at the minimum speed */
	fan_set_duty(ch, 0);

	fan_speed_state[ch].sts = FAN_STATUS_STOPPED;
	fan_speed_state[ch].last_diff = 0;
}

void fan_init(void)
{
	uint32_t* val;

	/* Fetch whether or not the fan is enabled, and run the setup if necessary */
	if ((!eeprom_get_mcu_flags(&val)) && (ntohl(*val) & EEPROM_FAN_PRESENT_FLAG)) {
		fan_set_present(FAN_CH_0, 1);
		fan_channel_setup(FAN_CH_0, 0);
		msleep(50);
	} else {
		fan_set_present(FAN_CH_0, 0);
		fan_set_enabled(FAN_CH_0, 0);
		gpio_set_level(GPIO_FAN_EN, 0);
	}

	fans_configure();
}
/* need to initialize the PWM before turning on IRQs */
DECLARE_HOOK(HOOK_INIT, fan_init, HOOK_PRIO_INIT_FAN);

#define FAN_READJUST 100

void fan_ctrl(void)
{
	int32_t duty, diff, actual, target, last_diff;

	if (!fan_get_enabled(FAN_CH_0)) {
		return;
	}

	duty = fan_get_duty(FAN_CH_0);
	target = (int32_t) fan_get_rpm_target(FAN_CH_0);
	actual = (int32_t) fan_get_rpm_actual(FAN_CH_0);
	diff = target - actual;

	last_diff = fan_speed_state[FAN_CH_0].last_diff;

	/* once we're locked, don't wiggle around too much */
	if (fan_speed_state[FAN_CH_0].sts == FAN_STATUS_LOCKED)
		diff = (99 * last_diff + diff * 1) / 100;

	fan_speed_state[FAN_CH_0].last_diff = diff;

	/* too slow, speed up ... */
	if (diff > FAN_READJUST) {
		if (duty == 100) {
			fan_speed_state[FAN_CH_0].sts = FAN_STATUS_FRUSTRATED;
			return;
		} else if(diff > 1000) {
			duty += 10;
		} else if (diff > 500) {
			duty += 5;
		} else if (diff > 100)
			duty += 1;

		duty = duty > 100 ? 100 : duty;

		fan_speed_state[FAN_CH_0].sts = FAN_STATUS_CHANGING;
		fan_set_duty(FAN_CH_0, duty);
	/* too fast, slow down ... */
	} else if (diff < -FAN_READJUST) {
		if (!duty) {
			fan_speed_state[FAN_CH_0].sts = FAN_STATUS_FRUSTRATED;
			return;
		} else if (diff < -1000) {
			duty-=10;
		} else if (diff < -500) {
			duty-=5;
		} else if (diff < -100) {
			duty-=5;
		}
		duty = duty < 0 ? 0 : duty;

		fan_speed_state[FAN_CH_0].sts = FAN_STATUS_CHANGING;
		fan_set_duty(FAN_CH_0, duty);
	} else {
		fan_speed_state[FAN_CH_0].sts = FAN_STATUS_LOCKED;
	}
}
DECLARE_HOOK(HOOK_SECOND, fan_ctrl, HOOK_PRIO_DEFAULT);

#if defined(TIM_CAPTURE_FAN0)
void __fan0_capture_irq(void)
{
	static uint32_t counter0, counter1;
	static int saw_first_edge;

	uint32_t sr = STM32_TIM_SR(TIM_CAPTURE_FAN0);

	/* Set this flag to say that we've seen the interrupt within the last second */
	fan_speed_state[0].last_seen = 1;

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

/* Check that we've gotten a fan capture interrupt within the last second */
void _fan_irq_within_last_sec(void)
{
	if (fan_speed_state[0].last_seen) {
		fan_speed_state[0].last_seen = 0;
	} else {
		/* if we haven't seen a fan capture interrupt, reset the variable we use
		   to calculate the actual RPM */
		fan_speed_state[0].ccr_irq = 0;
	}
}
DECLARE_HOOK(HOOK_SECOND, _fan_irq_within_last_sec, HOOK_PRIO_DEFAULT);
#endif /* TIM_CAPTURE_FAN0 */
