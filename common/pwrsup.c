#include "pwrsup.h"
#include "gpio.h"
#include "ioexpander.h"
#include "adc.h"
#include "timer.h"
#include "console.h"
#include "ec_commands.h"
#include "util.h"
#include "printf.h"
#include "task.h"
#include "hooks.h"

/*
 * states and transitions:
 *     OFF -> TURNING_ON            (power_on)
 *     TURNING_ON -> ON             (power good)
 *     TURNING_ON -> PG_TIMEOUT     (power good timeout)
 *     TURNING_ON -> TURN_ON_FAILED (failed to control supply)
 *     ON -> FAULT                  (!power good)
 *     FAULT -> TURNING_ON          (power_on)
 *     PG_TIMEOUT -> TURNING_ON     (power_on)
 *     TURN_ON_FAILED -> TURNING_ON (power_on)
 *     * -> OFF                     (power_off)
 */
enum pwrsup_state {
	PWRSUP_STATE_OFF,
	PWRSUP_STATE_TURNING_ON,
	PWRSUP_STATE_ON,
	PWRSUP_STATE_FAULT,
	PWRSUP_STATE_TURN_ON_FAILED,
	PWRSUP_STATE_PG_TIMEOUT,
};

static uint8_t supply_state[POWER_SUPPLY_COUNT];

static int pwrsup_powered_on(enum pwrsup_id ps)
{
	const struct pwrsup_info *sup = power_supply_list + ps;
	int level;

	if (signal_is_gpio(sup->enable))
		level = gpio_get_level(sup->enable);
	else if (signal_is_ioex(sup->enable))
		ioex_get_level(sup->enable, &level);

	if (sup->flag_enable_inverted)
		level = !level;

	return level;
}

static int pwrsup_control(enum pwrsup_id ps, int state)
{
	const struct pwrsup_info *sup = power_supply_list + ps;

	if (sup->flag_enable_inverted)
		state = !state;

	if (state && sup->parent != ps && !pwrsup_powered_on(sup->parent)) {
		ccprintf("attempting to turn on %s, but %s (parent) is off\n",
			 sup->name, power_supply_list[sup->parent].name);
		return -1;
	}

	if (signal_is_gpio(sup->enable))
		gpio_set_level(sup->enable, state);
	else if (signal_is_ioex(sup->enable))
		return ioex_set_level(sup->enable, state);
	else
		return -1;

	return 0;
}

static const char *pwrsup_get_name(enum pwrsup_id ps)
{
	const struct pwrsup_info *sup = power_supply_list + ps;
	return sup->name;
}

static enum pwrsup_id pwrsup_lookup(const char *name)
{
	for (int i = 0; i < POWER_SUPPLY_COUNT; i++)
		if (!strcasecmp(name, pwrsup_get_name(i)))
			return i;
	return POWER_SUPPLY_COUNT;
}

static int pwrsup_get_parent(enum pwrsup_id ps)
{
	const struct pwrsup_info *sup = power_supply_list + ps;
	return sup->parent;
}

static int pwrsup_get_voltage(enum pwrsup_id ps)
{
	const struct pwrsup_info *sup = power_supply_list + ps;

	if (sup->flag_mon_adc)
		return adc_read_channel(sup->feedback);

	return -1;
}

enum pwrsup_status pwrsup_get_status(enum pwrsup_id ps)
{
	const struct pwrsup_info *sup = power_supply_list + ps;
	int level;

	if (!pwrsup_powered_on(ps))
		return PWRSUP_STATUS_OFF;

	if (sup->flag_mon_adc) {
		level = adc_read_channel(sup->feedback);
		if (level < sup->level) {
			ccprintf("%s level %d is below min %d, reporting fault\n",
				 sup->name, level, sup->level);
			return PWRSUP_STATUS_FAULT;
		}
	} else if (sup->flag_mon_sig) {
		if (signal_is_gpio(sup->feedback) &&
		    !gpio_get_level(sup->feedback)) {
			ccprintf("%s powergood went low, reporting fault\n",
				 sup->name);
			return PWRSUP_STATUS_FAULT;
		} else if (signal_is_ioex(sup->feedback)) {
			ioex_get_level(sup->feedback, &level);
			if (!level) {
				return PWRSUP_STATUS_FAULT;
				ccprintf("%s powergood went low, reporting fault\n",
					 sup->name);
			}
		}
	} else {
		/* No way of checking, so hope for the best */
		return PWRSUP_STATUS_ON;
	}

