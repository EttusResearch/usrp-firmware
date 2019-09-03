
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

#define SUPPLY_1V8   (0)
#define SUPPLY_2V5   (1)
#define SUPPLY_3V3   (2)
#define SUPPLY_3V7   (3)
#define SUPPLY_12V   (4)
#define SUPPLY_MCU   (5)
#define SUPPLY_COUNT (6)

#define VALID_SUPPLY_MASK (BIT(SUPPLY_COUNT)-1)

struct db_pwr_supply {
	const char *name;
	enum ioex_signal enable;
	enum ioex_signal status;
};

struct db_pwr {
	uint8_t state;
	const struct db_pwr_supply supply[SUPPLY_COUNT];
	enum ioex_signal spi_oe_l;
};

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

static struct db_pwr db0_pwr = {
	.state = DB_PWR_STATE_OFF,
	.supply = {
		[SUPPLY_1V8] = { "+1V8", IOEX_DB0_1V8_EN, IOEX_DB0_1V8_PG },
		[SUPPLY_2V5] = { "+2V5", IOEX_DB0_2V5_EN, IOEX_DB0_2V5_PG },
		[SUPPLY_3V3] = { "+3V3", IOEX_DB0_3V3_EN, IOEX_DB0_3V3_PG },
		[SUPPLY_3V7] = { "+3V7", IOEX_DB0_3V7_EN, IOEX_DB0_3V7_PG },
		[SUPPLY_12V] = { "+12V", IOEX_DB0_12V_EN, IOEX_DB0_12V_PG },
		[SUPPLY_MCU] = { "+3V3M", IOEX_DB0_3V3MCU_EN, IOEX_DB0_3V3MCU_PG },
	},
	.spi_oe_l = IOEX_DB0_SPI_OE_L,
};

static struct db_pwr db1_pwr = {
	.state = DB_PWR_STATE_OFF,
	.supply = {
		[SUPPLY_1V8] = { "+1V8", IOEX_DB1_1V8_EN, IOEX_DB1_1V8_PG },
		[SUPPLY_2V5] = { "+2V5", IOEX_DB1_2V5_EN, IOEX_DB1_2V5_PG },
		[SUPPLY_3V3] = { "+3V3", IOEX_DB1_3V3_EN, IOEX_DB1_3V3_PG },
		[SUPPLY_3V7] = { "+3V7", IOEX_DB1_3V7_EN, IOEX_DB1_3V7_PG },
		[SUPPLY_12V] = { "+12V", IOEX_DB1_12V_EN, IOEX_DB1_12V_PG },
		[SUPPLY_MCU] = { "+3V3M", IOEX_DB1_3V3MCU_EN, IOEX_DB1_3V3MCU_PG },
	},
	.spi_oe_l = IOEX_DB1_SPI_OE_L,
};

static int inline __db_supply_status(const struct db_pwr_supply *sup,
				     int *enable, int *status)
{
	int rv = 0;

	if (enable)
		rv = ioex_get_level(sup->enable, enable);
	if (rv)
		return rv;

	if (status)
		rv = ioex_get_level(sup->status, status);
	if (rv)
		return rv;

	return rv;
}

static int db_supply_check_status(const struct db_pwr_supply *sup)
{
	int enable, status, rv;

	rv = __db_supply_status(sup, &enable, &status);
	if (rv)
		return rv;

	/*
	 * Return error if the supply is in a fault condition
	 */
	if (enable && !status) {
		ccprintf("%s enabled but power good is low!\n", sup->name);
		return -1;
	}

	return 0;
}

static inline int db_supply_control(const struct db_pwr_supply *sup, int enable)
{
	return ioex_set_level(sup->enable, enable);
}

/*
 * delay is the delay in milliseconds
 * supply_mask is the mask of supplies that should be enabled in this step
 */
struct db_seq_step {
	uint16_t delay;
	uint8_t supply_mask;
};

#define MAX_NUM_STEPS 8
struct db_pwr_seq {
	uint8_t valid;
	uint8_t nsteps;
	struct db_seq_step steps[MAX_NUM_STEPS];
};

static struct db_pwr_seq db0_seq;
static struct db_pwr_seq db1_seq;

static void db_pwr_seq_read(int eeprom, struct db_pwr_seq *seq)
{
	const struct usrp_eeprom_db_pwr_seq *eep;
	uint8_t i;

	seq->nsteps = 0;

	eep = eeprom_lookup_tag(eeprom, USRP_EEPROM_DB_PWR_SEQ_TAG);
	if (!eep)
		return;

	if (eep->nsteps > MAX_NUM_STEPS ||
	    eep->nsteps > ARRAY_SIZE(eep->steps)) {
		ccprintf("invalid number of db sequence steps! %u\n", eep->nsteps);
		return;
	}

	seq->nsteps = eep->nsteps;

	for (i = 0; i < seq->nsteps; i++) {
		seq->steps[i].delay = eep->steps[i].delay;
		seq->steps[i].supply_mask = eep->steps[i].supply_mask;
	}

	seq->valid = 1;
}

static inline int db_disable_rails(const struct db_pwr *db, uint8_t supply_mask,
				   uint16_t delay)
{
	uint8_t i;

	for (i = 0; i < SUPPLY_COUNT; i++) {
		if (supply_mask & BIT(i))
			db_supply_control(&db->supply[i], 0);
	}

	return 0;
}

static int db_power_good(const struct db_pwr *db, uint8_t supply_mask)
{
	uint8_t i;
	int rv;

	for (i = 0; i < SUPPLY_COUNT; i++) {
		if (supply_mask & BIT(i)) {
			rv = db_supply_check_status(&db->supply[i]);
			if (rv)
				return rv;
		}
	}

	return 0;
}

