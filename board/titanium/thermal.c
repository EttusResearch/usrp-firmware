#include "assert.h"
#include "board_power.h"
#include "chipset.h"
#include "common.h"
#include "console.h"
#include "eeproms.h"
#include "fan.h"
#include "hooks.h"
#include "host_command.h"
#include "keyboard_protocol.h"
#include "keyboard_scan.h"
#include "led.h"
#include "math_util.h"
#include "mcu_flags.h"
#include "pwrsup.h"
#include "printf.h"
#include "temp_sensor.h"
#include "timer.h"
#include "usrp_eeprom.h"
#include "util.h"

/* #define COOLING_STRATEGY_COOL_MAX */
#define COOLING_STRATEGY_COOL_WEIGHTED_AVERAGE

enum cooling_required {
	COOL_IGNORE_ME = 0,
	COOL_ME = 1
};

struct temp_zone {
	const char *name;
	int t_target; /* deg Celcius */
	int t_hyst; /* deg Celcius */
	int t_warn; /* deg Celcius */
	int t_crit; /* deg Celcius */
	int tending_to_critical;
	enum cooling_required cooling_required;
	int cooling_weight; /* percentage */
	int cooling_requirement;
	int kp; /* percent proportional factor */
	int ki; /* percent integral factor */
	/*
	 * Note: We express KP and KI as percentages because it allows us to set
	 * them to non-integral values like 4.5 which will be set/appear as 450.
	 * We account for them being percentages by dividing by 100 when they
	 * are used for cooling calculation. This in turn gives a little more
	 * fine-grained control over each zone's cooling requirement.
	 */
};

struct temp_zone temp_zones[TEMP_SENSOR_COUNT] = {
	{"", 95, 115, 120, 130, 0, COOL_ME, 0, 0, 0, 0}, /* PMBUS-0 */
	{"", 95, 115, 120, 130, 0, COOL_ME, 0, 0, 0, 0}, /* PMBUS-1 */
	{"", 35, 50, 60, 70, 0, COOL_IGNORE_ME, 0, 0, 0, 0}, /* EC Internal */
	{"", 25, 40, 45, 50, 0, COOL_IGNORE_ME, 0,  0, 0, 0}, /* TMP464 Internal */
	{"", 60, 75, 80, 85, 0, COOL_ME, 0,  0, 0, 0}, /* Sample Clock PCB*/
	{"", 78, 85, 95, 99, 0, COOL_ME, 0, 0, 0, 0}, /* RFSoC */
	{"", 44, 75, 80, 85, 0, COOL_ME, 100,  0, 2700, 8}, /* DRAM PCB */
	{"", 80, 90, 95, 105, 0, COOL_ME, 0, 0, 0, 0}, /* Power Supply PCB */
	{"", 55, 80, 85, 90, 0, COOL_ME, 0, 0, 0, 0}, /* TMP112 DB0 Top */
	{"", 55, 80, 85, 90, 0, COOL_ME, 0, 0, 0, 0}, /* TMP112 DB0 Bottom */
	{"", 55, 80, 85, 90, 0, COOL_ME, 0, 0, 0, 0}, /* TMP112 DB1 Top */
	{"", 55, 80, 85, 90, 0, COOL_ME, 0, 0, 0, 0}, /* TMP112 DB1 Bottom */
};
BUILD_ASSERT(ARRAY_SIZE(temp_zones) == TEMP_SENSOR_COUNT);

#define NUM_MB_ZONES TEMP_SENSOR_COUNT - 4 /* 2 per DB */
#define NUM_DB_ZONES 2

int are_db_temp_sensors_present(int eeprom) {
	const struct usrp_eeprom_board_info *eep;
	eep = eeprom_lookup_tag(eeprom, USRP_EEPROM_BOARD_INFO_TAG);

	/* IF Test CCA DB does not have DB temp sensors */
	if (!eep)
		return 0;
	else if (eep->pid == 0x4006 /*IF Test CCA PID*/)
		return 0;

	return 1;
}

void init_db_temp_zones(int eeprom, struct temp_zone *zones, int nzones)
{
	if (!is_board_present(eeprom) || !are_db_temp_sensors_present(eeprom)) {
		ccprintf("warning! db not present or eeprom not initialized "
			"or no db temp sensors supported!\n");
		for (int i = 0; i < nzones; i++)
			zones[i].cooling_required = COOL_IGNORE_ME;
	}
}

