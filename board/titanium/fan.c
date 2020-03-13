/* Copyright (c) 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "hooks.h"
#include "pwm.h"
#include "common.h"
#include "gpio.h"
#include "timer.h"
#include "clock.h"
#include "clock-f.h"
#include "console.h"
#include "hwtimer.h"
#include "pwrsup.h"
#include "task.h"
#include "registers.h"
#include "system.h"
#include "util.h"
#include "usrp_eeprom.h"
#include "eeproms.h"
#include "fan.h"


#define IRQ_TIM(n) CONCAT2(STM32_IRQ_TIM, n)

struct fan_speed_t {
	int fan_mode;

	int rpm_target;

	enum fan_status sts;
	int enabled;

	int last_diff;

	uint32_t ccr_irq;

	uint16_t counter_prev;
	uint16_t counter_new;
	int saw_first_edge;
};

void fans_configure(void)
{
#if defined (TIM_CAPTURE_FAN0_1)
	__hw_timer_enable_clock(TIM_CAPTURE_FAN0_1, 1);

	/* Disable timer during setup */
	STM32_TIM_CR1(TIM_CAPTURE_FAN0_1) = 0x0000;

	/*
	 * Let's examine the speed measurement using timer's input capture
	 * function. This process will lead us to chosing the appropriate values
	 * for the PSC (Prescalar) and ARR (Auto Reload Register).
	 *
	 * Time required for one rotation = Time elapsed between two
	 * tachometer pulses emitted by the fan assembly.
	 *
	 * We measure this duration by taking note of the timer value
	 * whenever we see a rising tach pulse from the fan. The timer/counter
	 * value difference between any such consecutive events is the basis for
	 * speed calculation.
	 *
	 * So let's say the timer/counter difference is dt between two pulses.
	 * The time required for one fan rotation
	 * 				= dt / ( count frequency)
	 * 				= dt / (timer clock frequency/ PSC)
	 * 				= (dt * PSC) / (timer clock frequency)
	 *
	 * So the fan speed in rotations per sec =
	 * 				(timer clock frequency) / (dt * PSC)
	 *
	 * Fan speed in RPM(rotation per minute) = 60 * rotation per sec
	 * RPM = (60 * timer clock frequency) / (dt * PSC)
	 *
	 * Also considering that the fan actually outputs two pulses every
	 * rotation and we have a timer-channel prescalar which essentially
	 * interrupts every (1/prescalar) times we get
	 *
	 * RPM = (60 * timer clock frequency * 8) / (dt * PSC * 2)
	 *
	 * Giving us
	 *
	 * dt = (60 * timer clock frequency * 8) / ( RPM * PSC * 2)
	 *
	 * Now knowing that a timer counts from 0 to ARR value and we would like
	 * the duration between two consecutive timer interrupts to be less than
	 * ARR. If it exceeds ARR we won't be able to really measure the
	 * RPM because time would have slipped (unless we monitor timer update
	 * events)
	 *
	 * At all speeds being measured:
	 * dt <  ARR
	 * So  (60 * timer clock frequency * 8) / ( RPM * PSC * 2) < ARR
	 *
	 * The maximum value of dt occurs when the RPM is at minimum.
	 * In other words time difference (count difference) between pulses is
	 * largest when the fan speed is the lowest. This value is 3800 rpm as
	 * per fan datasheet. (https://www.sanyodenki.com/archive/document/product
	 * /cooling/catalog_pdf/San_Ace_40GA20_E.pdf)
	 *
	 * So (60 * 96M * 8) / (3800 * PSC * 2) < ARR
	 *
	 * Say we let ARR (16 bit register) be the the maximum permissible value
	 * i.e. 0xFFFF
	 *
	 * That gives approximately PSC > 93
	 *
	 * Let's choose a value which divides the timer frequency (96M) exactly
	 * to ease speed calculation further on..
	 *
	 * Say PSC = 120
	 *
	 * Note that, plugging into the dt formula above:
	 *
	 * At max possible RPM (12400); dt = 15483 and
	 * at max possible RPM (3800);  dt = 50526 approximately.
	 *
	 * which is well within the ARR value of 65535 chosen earlier.
	 *
	 * Note that for simplicity all equations above actually need to have
	 * a (PSC + 1) term instead of PSC but PSC is chosen for simplicity.
	 */

	/* Configuration common to TIMx_CH1 and TIMx_CH2 */
	STM32_TIM_PSC(TIM_CAPTURE_FAN0_1) = 119;
	STM32_TIM_ARR(TIM_CAPTURE_FAN0_1) = 0xFFFF;

	/* TIMx_CH1 Configuration */
	STM32_TIM_CCMR1(TIM_CAPTURE_FAN0_1) |= STM32_TIM_CCMR_CC1S_0
		|  STM32_TIM_CCMR_ICF1F_1 | STM32_TIM_CCMR_ICF1F_0
		| STM32_TIM_CCMR_IC1_PSC_0 | STM32_TIM_CCMR_IC1_PSC_1;
	STM32_TIM_CCER(TIM_CAPTURE_FAN0_1) |= STM32_TIM_CCER_CC1E;
	STM32_TIM_DIER(TIM_CAPTURE_FAN0_1) |= STM32_TIM_DIER_CC1IE;

	/* TIMx_CH2 Configuration */
	STM32_TIM_CCMR1(TIM_CAPTURE_FAN0_1) |= STM32_TIM_CCMR_CC2S_0
		|  STM32_TIM_CCMR_ICF2F_1 | STM32_TIM_CCMR_ICF2F_0
		| STM32_TIM_CCMR_IC2_PSC_0 | STM32_TIM_CCMR_IC2_PSC_1;
	STM32_TIM_CCER(TIM_CAPTURE_FAN0_1) |= STM32_TIM_CCER_CC2E;
	STM32_TIM_DIER(TIM_CAPTURE_FAN0_1) |= STM32_TIM_DIER_CC2IE;

	/* Configuration common to TIMx_CH1 and TIMx_CH2 */
	STM32_TIM_EGR(TIM_CAPTURE_FAN0_1) = STM32_TIM_EGR(TIM_CAPTURE_FAN0_1) | STM32_TIM_EGR_UG;
	STM32_TIM_CR1(TIM_CAPTURE_FAN0_1) = STM32_TIM_CR1_ARPE | STM32_TIM_CR1_CEN;

	task_enable_irq(IRQ_TIM(TIM_CAPTURE_FAN0_1));