static int db_enable_rails(const struct db_pwr *db, uint8_t supply_mask,
			   uint16_t delay)
{
	uint8_t i;
	int rv;

	for (i = 0; i < SUPPLY_COUNT; i++) {
		if (supply_mask & BIT(i)) {
			rv = db_supply_control(&db->supply[i], 1);
			if (rv)
				goto err;
		}
	}

	if (delay)
		msleep(delay);

	rv = db_power_good(db, supply_mask);
	if (rv)
		goto err;

	return rv;
err:
	db_disable_rails(db, supply_mask, 0);
	return rv;
}

static int db_poweron(struct db_pwr *db, const struct db_pwr_seq *seq)
{
	uint8_t i, all_supply_mask = 0;
	int rv = 0;

	if (!seq->valid) {
		ccprintf("error: attempted to power on daughterboard without a valid sequence\n");
		return -1;
	}

	if (db->state == DB_PWR_STATE_ON)
		return 0;

	for (i = 0; i < seq->nsteps; i++) {
		all_supply_mask |= seq->steps[i].supply_mask;
		rv = db_enable_rails(db, seq->steps[i].supply_mask,
				     seq->steps[i].delay);
		if (rv)
			goto err;
	}

	rv = db_power_good(db, all_supply_mask);
	if (rv)
		goto err;

	db->state = DB_PWR_STATE_ON;

	rv = ioex_set_level(db->spi_oe_l, 0);
	if (rv)
		goto err;

	return rv;
err:
	db->state = DB_PWR_STATE_FAULT;
	db_disable_rails(db, all_supply_mask, 0);
	return rv;
}

static int db_poweroff(struct db_pwr *db, const struct db_pwr_seq *seq)
{
	uint8_t i = seq->nsteps;

	if (db->state == DB_PWR_STATE_OFF)
		return 0;

	ioex_set_level(db->spi_oe_l, 1);

	while (i-- > 0)
		db_disable_rails(db, seq->steps[i].supply_mask,
				 seq->steps[i].delay);

	db->state = DB_PWR_STATE_OFF;

	return 0;
}

static int db_pwr_show_seq(const struct db_pwr *db, const struct db_pwr_seq *seq)
{
	uint8_t i, j;

	if (!seq->valid) {
		ccprintf("no valid sequence loaded\n");
		return 0;
	}

	for (i = 0; i < seq->nsteps; i++) {
		ccprintf("step %u: delay: %u ms, rails: ", i,
			 seq->steps[i].delay);
		for (j = 0; j < SUPPLY_COUNT; j++) {
			if (seq->steps[i].supply_mask & BIT(j))
				ccprintf("%s ", db->supply[j].name);
		}
		ccprintf("\n");
	}

	return 0;
}

static int db_pwr_show_status(const struct db_pwr *db)
{
	int enable, status, rv;
	uint8_t i;

	ccprintf("supply is: %s\n", state_strs[db->state]);
	ccprintf("rail   \tstatus\tEN\tPG\n");
	ccprintf("-------\t------\t--\t--\n");
	for (i = 0; i < SUPPLY_COUNT; i++) {
		rv = __db_supply_status(&db->supply[i], &enable, &status);
		if (rv)
			goto out;
		ccprintf("%s\t%s\t%d\t%d\n", db->supply[i].name,
			 ((enable && !status) ? "ERR" : "ok"), enable, status);
	}

	return 0;
out:
	ccprintf("error %d when reading signal for %s\n", rv,
		 db->supply[i].name);
	return rv;
}

void db_pwr_init(void)
{
	db_pwr_seq_read(TLV_EEPROM_DB0, &db0_seq);
	db_pwr_seq_read(TLV_EEPROM_DB1, &db1_seq);
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
		 return db_pwr_show_seq(db, seq);
	else if (!strcasecmp(argv[2], "on"))
		return db_poweron(db, seq);
	else if (!strcasecmp(argv[2], "off"))
		return db_poweroff(db, seq);

	return EC_ERROR_PARAM2;
}
DECLARE_CONSOLE_COMMAND(dbpwr, command_dbpwr, "[0|1] [on|off|status|seq]",
			"control daughterboard power");

/*
 * Command intended for debugging purposes only. This may be disabled in the
 * shipping image
 */
static int command_dbpwrseq(int argc, char **argv)
{
	struct db_pwr_seq *seq;
	char *ep;
	int which, idx, val;
	uint16_t delay;
	uint8_t mask;

	if (argc < 5)
		return EC_ERROR_PARAM_COUNT;

	if (*argv[1] != '0' && *argv[1] != '1')
		return EC_ERROR_PARAM1;

	idx = strtoi(argv[2], &ep, 0);
	if (idx < 0 || idx > MAX_NUM_STEPS)
		return EC_ERROR_PARAM2;

	val = strtoi(argv[3], &ep, 0);
	if (val < 0 || val > 65535)
		return EC_ERROR_PARAM3;
	delay = (uint16_t)val;

	val = strtoi(argv[4], &ep, 0);
	if (val < 0 || val > 255)
		return EC_ERROR_PARAM4;
	mask = (uint8_t)val;

	which = *argv[1] - '0';
	seq = which ? &db1_seq : &db0_seq;

	if ((idx + 1) > seq->nsteps)
		seq->nsteps = idx + 1;
	seq->steps[idx].supply_mask = mask;
	seq->steps[idx].delay = delay;

	seq->valid = 1;

	return 0;
}
DECLARE_CONSOLE_COMMAND(dbpwrseq, command_dbpwrseq,
			"[0|1] [0-8] <delay> <supply mask>",
			"modify the power sequence");

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
