#include "assert.h"
#include "chipset.h"
#include "common.h"
#include "console.h"
#include "eeproms.h"
#include "fan.h"
#include "hooks.h"
#include "host_command.h"
#include "printf.h"
#include "temp_sensor.h"
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
};

/*
 * TODO: Start with non-arbitrary values.
 * Choose good defaults based on data from thermal VnV.
 */
struct temp_zone temp_zones[TEMP_SENSOR_COUNT] = {
	{"", 45, 60, 65, 80, 0, COOL_ME, 10, 0}, /* PMBUS-0 */
	{"", 45, 60, 65, 80, 0, COOL_ME, 10, 0}, /* PMBUS-1 */
	{"", 35, 50, 60, 70, 0, COOL_ME, 10, 0}, /* EC Internal */
	{"", 25, 40, 45, 50, 0, COOL_ME, 5,  0}, /* TMP464 Internal */
	{"", 35, 50, 60, 65, 0, COOL_ME, 5,  0}, /* TMP464 Remote1 */
	{"", 35, 50, 55, 60, 0, COOL_ME, 25, 0}, /* RFSoC */
	{"", 25, 35, 40, 45, 0, COOL_ME, 5,  0}, /* TMP464 Remote3 */
	{"", 35, 50, 55, 60, 0, COOL_ME, 10, 0}, /* TMP464 Remote4 */
	{"", 50, 80, 85, 100, 0, COOL_ME, 5, 0}, /* TMP112 DB0 Top */
	{"", 50, 80, 85, 100, 0, COOL_ME, 5, 0}, /* TMP112 DB0 Bottom */
	{"", 50, 80, 85, 100, 0, COOL_ME, 5, 0}, /* TMP112 DB1 Top */
	{"", 50, 80, 85, 100, 0, COOL_ME, 5, 0}, /* TMP112 DB1 Bottom */
};
BUILD_ASSERT(ARRAY_SIZE(temp_zones) == TEMP_SENSOR_COUNT);

#define NUM_MB_ZONES TEMP_SENSOR_COUNT - 4 /* 2 per DB */
#define NUM_DB_ZONES 2