#endif
}

static struct fan_speed_t fan_speed_state[FAN_CH_COUNT];

int fan_percent_to_rpm(int fan, int pct)
{
	int rpm, max, min;

	if (!pct) {
		rpm = 0;
	} else {
		min = fans[fan].rpm->rpm_min;
		max = fans[fan].rpm->rpm_max;
		rpm = ((pct - 1) * max + (100 - pct) * min) / 99;
	}

	return rpm;
}

void fan_set_enabled(int ch, int enabled)
{
	const struct fan_t *fan = fans + ch;

	if (enabled) {
		fan_speed_state[ch].sts = FAN_STATUS_CHANGING;
		pwm_enable(fan->conf->ch, enabled);
	} else {
		pwm_set_duty(fan->conf->ch, 0);
	}

	fan_speed_state[ch].enabled = enabled;
}

int fan_get_enabled(int ch)
{
	const struct fan_t *fan = fans + ch;

	return pwm_get_enabled(fan->conf->ch)
		&& fan_speed_state[ch].enabled;
}

int fan_power_is_good(void)
{
	return pwrsup_get_status(POWER_SUPPLY_12V) == PWRSUP_STATUS_ON;
}

void fan_set_duty(int ch, int percent)
{
	const struct fan_t *fan = fans + ch;

	if (!percent)
		percent = 1;

	pwm_set_duty(fan->conf->ch, percent);
}

