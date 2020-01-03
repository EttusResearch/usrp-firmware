#ifndef __CROS_EC_PWRSUP_H
#define __CROS_EC_PWRSUP_H

#include <stdint.h>
#include "gpio.h"

enum pwrsup_id;

/*
 * struct pwrsup_info
 *
 * name: name of the supply
 * parent: enum pwrsup_id of the parent supply
 * enable: ioex or gpio that controls this supply
 *
 * flag_enable_inverted: if set, enable is active low
 * flag_mon_adc: if set, feedback is used as the ADC id to monitor, and level
 *    is the minimum voltage in millivolts to consider the output good
 * flag_mon_sig: if set, feedback is used as a signal to indicate power good
 */
struct pwrsup_info {
	const char *name;
	int parent;
	int enable;

	int feedback;
	int level;

	uint32_t flag_enable_inverted:1;
	uint32_t flag_mon_adc:1;
	uint32_t flag_mon_sig:1;
};

extern const struct pwrsup_info power_supply_list[];

enum pwrsup_status {
	PWRSUP_STATUS_OFF,
	PWRSUP_STATUS_ON,
	PWRSUP_STATUS_FAULT,
};

/*
 * PWRSUP_INFO: Helper macro for constructing a power_supply_info entry
 *
 * name_: the name of the supply
 * parent_: the name of the parent supply
 * enable_: the gpio or ioex to enable this supply
 * ... other parameters, such as a PWRSUP_MON entry
 */
#define PWRSUP_INFO(name_, parent_, enable_, ...) \
	[POWER_SUPPLY_ ## name_] = { \
		.name = #name_, \
		.parent = POWER_SUPPLY_ ## parent_, \
		.enable = (enable_), \
		__VA_ARGS__ \
	}

/*
 * macros for defining monitoring
 */
#define PWRSUP_MON_ADC(adc_, min_level_) \
	.feedback = (adc_), \
	.level = (min_level_), \
	.flag_mon_adc = 1

#define PWRSUP_MON_SIG(signal_) \
	.feedback = (signal_), \
	.flag_mon_sig = 1

/**
 * Turn on a power supply
 *
 * Turn on the specified power supply with specified delay, and check to ensure
 * power is good.
 *
 * @param ps:		Power supply to power on
 * @param delay:	Delay in milliseconds to wait before checking supply state
 *			or returning
 * @param timeout:	Time in milliseconds to wait for power good to assert
 */
int pwrsup_power_on(enum pwrsup_id ps, int delay, int timeout);
int pwrsup_power_off(enum pwrsup_id ps);

struct pwrsup_seq {
	int supply;
	uint8_t delay;
};

int pwrsup_seq_power_on(const struct pwrsup_seq *seq, int op_count);
int pwrsup_seq_power_off(const struct pwrsup_seq *seq, int op_count);
void pwrsup_seq_show(const struct pwrsup_seq *seq, int op_count);

int pwrsup_check_supplies(const struct pwrsup_seq *seq, int op_count);
enum pwrsup_status pwrsup_get_status(enum pwrsup_id ps);

void pwrsup_interrupt(enum gpio_signal signal);
#endif