void init_db_temp_zones(int eeprom, struct temp_zone *zones, int nzones)
{
	if (!is_board_present(eeprom)) {
		ccprintf("warning! db not present or eeprom not initialized!\n");
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

static void cooling_calculator(void)
{
	int rv, t_zone;
	int cool_percent = 0;

	if (!is_thermal_control_enabled(FAN_CH_0) ||
		!is_thermal_control_enabled(FAN_CH_1))
		return;

	for (int i = 0; i < TEMP_SENSOR_COUNT; i++) {
		struct temp_zone *z = &temp_zones[i];
		if (z->cooling_required != COOL_ME) {
			z->cooling_requirement = 0;
			continue;
		}

		rv = temp_sensor_read(i /* temp_sensor_id */, &t_zone);
		if (rv) {
			ccprintf("warning! failed to read %s temperature sensor!\n",
				temp_sensors[i].name);
			/* Assume worst case; run fans at full speed */
			z->cooling_requirement = 100;
			break;
		}

		t_zone = K_TO_C(t_zone);

		if (t_zone >= z->t_crit) {
			/*
			 * TODO: This is an insufficient solution for thermal
			 * shutdown because it can be easily disabled when
			 * thermal control of fans is disabled. Modify.
			 */
			ccprintf("%s temperature: %d is critical, "
				"forcing shutdown\n",
				z->name, t_zone);
			chipset_force_shutdown(CHIPSET_SHUTDOWN_THERMAL);
		} else if (t_zone >= z->t_warn) {
			ccprintf("%s temperature: %d is above warning limit, "
				"maximum cooling\n",
				z->name, t_zone);
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
			if (z->t_warn - z->t_target != 0) {
				cool_percent = 100 * (t_zone - z->t_target) / (z->t_warn - z->t_target);
			} else {
				ccprintf("warning! t_warn equals t_target for "
					"zone %d (%s); change them to be not equal\n",
					i, z->name);
				cool_percent = 100;
			}
		}

		if (cool_percent < 0)
			cool_percent = 0;

		if (cool_percent >= 100)
			cool_percent = 100;

		z->cooling_requirement = cool_percent;
	}
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

	if (aggregate_cooling < 0)
		aggregate_cooling = 0;
	else if(aggregate_cooling >= 100)
		aggregate_cooling = 100;
#endif
	return aggregate_cooling;
}

static void temp_control(void)
{
	int aggregate_cooling = get_aggregate_cooling();

	if (!is_thermal_control_enabled(FAN_CH_0) ||
		!is_thermal_control_enabled(FAN_CH_1))
		return;

	/* ccprintf("Aggregate Cooling: %d %%\n", aggregate_cooling); */

	fan_set_percent_needed(FAN_CH_0, aggregate_cooling);
	fan_set_percent_needed(FAN_CH_1, aggregate_cooling);
}
DECLARE_HOOK(HOOK_SECOND, temp_control, HOOK_PRIO_TEMP_SENSOR_DONE + 2);

static int command_tempzones(int argc, char **argv)
{
	int t_zone, rv, total_cooling_weight;
	char *e;
	char buffer[10];

	if (!is_thermal_control_enabled(FAN_CH_0) ||
		!is_thermal_control_enabled(FAN_CH_1))
		ccprintf("warning! thermal algorithm is currently disabled!\n");

	if (argc == 1) {
		total_cooling_weight = get_total_cooling_weight();

		/* Print temperature zones information. */
		ccprintf("\n**** Temperature Zone Summary ****\n");
		ccprintf("%-10s%s","Current:", "Current temperature\n");
		ccprintf("%-10s%s","Target:",
			"Ideal operating temperature to aim for\n");
		ccprintf("%-10s%s","Hyst:",
			"Temp at which to cease maximum cooling\n");
		ccprintf("%-10s%s","Warn:",
			"Temp at which maximum cooling is initiated\n");
		ccprintf("%-10s%s","Crit:",
			"Dangerous to operate at this temperature;"
			" initiate device shutdown\n");
		ccprintf("\n%-5s%-20s%-10s%-10s%-10s%-10s%-10s%-15s%-20s%-20s\n",
			"Zone", "Name", "Current", "Target", "Hyst", "Warn",
			"Crit", "Is_critical", "Cooling Weight",
			"Cooling Req (%)");

		for (int i = 0; i < ARRAY_SIZE(temp_zones); i++) {
			struct temp_zone *z = &temp_zones[i];
			if (z->cooling_required == COOL_ME) {
				rv = temp_sensor_read(i /* temp_sensor_id */, &t_zone);
				if (rv) {
					ccprintf("warning! failed to read %s "
						"temperature sensor!\n",
						temp_sensors[i].name);
					t_zone = 273;/* 0 deg C */
				}

				snprintf(buffer,
					sizeof(buffer),
					"%d (%d%%)",
					z->cooling_weight,
					z->cooling_weight * 100 / total_cooling_weight);
				ccprintf("%-5d%-20s%-10d%-10d%-10d%-10d%-10d%-15s%-20s%-20d\n",
					i,
					z->name,
					K_TO_C(t_zone),
					z->t_target,
					z->t_hyst,
					z->t_warn,
					z->t_crit,
					z->tending_to_critical ? "yes" : "no",
					buffer,
					z->cooling_requirement);
			}
		}

		ccprintf("\nAggregate Cooling is %d %% \n", get_aggregate_cooling());
	} else if (argc == 4) {
		int zone, val;
		zone = strtoi(argv[1], &e, 0);
		if (*e || zone < 0 || zone > TEMP_SENSOR_COUNT - 1) {
			ccprintf ("Invalid zone number %d,"
				"valid zone numbers are: ",
				 zone);
			for (int i = 0; i < TEMP_SENSOR_COUNT; i++) {
				struct temp_zone *z = &temp_zones[i];
				if (z->cooling_required == COOL_ME)
					ccprintf("%d ", i);
			}
			ccprintf("\n");
			return EC_ERROR_PARAM1;
		}

		val = strtoi(argv[3], &e, 0);
		if (*e)
			return EC_ERROR_PARAM3;

		if (!strcasecmp(argv[2], "target"))
			temp_zones[zone].t_target = val;
		else if (!strcasecmp(argv[2], "hyst"))
			temp_zones[zone].t_hyst = val;
		else if (!strcasecmp(argv[2], "warn"))
			temp_zones[zone].t_warn = val;
		else if (!strcasecmp(argv[2], "crit"))
			temp_zones[zone].t_crit = val;
		else if (!strcasecmp(argv[2], "weight"))
			temp_zones[zone].cooling_weight = val;
		else
			return EC_ERROR_PARAM2;

	} else {
		return EC_ERROR_PARAM_COUNT;
	}

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(tempzones, command_tempzones,
			"{zone} {target|hyst|warn|crit|weight} (value)",
			"Get/Set temperature zone metrics.");