int fan_get_duty(int ch)
{

	const struct fan_t *fan = fans + ch;

	return pwm_get_duty(fan->conf->ch);
}

int fan_get_rpm_mode(int ch)
{
	const struct fan_t *fan = fans + ch;

	return fan_speed_state[fan->conf->ch].fan_mode;
}

void fan_set_rpm_mode(int ch, int rpm_mode)
{
	const struct fan_t *fan = fans + ch;

	fan_speed_state[fan->conf->ch].fan_mode = rpm_mode;
}

int fan_get_rpm_actual(int ch)
{
	uint32_t freq, meas;
	uint16_t psc;

	if (!fan_get_enabled(ch))
		return 0;

	freq = clock_get_timer_freq();

	meas = fan_speed_state[ch].ccr_irq;
	psc = STM32_TIM_PSC(TIM_CAPTURE_FAN0_1);
	if (!meas)
		return 0;

	return freq / (psc + 1) / meas * 30 * 8/* channel prescalar */;
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

	if (rpm < fan->rpm->rpm_min)
		rpm = fan->rpm->rpm_min;
	else if (rpm > fan->rpm->rpm_max)
		rpm = fan->rpm->rpm_max;

	fan_speed_state[ch].rpm_target = rpm;
}

enum fan_status fan_get_status(int ch)
{
	return fan_speed_state[ch].sts;
}

int fan_is_stalled(int ch)
{
	if (!fan_get_enabled(ch) || fan_get_rpm_target(ch) == 0 ||
	    !fan_get_duty(ch) || !fan_power_is_good())
		return 0;

	return !fan_get_rpm_actual(ch);
}

void fan_init_limits_from_eeprom(void)
{
	const struct usrp_eeprom_fan_limits *fan_limits;
	fan_limits = eeprom_lookup_tag(TLV_EEPROM_MB, USRP_EEPROM_FAN_LIMITS);
	if (!fan_limits)
		return;

	for (int fan = 0; fan < FAN_CH_COUNT; fan++) {
		fans[fan].rpm->rpm_min = fan_limits->min;
		fans[fan].rpm->rpm_start = fan_limits->start;
		fans[fan].rpm->rpm_max = fan_limits->max;
	}
}

void fan_channel_setup(int ch, unsigned int flags)
{
	pwm_enable(ch, 1);
	pwm_set_duty(ch, 0);

	fan_speed_state[ch].sts = FAN_STATUS_STOPPED;
	fan_speed_state[ch].last_diff = 0;
}

void fan_init(void)
{
	int i;

	fan_init_limits_from_eeprom();

	for (i = 0; i < FAN_CH_COUNT; i++)
		fan_channel_setup(i, 0);

	msleep(50);

	fans_configure();
}
/* Need to initialize the PWM before turning on IRQs */
DECLARE_HOOK(HOOK_INIT, fan_init, HOOK_PRIO_INIT_FAN);

#define FAN_READJUST 150