	return PWRSUP_STATUS_ON;
}

static const char *pwrsup_get_state_str(enum pwrsup_id ps)
{
	switch (supply_state[ps]) {
	case PWRSUP_STATE_OFF: return "off";
	case PWRSUP_STATE_TURNING_ON: return "turning on";
	case PWRSUP_STATE_ON: return "on";
	case PWRSUP_STATE_FAULT: return "fault";
	case PWRSUP_STATE_TURN_ON_FAILED: return "turn on failed";
	case PWRSUP_STATE_PG_TIMEOUT: return "timeout";
	}

	return "??";
}

int pwrsup_power_on(enum pwrsup_id ps, int delay, int timeout)
{
	const struct pwrsup_info *sup = power_supply_list + ps;
	const int timeout_in = timeout;
	int rv;

	if (supply_state[ps] == PWRSUP_STATE_ON)
		return 0;

	supply_state[ps] = PWRSUP_STATE_TURNING_ON;

	rv = pwrsup_control(ps, 1);
	if (rv) {
		supply_state[ps] = PWRSUP_STATE_TURN_ON_FAILED;
		return rv;
	}

	if (delay)
		msleep(delay);

	do {
		if (pwrsup_get_status(ps) == PWRSUP_STATUS_ON)
			break;
		msleep(1);
	} while (timeout--);

	if (pwrsup_get_status(ps) != PWRSUP_STATUS_ON) {
		ccprintf("pwrsup: failed to bring up %s, polled %d ms (of %d ms)\n",
			 sup->name, timeout_in - timeout, timeout_in);
		if (sup->flag_mon_adc)
			ccprintf("min voltage: %u mV, cur voltage: %u\n",
				 sup->level, pwrsup_get_voltage(ps));
		supply_state[ps] = PWRSUP_STATE_PG_TIMEOUT;
		return -1;
	}

	supply_state[ps] = PWRSUP_STATE_ON;

	if (0) /* helpful for profiling */
		ccprintf("supply %s came up in %d ms (timeout: %d ms, delay: %d ms)\n",
			 sup->name, timeout_in - timeout, timeout_in, delay);

	return 0;
}

int pwrsup_power_off(enum pwrsup_id ps)
{
	supply_state[ps] = PWRSUP_STATE_OFF;
	return pwrsup_control(ps, 0);
}

int pwrsup_seq_power_on(const struct pwrsup_seq *seq, int op_count)
{
	int i, rv = 0;

	for (i = 0; i < op_count; i++) {
		rv = pwrsup_power_on(seq[i].supply, seq[i].delay, 50);
		if (rv) {
			ccprintf("failed to run sequence!\n");
			return rv;
		}
	}

	for (i = 0; i < op_count; i++) {
		if (pwrsup_get_status(seq[i].supply) != PWRSUP_STATUS_ON) {
			ccprintf("pwrsup: %s is not on after running full sequence\n",
				 pwrsup_get_name(seq[i].supply));
			rv = -1;
		}
	}

	return rv;
}

int pwrsup_seq_power_off(const struct pwrsup_seq *seq, int op_count)
{
	for (int i = op_count - 1; i >= 0; i--)
		pwrsup_power_off(seq[i].supply);

	return 0;
}

void pwrsup_seq_show(const struct pwrsup_seq *seq, int op_count)
{
	for (int i = 0; i < op_count; i++)
		ccprintf("step %u: %s, %u ms\n",
			 i, pwrsup_get_name(seq[i].supply), seq[i].delay);
}

/*
 * quick and dirty topological sort; this is only run once when the host
 * command is run, so this is good enough
 */
static struct {
	enum pwrsup_id sup;
	int depth;
} pwrsup_sorted[POWER_SUPPLY_COUNT];
static int pwrsup_sorted_idx;

static int __get_depth(enum pwrsup_id ps)
{
	int depth = 0;

	while (ps != pwrsup_get_parent(ps)) {
		depth++;
		ps = pwrsup_get_parent(ps);
	}

	return depth;
}

static int __in_stack(enum pwrsup_id ps)
{
	for (int i = 0; i < pwrsup_sorted_idx; i++)
		if (pwrsup_sorted[i].sup == ps)
			return 1;

	return 0;
}

