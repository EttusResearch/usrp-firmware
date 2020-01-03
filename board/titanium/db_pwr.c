
#include "gpio.h"
#include "gpio_signal.h"
#include "timer.h"
#include "console.h"
#include "host_command.h"
#include "util.h"
#include "ioexpander.h"
#include "hooks.h"
#include "eeproms.h"
#include "usrp_eeprom.h"
#include "pwrsup.h"
#include "assert.h"

#define DB_SUPPLY_1V8   (0)
#define DB_SUPPLY_2V5   (1)
#define DB_SUPPLY_3V3   (2)
#define DB_SUPPLY_3V7   (3)
#define DB_SUPPLY_12V   (4)
#define DB_SUPPLY_MCU   (5)
#define DB_SUPPLY_COUNT (6)

#define VALID_SUPPLY_MASK (BIT(DB_SUPPLY_COUNT)-1)

enum db_pwr_state {
	DB_PWR_STATE_OFF = 0,
	DB_PWR_STATE_ON = 1,
	DB_PWR_STATE_FAULT = 2,
};

static const char * const state_strs[] = {
	[DB_PWR_STATE_OFF] = "off",
	[DB_PWR_STATE_ON] = "on",
	[DB_PWR_STATE_FAULT] = "error",
};

struct db_pwr {
	enum db_pwr_state state;
	const enum pwrsup_id supply[DB_SUPPLY_COUNT];
	enum ioex_signal spi_oe_l;
};

static struct db_pwr db0_pwr = {
	.state = DB_PWR_STATE_OFF,
	.supply = {
		[DB_SUPPLY_1V8] = POWER_SUPPLY_DB0_1V8,
		[DB_SUPPLY_2V5] = POWER_SUPPLY_DB0_2V5,
		[DB_SUPPLY_3V3] = POWER_SUPPLY_DB0_3V3,
		[DB_SUPPLY_3V7] = POWER_SUPPLY_DB0_3V7,
		[DB_SUPPLY_12V] = POWER_SUPPLY_DB0_12V,
		[DB_SUPPLY_MCU] = POWER_SUPPLY_DB0_3V3MCU,
	},
	.spi_oe_l = IOEX_DB0_SPI_OE_L,
};

static struct db_pwr db1_pwr = {
	.state = DB_PWR_STATE_OFF,
	.supply = {
		[DB_SUPPLY_1V8] = POWER_SUPPLY_DB1_1V8,
		[DB_SUPPLY_2V5] = POWER_SUPPLY_DB1_2V5,
		[DB_SUPPLY_3V3] = POWER_SUPPLY_DB1_3V3,
		[DB_SUPPLY_3V7] = POWER_SUPPLY_DB1_3V7,
		[DB_SUPPLY_12V] = POWER_SUPPLY_DB1_12V,
		[DB_SUPPLY_MCU] = POWER_SUPPLY_DB1_3V3MCU,
	},
	.spi_oe_l = IOEX_DB1_SPI_OE_L,
};

#define MAX_NUM_STEPS 8
struct db_pwr_seq {
	uint8_t valid;
	uint8_t nsteps;
	struct pwrsup_seq seq[MAX_NUM_STEPS];
};

static struct db_pwr_seq db0_seq;
static struct db_pwr_seq db1_seq;

static void db_pwr_seq_read(int eeprom, const struct db_pwr *pwr, struct db_pwr_seq *seq)
{
	const struct usrp_eeprom_db_pwr_seq *eep;
	uint8_t i, j;

	seq->nsteps = 0;

	eep = eeprom_lookup_tag(eeprom, USRP_EEPROM_DB_PWR_SEQ_TAG);
	if (!eep)
		return;

	if (eep->nsteps > MAX_NUM_STEPS ||
	    eep->nsteps > ARRAY_SIZE(eep->steps)) {
		ccprintf("invalid number of db sequence steps! %u\n", eep->nsteps);
		return;
	}

	/*
	 * EEPROM format is compact and allows multiple supplies enabled
	 * per step. This unrolls that into a flat sequence,
	 */
	for (i = 0; i < eep->nsteps; i++) {
		for (j = 0; j < DB_SUPPLY_COUNT; j++) {
			uint16_t mask = eep->steps[i].supply_mask;
			if (mask & BIT(j)) {
				/* only delay on the last one */
				if ((mask >> (j + 1)) == 0)
					seq->seq[seq->nsteps].delay = eep->steps[i].delay;
				seq->seq[seq->nsteps++].supply = pwr->supply[j];
				assert(seq->nsteps < MAX_NUM_STEPS);
			}
		}
	}

	seq->valid = 1;
}

static int db_poweron(struct db_pwr *db, const struct db_pwr_seq *seq)
{
	int rv = 0;

	if (!seq->valid) {
		ccprintf("error: attempted to power on daughterboard without a valid sequence\n");
		return -1;
	}

	if (db->state == DB_PWR_STATE_ON)
		return 0;

	rv = pwrsup_seq_power_on(seq->seq, seq->nsteps);
	if (rv)
		goto err;

	db->state = DB_PWR_STATE_ON;

	rv = ioex_set_level(db->spi_oe_l, 0);
	if (rv)
		goto err;

	return rv;
err:
	db->state = DB_PWR_STATE_FAULT;
	return rv;
}