void fan_ctrl(void)
{
	int32_t ch, duty, diff, actual, target, last_diff;

	if (!fan_power_is_good())
		return;

	for (ch = 0; ch < FAN_CH_COUNT; ch++)
	{
		if (!fan_get_enabled(ch) && !fan_get_duty(ch))
			continue;

		/* If fan is in manual mode don't control. */
		if (fan_get_rpm_mode(ch) == 0)
			continue;

		duty = fan_get_duty(ch);
		target = (int32_t) fan_get_rpm_target(ch);
		actual = (int32_t) fan_get_rpm_actual(ch);

		diff = target - actual;

		last_diff = fan_speed_state[ch].last_diff;

		/* Once we're locked, don't wiggle around too much */
		if (fan_speed_state[ch].sts == FAN_STATUS_LOCKED)
			diff = (99 * last_diff + diff * 1) / 100;

		fan_speed_state[ch].last_diff = diff;

		/* Too slow, speed up ... */
		if (diff > FAN_READJUST) {
			if (duty == 100) {
				fan_speed_state[ch].sts = FAN_STATUS_FRUSTRATED;
				continue;
			} else if(diff > 1000) {
				duty += 10;
			} else if (diff > 500) {
				duty += 5;
			} else if (diff > 100) {
				duty += 1;
			}

			duty = duty > 100 ? 100 : duty;

			fan_speed_state[ch].sts = FAN_STATUS_CHANGING;
			fan_set_duty(ch, duty);
		/* Too fast, slow down ... */
		} else if (diff < -FAN_READJUST) {
			if (!duty) {
				fan_speed_state[ch].sts = FAN_STATUS_FRUSTRATED;
				continue;
			} else if (diff < -1000) {
				duty -= 10;
			} else if (diff < -500) {
				duty -= 5;
			} else if (diff < -100) {
				duty -= 1;
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

#if defined(TIM_CAPTURE_FAN0_1)
void __fan_capture_irq(int fan)
{
	uint16_t sr, sr_ccif, sr_ccof, ccr, arr;

	sr = STM32_TIM_SR(TIM_CAPTURE_FAN0_1);
	sr_ccif = fan == 0 ? STM32_TIM_SR_CC1IF : STM32_TIM_SR_CC2IF;
	sr_ccof = fan == 0 ? STM32_TIM_SR_CC1OF : STM32_TIM_SR_CC2OF;
	ccr = fan == 0 ? STM32_TIM_CCR1(TIM_CAPTURE_FAN0_1) : STM32_TIM_CCR2(TIM_CAPTURE_FAN0_1);
	arr = STM32_TIM_ARR(TIM_CAPTURE_FAN0_1);

	/* If no capture event, this was unexpected, return early */
	if (!(sr & sr_ccif))
		return;

	/* Got an overflow, clear flag, say we didn't see no edge */
	if (sr & sr_ccof) {
		fan_speed_state[fan].saw_first_edge = 0;
		STM32_TIM_SR(TIM_CAPTURE_FAN0_1) &= ~(sr_ccof);
		return;
	}

	/* This is the first edge */
	if (!fan_speed_state[fan].saw_first_edge) {
		fan_speed_state[fan].counter_prev = ccr;
		fan_speed_state[fan].saw_first_edge = 1;
		return;
	}

	fan_speed_state[fan].counter_new = ccr;
	/*
	 * The actual number of counter increments in between two input capture
	 * events equals the difference between the counter values at the two
	 * capture events.
	 */
	if (fan_speed_state[fan].counter_new > fan_speed_state[fan].counter_prev)
		fan_speed_state[fan].ccr_irq = fan_speed_state[fan].counter_new - fan_speed_state[fan].counter_prev;
	else
		fan_speed_state[fan].ccr_irq =
			arr + fan_speed_state[fan].counter_new - fan_speed_state[fan].counter_prev + 1;

	fan_speed_state[fan].counter_prev = fan_speed_state[fan].counter_new;
}

void __fans_capture_irq(void)
{
	__fan_capture_irq(FAN_CH_0);
	__fan_capture_irq(FAN_CH_1);
}
DECLARE_IRQ(IRQ_TIM(TIM_CAPTURE_FAN0_1), __fans_capture_irq, 2);
#endif /* TIM_CAPTURE_FAN0_1 */

/*
 * This monitor checks if the the total number of fan rotations has incremented
 * and mark them as STOPPED if they haven't. Also the ccr_irq
 * (interrupt count) is set to zero which sets the output of fan_is_stalled()
 * true. This is done for all fans.
 */
void fan_health_monitor(void)
{
	static uint16_t fan_counter[FAN_CH_COUNT];
	for (uint8_t fan = 0; fan < FAN_CH_COUNT; fan++) {
		if (!fan_get_enabled(fan) ||
			!fan_get_duty(fan) ||
			!fan_power_is_good() ||
			(fan_get_rpm_mode(fan) &&
				fan_get_rpm_target(fan) == 0)) {
			continue;
		} else if (fan_counter[fan] == fan_speed_state[fan].counter_new) {
			/* # cycles didn't increment; fan is not spinning */
			fan_speed_state[fan].ccr_irq = 0;
			fan_speed_state[fan].sts = FAN_STATUS_STOPPED;
		} else {
			fan_speed_state[fan].sts = FAN_STATUS_CHANGING;
		}
	}

	/* Store current values for next comparison */
	for (uint8_t fan = 0; fan < FAN_CH_COUNT; fan++)
		fan_counter[fan] = fan_speed_state[fan].counter_new;
}
DECLARE_HOOK(HOOK_SECOND, fan_health_monitor, HOOK_PRIO_DEFAULT);

/*
 * Fan Test:
 * Iterate over multiple fan speeds from min to max and check that each
 * fan can lock to each tested speed. Test fails if the fan fails to lock to
 * speed under test.
 * Test assumes that the fans have already been initialized.
 */
#ifdef CONFIG_CMD_FANTEST
#define FANTEST_RPM_INTERVAL 500
#define FANTEST_RPM_SETTLING_TIMEOUT 15000 /* 15 sec */
static int command_fantest(int argc, char **argv)
{
	uint16_t rpm, test_rpm_min, test_rpm_max;
	uint16_t actual_rpms_before_test [FAN_CH_COUNT];
	int sleep_timer, ret = EC_SUCCESS;
	enum fan_status status;

	for (uint8_t fan = 0; fan < FAN_CH_COUNT; fan++)
		actual_rpms_before_test[fan] = fan_get_rpm_actual(fan);

	ccprintf("Testing FAN RPM\n");
	for (uint8_t fan = 0; fan < FAN_CH_COUNT; fan++) {
		test_rpm_min = fans[fan].rpm->rpm_min;
		test_rpm_max = fans[fan].rpm->rpm_max;
		for (rpm = test_rpm_min; rpm < test_rpm_max; rpm += FANTEST_RPM_INTERVAL) {
			ccprintf("Testing %d rpm on FAN%d\n", rpm, fan);
			fan_set_rpm_target(fan, rpm);
			/*
			 * Trick to prevent spurious LOCKED status from old
			 * fan state disrupting the current LOCKED status check.
			 */
			fan_speed_state[fan].sts = FAN_STATUS_CHANGING;
			fan_ctrl();
			sleep_timer = FANTEST_RPM_SETTLING_TIMEOUT;
			do {
				msleep(100);
				sleep_timer -= 100;
				status = fan_get_status(fan);
				if (status == FAN_STATUS_FRUSTRATED ||
					status == FAN_STATUS_STOPPED) {
					ccprintf("FAN%d status %s. Fan test failed.\n",
						fan,
						status == FAN_STATUS_FRUSTRATED ? "frustrated" : "stopped"
						);
					ret = EC_ERROR_UNKNOWN;
					goto end;
				}
			} while (status != FAN_STATUS_LOCKED && sleep_timer > 0);
			if (sleep_timer <= 0) {
				ccprintf("FAN%d status did not report locked"
				" within timeout. Fan test failed.\n", fan);
				ret = EC_ERROR_TIMEOUT;
				goto end;
			}
		}
		fan_set_rpm_target(fan, actual_rpms_before_test[fan]);
		/*
		 * TODO: May be wait here for this fan to setttle to old speed
		 * before testing next fan.
		 */
	}
end:
	for (uint8_t fan = 0; fan < FAN_CH_COUNT; fan++)
		fan_set_rpm_target(fan, actual_rpms_before_test[fan]);
	return ret;
}
DECLARE_CONSOLE_COMMAND(fantest, command_fantest,
			"",
			"Run a fan test");
#endif