static void init_temp_zones(void)
{
	for (int i = 0; i < TEMP_SENSOR_COUNT; i++)
		temp_zones[i].name = temp_sensors[i].name;

	/* Store zone information sequentially DB0 -> DB1 */
	init_db_temp_zones(TLV_EEPROM_DB0,
		temp_zones + NUM_MB_ZONES,
		NUM_DB_ZONES);
	init_db_temp_zones(TLV_EEPROM_DB1,
		temp_zones + NUM_MB_ZONES + NUM_DB_ZONES,
		NUM_DB_ZONES);
}
DECLARE_HOOK(HOOK_INIT, init_temp_zones, HOOK_PRIO_DEFAULT);

static void run_fans_manually(uint8_t capacity)
{
	int fixed_rpm = fan_percent_to_rpm(0, capacity);

	/* Configure fans to run in rpm mode at fixed cooling capacity. */
	for (uint8_t fan = 0; fan < FAN_CH_COUNT; fan++) {
		fan_set_rpm_mode(FAN_CH(fan), 1);
		fan_set_enabled(FAN_CH(fan), 1);
		gpio_set_level(fans[fan].conf->enable_gpio, 1);
		fan_set_rpm_target(FAN_CH(fan), fixed_rpm);
	}
}

static void init_fixed_cooling(void)
{
	const struct usrp_eeprom_fan_fixed_capacity *eep;
	eep = eeprom_lookup_tag(TLV_EEPROM_MB, USRP_EEPROM_FAN_FIXED_CAPACITY);

	if (!eep) {
		return;
	} else if (eep->capacity > 100) {
		ccprintf("warning! invalid fan fixed capacity value in eeprom."
			" Valid range is 0-100.\n");
		return;
	}

	ccprintf("Fixed Fan capacity read from eeprom. "
		"Disabling thermal control algorithm! "
		"Running fans at fixed %d%% capacity.\n", eep->capacity);
	for (uint8_t fan = 0; fan < FAN_CH_COUNT; fan++)
		set_thermal_control_enabled(fan, 0);

	run_fans_manually(eep->capacity);
}
/*
 * HOOK_CHIPSET_RESUME:
 * Ensure execution after power rails have been brought up and
 * after pwm_fan_start() (refer common/fan.c) which sets the
 * thermal_control_enabled state in order to overwrite what it sets.
 *
 * HOOK_INIT:
 * Typically, thermal control is in enabled state which sets the fans in
 * rpm mode (refer pwm_fan_init() in common/fan.c) and fans are able to retain
 * their state after a SYSJUMP but here we disable
 * thermal control on boot so the fan state gets reset to manual mode on a
 * SYSJUMP. Tying this function to HOOK_INIT ensures that the fan mode gets set
 * to rpm mode and fans continue to operate at fixed cooling after a SYSJUMP.
 * Ideally we can avoid this roundabout method by actually preserving the
 * fan mode in pwm_fan_preserve_state() and then restoring the state in
 * pwm_fan_init() but that would need modifcation to the pwm_fan_state struct
 * which is not advised.
 */
DECLARE_HOOK(HOOK_CHIPSET_RESUME, init_fixed_cooling, HOOK_PRIO_DEFAULT + 1);
DECLARE_HOOK(HOOK_INIT, init_fixed_cooling, HOOK_PRIO_DEFAULT);

static int get_total_cooling_weight(void)
{
	int total_cooling_weight = 0;

	for (int i = 0; i < TEMP_SENSOR_COUNT; i++)
		if (temp_zones[i].cooling_required == COOL_ME)
			total_cooling_weight += temp_zones[i].cooling_weight;

	if (total_cooling_weight == 0) {
		ccprintf("warning! total cooling weight is zero!\n");
		total_cooling_weight = 1;
	}

	return total_cooling_weight;
}

#define FAN_THERMAL_SHUTDOWN_DUTY 40
static void thermal_shutdown_run_fans(void)
{
	/* Turn on fan power supply */
	if (pwrsup_power_on(POWER_SUPPLY_12V, 0, 200)) {
		ccprintf("failed to enable 12v rail\n");
		ccprintf("can not turn on fans!\n");
		set_board_power_status(POWER_INPUT_BAD);
		return;
	}

	run_fans_manually(FAN_THERMAL_SHUTDOWN_DUTY);
}