static void __pwrsup_sort(enum pwrsup_id ps)
{
	for (int i = 0; i < POWER_SUPPLY_COUNT; i++) {
		if (pwrsup_get_parent(i) == ps && !__in_stack(i)) {
			pwrsup_sorted[pwrsup_sorted_idx].sup = i;
		        pwrsup_sorted[pwrsup_sorted_idx].depth = __get_depth(i);
			pwrsup_sorted_idx++;
			__pwrsup_sort(i);
		}
	}
}

static int command_pwrsup(int argc, char **argv)
{
	const struct pwrsup_info *sup;
	int i, depth, level, rv = 0;
	enum pwrsup_id ps;
	char buf[32];

	if (!pwrsup_sorted_idx)
		__pwrsup_sort(0);

	if (argc == 1) {
		ccprintf("%-20s%-10s%-10s\n\n", "supply", "state", "voltage");
		for (i = 0; i < POWER_SUPPLY_COUNT; i++) {
			ps = pwrsup_sorted[i].sup;
			depth = pwrsup_sorted[i].depth;
			sup = power_supply_list + ps;

			for (int j = 0; j < depth; j++)
				ccprintf("  ");
			ccprintf("%s", sup->name);
			for (int j = 0; j < 20 - (strlen(sup->name) + 2 * depth); j++)
				ccprintf(" ");

			ccprintf("%-10s", pwrsup_get_state_str(ps));

			buf[0] = 0;
			level = pwrsup_get_voltage(ps);
			if (level >= 0 && pwrsup_powered_on(ps))
				snprintf(buf, sizeof(buf), "%6u mV", level);
			ccprintf("%-10s\n", buf);
		}
	} else {
		if (!strcasecmp(argv[1], "on") || !strcasecmp(argv[1], "off")) {
			if (argc != 3)
				return EC_ERROR_PARAM_COUNT;
			ps = pwrsup_lookup(argv[2]);
			if (ps == POWER_SUPPLY_COUNT) {
				ccprintf("can't find supply %s\n", argv[2]);
				return EC_ERROR_PARAM2;
			}
			if (!strcasecmp(argv[1], "on"))
				return pwrsup_power_on(ps, 100, 500);
			else
				return pwrsup_power_off(ps);
		}
	}

	return rv;
}
DECLARE_CONSOLE_COMMAND(pwrsup, command_pwrsup, "", "show power supplies");

static void pwrsup_deferred(void)
{
	enum pwrsup_status status;
	int last_voltage;

	for (int i = 0; i < POWER_SUPPLY_COUNT; i++) {
		/* only check rails that are on */
		if (supply_state[i] != PWRSUP_STATE_ON)
			continue;

		status = pwrsup_get_status(i);
		if (status == PWRSUP_STATUS_FAULT) {
			supply_state[i] = PWRSUP_STATE_FAULT;
			last_voltage = pwrsup_get_voltage(i);
			ccprintf("pwrsup: %s fault! disabling...\n",
				 pwrsup_get_name(i));
			if (last_voltage != -1)
				ccprintf("  voltage: %d\n", last_voltage);
			pwrsup_control(i, 0);
		}
	}

	task_wake(TASK_ID_CHIPSET);
}
DECLARE_DEFERRED(pwrsup_deferred);

void pwrsup_interrupt(enum gpio_signal signal)
{
	hook_call_deferred(&pwrsup_deferred_data, 0);
}

int pwrsup_check_supplies(const struct pwrsup_seq *seq, int op_count)
{
	enum pwrsup_status status;
	int okay;

	okay = 1;
	for (int i = 0; i < op_count; i++) {
		status = pwrsup_get_status(seq[i].supply);
		if (status != PWRSUP_STATUS_ON) {
			ccprintf("pwrsup_check_supplies: %s is not on\n",
				 pwrsup_get_name(i));
			okay = 0;
		}
	}

	return okay;
}

static void pwrsup_init(void)
{
	/*
	 * Initialize state based on live supply status.
	 */

	for (int i = 0; i < POWER_SUPPLY_COUNT; i++)
		switch (pwrsup_get_status(i)) {
		case PWRSUP_STATUS_ON:
			supply_state[i] = PWRSUP_STATE_ON;
			break;
		case PWRSUP_STATUS_OFF:
			supply_state[i] = PWRSUP_STATE_OFF;
			break;
		case PWRSUP_STATUS_FAULT:
			supply_state[i] = PWRSUP_STATE_FAULT;
			break;
		}
}
DECLARE_HOOK(HOOK_INIT, pwrsup_init, HOOK_PRIO_DEFAULT);