static int db_poweroff(struct db_pwr *db, const struct db_pwr_seq *seq)
{
	if (db->state == DB_PWR_STATE_OFF)
		return 0;

	ioex_set_level(db->spi_oe_l, 1);

	pwrsup_seq_power_off(seq->seq, seq->nsteps);

	db->state = DB_PWR_STATE_OFF;

	return 0;
}

static int db_pwr_show_status(const struct db_pwr *db)
{
	ccprintf("supply is: %s\n", state_strs[db->state]);
	return 0;
}

static int db_pwr_show_seq(const struct db_pwr_seq *seq)
{
	if (!seq->valid) {
		ccprintf("no valid sequence loaded\n");
		return 0;
	}

	pwrsup_seq_show(seq->seq, seq->nsteps);

	return 0;
}

void db_pwr_init(void)
{
	db_pwr_seq_read(TLV_EEPROM_DB0, &db0_pwr, &db0_seq);
	db_pwr_seq_read(TLV_EEPROM_DB1, &db1_pwr, &db1_seq);
}
DECLARE_HOOK(HOOK_INIT, db_pwr_init, HOOK_PRIO_DEFAULT + 2);

static int command_dbpwr(int argc, char **argv)
{
	const struct db_pwr_seq *seq;
	struct db_pwr *db;
	int which;

	if (argc < 3)
		return EC_ERROR_PARAM_COUNT;

	if (*argv[1] != '0' && *argv[1] != '1')
		return EC_ERROR_PARAM1;

	which = *argv[1] - '0';
	db = which ? &db1_pwr : &db0_pwr;
	seq = which ? &db1_seq : &db0_seq;

	if (!strcasecmp(argv[2], "status"))
		return db_pwr_show_status(db);
	else if (!strcasecmp(argv[2], "seq"))
		return db_pwr_show_seq(seq);
	else if (!strcasecmp(argv[2], "on"))
		return db_poweron(db, seq);
	else if (!strcasecmp(argv[2], "off"))
		return db_poweroff(db, seq);

	return EC_ERROR_PARAM2;
}
DECLARE_CONSOLE_COMMAND(dbpwr, command_dbpwr, "[0|1] [on|off|seq|status]",
			"control daughterboard power");

#define EC_REGULATOR_CTRL_OFF	BIT(0)
#define EC_REGULATOR_CTRL_ON	BIT(1)

static enum ec_status command_regulator_control(struct host_cmd_handler_args *args)
{
	const struct ec_params_regulator_control *p = args->params;
	const struct db_pwr_seq *seq;
	struct db_pwr *db;

	if (p->regulator != 0 && p->regulator != 1)
		return EC_RES_INVALID_PARAM;

	if (p->control & ~(EC_REGULATOR_CTRL_OFF | EC_REGULATOR_CTRL_ON))
		return EC_RES_INVALID_PARAM;

	if (p->control & EC_REGULATOR_CTRL_OFF &&
	    p->control & EC_REGULATOR_CTRL_ON)
		return EC_RES_INVALID_PARAM;

	db = p->regulator ? &db1_pwr : &db0_pwr;
	seq = p->regulator ? &db1_seq : &db0_seq;

	if (p->control & EC_REGULATOR_CTRL_ON)
		return db_poweron(db, seq);
	else if (p->control & EC_REGULATOR_CTRL_OFF)
		return db_poweroff(db, seq);

	return EC_RES_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_REGULATOR_CONTROL,
		     command_regulator_control,
		     EC_VER_MASK(0));

#define EC_REGULATOR_STATUS_OFF		BIT(0)
#define EC_REGULATOR_STATUS_ON		BIT(1)
#define EC_REGULATOR_STATUS_FAULT	BIT(2)

static enum ec_status command_regulator_status(struct host_cmd_handler_args *args)
{
	const struct ec_params_regulator_status *p = args->params;
	struct ec_response_regulator_status *r = args->response;
	const struct db_pwr *db;

	if (p->regulator != 0 && p->regulator != 1)
		return EC_RES_INVALID_PARAM;

	db = p->regulator ? &db1_pwr : &db0_pwr;
	switch (db->state) {
	case DB_PWR_STATE_OFF:
		r->status = EC_REGULATOR_STATUS_OFF;
		break;
	case DB_PWR_STATE_ON:
		r->status = EC_REGULATOR_STATUS_ON;
		break;
	case DB_PWR_STATE_FAULT:
		r->status = EC_REGULATOR_STATUS_FAULT;
		break;
	}

	args->response_size = 1;

	return EC_RES_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_REGULATOR_STATUS,
		     command_regulator_status,
		     EC_VER_MASK(0));