static void force_thermal_shutdown(void)
{
	chipset_force_shutdown(CHIPSET_SHUTDOWN_THERMAL);
}
DECLARE_DEFERRED(force_thermal_shutdown);

/*
 * Called on AP S3S5 -> S5 transition.
 * When this executes we know that the device will power down automatically
 * as a part of normal shutdown sequence, so cancel the forced shutdown.
 */
static void cancel_forced_thermal_shutdown(void)
{
	hook_call_deferred(&force_thermal_shutdown_data, -1);
}
DECLARE_HOOK(HOOK_CHIPSET_SHUTDOWN, cancel_forced_thermal_shutdown,
	     HOOK_PRIO_DEFAULT);

static void post_thermal_shutdown(void)
{
	/* Set visual indication of thermal shutdown */
	set_board_power_status(POWER_BAD);

	/* Turn on cooling */
	thermal_shutdown_run_fans();
}
DECLARE_DEFERRED(post_thermal_shutdown);

#define THERMAL_SHUTDOWN_DELAY (2 * SECOND)

static int thermal_shutdown_state = 0;
static void thermal_shutdown(void)
{
	ccprintf("initiating thermal shutdown!\n");

	/* Initiate orderly power down on PS using MKBP mechanism. */

	/* Simulate power button press for fixed duration */
	keyboard_update_button(KEYBOARD_BUTTON_POWER, 1);
	msleep(200);

	/* Simulate power button release */
	keyboard_update_button(KEYBOARD_BUTTON_POWER, 0);

	hook_call_deferred(&force_thermal_shutdown_data, THERMAL_SHUTDOWN_DELAY);

	/* The 100 ms added delay is arbitrary. */
	hook_call_deferred(&post_thermal_shutdown_data, THERMAL_SHUTDOWN_DELAY + 100 * MSEC);
}

static void thermal_shutdown_recovery(void)
{
	if (chipset_in_state(CHIPSET_STATE_ON)) {
		ccprintf("device recovered from thermal shutdown!\n");
		thermal_shutdown_state = 0;
		return;
	}

	if (mcu_flags_get_thermal_recovery()) {
		ccprintf("starting recovery from thermal shutdown\n");
		chipset_exit_hard_off();
		thermal_shutdown_state = 0;
	}

	/* TODO: How to detect and recover from failure? */
}

static int all_zones_below_warning(void)
{
	int rv, t_zone;
	int below_warning = 1;

	for (int i = 0; i < TEMP_SENSOR_COUNT; i++) {
		struct temp_zone *z = &temp_zones[i];

		if (z->cooling_required != COOL_ME)
			continue;

		rv = temp_sensor_read(i /* temp_sensor_id */, &t_zone);
		if (rv) {
			ccprintf("warning! failed to read %s temperature sensor!\n",
				temp_sensors[i].name);
			below_warning = 0;
			break;
		}

		t_zone = K_TO_C(t_zone);
		if (t_zone >= z->t_warn) {
			below_warning = 0;
			break;
		}
	}

	return below_warning;
}

int pid_allowed_abs_min_error = 0; /* deg C */
int pid_allowed_abs_max_error = 10; /* deg C */
int pid_allowed_abs_max_integral = 750; /* deg C */
int pid_error_history_length = 50; /* number of readings used for averaging */
int pid_debug = 0; /* enable/disable debug prints */

#define ERR_HISTORY_MIN 1 /* at least one reading */
#define ERR_HISTORY_MAX 120 /* number of readings */
#define ERR_INIT 2 /* deg C initial error, arbitrary */
static float error_signal[TEMP_SENSOR_COUNT][ERR_HISTORY_MAX];
void init_error_signal(void)
{
	for (int i = 0; i < TEMP_SENSOR_COUNT; i++)
		for (int j = 0; j < ERR_HISTORY_MAX; j++)
			error_signal[i][j] = ERR_INIT;
}
DECLARE_HOOK(HOOK_INIT, init_error_signal, HOOK_PRIO_INIT_I2C + 2);

void pid_debug_print(char * format, ...)
{
	if (pid_debug) {
		char buffer[256];
		va_list args;
		va_start(args, format);
		vsnprintf(buffer, sizeof(buffer), format, args);
		ccprintf("%s", buffer);
		va_end(args);
	}
}

void update_average_error(float* error_signal_avg)
{
	int rv;
	float t_zone, error, sum_error;
	static uint8_t instant;

	pid_debug_print("new_err::");

	for (int i = 0; i < TEMP_SENSOR_COUNT; i++) {
		struct temp_zone *z = &temp_zones[i];
		if (z->cooling_required != COOL_ME) {
			continue;
		}

		rv = temp_sensor_readf(i /* temp_sensor_id */, &t_zone);
		if (rv) {
			ccprintf("warning! failed to read %s temperature sensor!\n",
				temp_sensors[i].name);
			continue;
		}

		t_zone = t_zone - 273.15f;

		error = t_zone - z->t_target;
		error_signal[i][instant] = error;

		pid_debug_print("%d:%d\t", i, (int)error);

		sum_error = 0;
		for (int j = 0; j < pid_error_history_length; j++)
			sum_error += error_signal[i][j];

		error_signal_avg[i] = (float)sum_error / (float)pid_error_history_length;
	}

	pid_debug_print("\n");

	instant++;

	/*
	 * Reset instant so that next write happens at first location in the
	 * error_signal array's sensors reading row.
	 * Note that if the pid_error_history_length setting is modified
	 * sometime during the run, reset instant if it is greater than the
	 * newly set pid_error_history_length value.
	 */
	if (instant >= pid_error_history_length)
		instant = 0;
}

static void cooling_calculator(void)
{
	int rv;
	float t_zone;
	int cool_percent = 0;

	static float error_signal_avg[TEMP_SENSOR_COUNT];
	static float integral[TEMP_SENSOR_COUNT];

	if (!is_thermal_control_enabled(FAN_CH_0) ||
		!is_thermal_control_enabled(FAN_CH_1))
		return;

	update_average_error(error_signal_avg);

	pid_debug_print("avg_err::");

	for (int i = 0; i < TEMP_SENSOR_COUNT; i++) {
		struct temp_zone *z = &temp_zones[i];
		float p_component; /* proportional component */
		float i_component; /* integral component */

		if (z->cooling_required != COOL_ME) {
			z->cooling_requirement = 0;
			continue;
		}

		/* Trick to print a float with single decimal precision */
		pid_debug_print("%d:%.1d\t", i, (int)(error_signal_avg[i] * 10));

		rv = temp_sensor_readf(i /* temp_sensor_id */, &t_zone);
		if (rv) {
			ccprintf("warning! failed to read %s temperature sensor!\n",
				temp_sensors[i].name);
			/* Assume worst case; run fans at full speed */
			z->cooling_requirement = 100;
			break;
		}

		t_zone = t_zone - 273.15f;

		if (t_zone >= z->t_warn) {
			ccprintf("%s temperature: %d is above warning limit, "
				"maximum cooling\n",
				z->name, (int)t_zone);
			cool_percent = 100;
			z->tending_to_critical = 1;
		} else if (t_zone < z->t_warn &&
				t_zone >= z->t_hyst &&
				z->tending_to_critical) {
			cool_percent = 100;
		} else if (t_zone < z->t_hyst &&
				z->tending_to_critical) {
			z->tending_to_critical = 0;
		}

		if (!z->tending_to_critical) {
			/*
			 * Reset the integral to zero if we reach very close to
			 * the setpoint to avoid "over-shoot at zero" error
			 * where a high integral value will cause us to
			 * excessively cool even if we have reached the
			 * setpoint.
			 * If the error is very high we want P control to work
			 * rather than I because integral will very quickly
			 * reach a very high unusable value.
			 * Note that we do have further limits on the maximum
			 * value that integral will take but this check/reset
			 * here helps prevents it.
			 * In effect we want integral component to play a role
			 * in an optimal error signal range.
			 */
			if (ABS(error_signal_avg[i]) <= pid_allowed_abs_min_error ||
				ABS(error_signal_avg[i]) >= pid_allowed_abs_max_error)
				integral[i] = 0;
			else
				integral[i] = integral[i] + error_signal_avg[i];

			/*
			 * Cap the Integral at a maximum to avoid wind-up error
			 * where the integral keeps on accumulating to a large
			 * unusable value during normal operation.
			 */
			integral[i] = CLAMP(integral[i],
					-pid_allowed_abs_max_integral,
					pid_allowed_abs_max_integral);

			/* KP and KI are expressed as percentages; account for that */
			p_component = error_signal_avg[i] * (float)z->kp / (float)100;
			i_component = integral[i] * (float)z->ki / (float)100;

			cool_percent = (int)(p_component + i_component);
		}

		if (cool_percent < 0)
			cool_percent = 0;

		if (cool_percent >= 100)
			cool_percent = 100;

		z->cooling_requirement = cool_percent;
	}

	pid_debug_print("\n");
}
DECLARE_HOOK(HOOK_SECOND, cooling_calculator, HOOK_PRIO_TEMP_SENSOR_DONE + 1);

static int get_aggregate_cooling(void)
{
	int aggregate_cooling = 0;
	int __maybe_unused total_cooling_weight = get_total_cooling_weight();

	if (!is_thermal_control_enabled(FAN_CH_0) ||
		!is_thermal_control_enabled(FAN_CH_1))
		return 0;

#ifdef COOLING_STRATEGY_COOL_MAX
	for (int i = 0; i < TEMP_SENSOR_COUNT; i++) {
		struct temp_zone *z = &temp_zones[i];
		if (z->cooling_requirement > aggregate_cooling)
			aggregate_cooling = z->cooling_requirement;
	}
#endif

#ifdef COOLING_STRATEGY_COOL_WEIGHTED_AVERAGE
	aggregate_cooling = 0;
	for (int i = 0; i < TEMP_SENSOR_COUNT; i++) {
		struct temp_zone *z = &temp_zones[i];
		if (z->cooling_required == COOL_ME) {
			if (z->cooling_requirement == 100) {
				aggregate_cooling = 100;
				break;
			}
			aggregate_cooling += (z->cooling_weight *
				z->cooling_requirement) / total_cooling_weight;
		}
	}
#endif

	if (aggregate_cooling < 0)
		aggregate_cooling = 0;
	else if(aggregate_cooling >= 100)
		aggregate_cooling = 100;

	return aggregate_cooling;
}

static void temp_control(void)
{
	int aggregate_cooling;

	if (!is_thermal_control_enabled(FAN_CH_0) ||
		!is_thermal_control_enabled(FAN_CH_1))
		return;

	aggregate_cooling = get_aggregate_cooling();

	fan_set_percent_needed(FAN_CH_0, aggregate_cooling);
	fan_set_percent_needed(FAN_CH_1, aggregate_cooling);
}
DECLARE_HOOK(HOOK_SECOND, temp_control, HOOK_PRIO_TEMP_SENSOR_DONE + 3);

static void critical_monitor(void)
{
	int rv, t_zone;

	/* Don't do anything if we are in thermal shutdown or not-powered up */
	if (thermal_shutdown_state || chipset_in_state(CHIPSET_STATE_HARD_OFF))
		return;

	for (int i = 0; i < TEMP_SENSOR_COUNT; i++) {
		struct temp_zone *z = &temp_zones[i];

		if (z->cooling_required != COOL_ME) {
			continue;
		}

		rv = temp_sensor_read(i /* temp_sensor_id */, &t_zone);
		/*
		 * TODO: Stricter policy? Keep track of sensor read failures and
		 * shutdown if repeated errors?
		 */
		if (rv) {
			ccprintf("warning! failed to read %s temperature sensor!\n",
				temp_sensors[i].name);
			continue;
		}

		t_zone = K_TO_C(t_zone);

		if (t_zone >= z->t_crit) {
			ccprintf("%s temperature: %d is critical\n",
				z->name, t_zone);
			thermal_shutdown_state = 1;
			thermal_shutdown();
			return;
		}
	}
}
DECLARE_HOOK(HOOK_SECOND, critical_monitor, HOOK_PRIO_TEMP_SENSOR_DONE + 2);

static void recovery_monitor(void)
{
	/* If we are NOT in thermal shutdown state; do nothing. */
	if (!thermal_shutdown_state)
		return;

	/* Check that board is cool enough to restart after thermal shutdown */
	if (all_zones_below_warning())
		thermal_shutdown_recovery();
}
DECLARE_HOOK(HOOK_SECOND, recovery_monitor, HOOK_PRIO_TEMP_SENSOR_DONE + 4);
