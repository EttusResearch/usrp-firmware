#include "host_control_gpio.h"
#include "host_command.h"
#include "gpio.h"
#include "ioexpander.h"
#include <string.h>
#include "assert.h"

static int host_control_gpio_get(const struct host_control_gpio *g)
{
	int val;

	if (g->get)
		return g->get(g->signal);

	if (signal_is_gpio(g->signal))
		return gpio_get_level(g->signal);

	if (signal_is_ioex(g->signal)) {
		ioex_get_level(g->signal, &val);
		return val;
	}

	assert(0);
	return 0;
}

static void host_control_gpio_set(const struct host_control_gpio *g, int value)
{
	if (g->set)
		return g->set(g->signal, value);
	else if (signal_is_gpio(g->signal))
		gpio_set_level(g->signal, value);
	else if (signal_is_ioex(g->signal))
		ioex_set_level(g->signal, value);
	else
		assert(0);
}

static int host_control_gpio_get_flags(const struct host_control_gpio *g)
{
	int val;

	if (g->set && g->get)
		assert(0);

	if (g->set)
		return GPIO_OUTPUT;
	if (g->get)
		return GPIO_INPUT;

	if (signal_is_gpio(g->signal))
		return gpio_get_default_flags(g->signal);

	if (signal_is_ioex(g->signal)) {
		ioex_get_flags(g->signal, &val);
		return val;
	}

	assert(0);
	return 0;
}

static enum ec_status host_gpio_query(struct host_cmd_handler_args *args)
{
	const struct ec_params_host_gpio_query *p = args->params;
	struct ec_response_host_gpio_query *r = args->response;
	const struct host_control_gpio *gpio;

	if (p->subcmd == EC_HOST_GPIO_GET_COUNT) {
		r->get_count.val = HOST_CONTROL_GPIO_COUNT;
		args->response_size = sizeof(r->get_count.val);
		return EC_SUCCESS;
	}

	if (p->index >= HOST_CONTROL_GPIO_COUNT)
		return EC_RES_INVALID_PARAM;

	gpio = &host_control_gpios[p->index];

	switch (p->subcmd) {
	case EC_HOST_GPIO_GET_INFO:
		memcpy(r->get_info.name, gpio->name, sizeof(r->get_info.name));
		r->get_info.name[sizeof(r->get_info.name) - 1] = 0;
		r->get_info.flags = host_control_gpio_get_flags(gpio);
		args->response_size = sizeof(r->get_info);
		break;
	case EC_HOST_GPIO_GET_STATE:
		r->get_state.val = host_control_gpio_get(gpio);
		args->response_size = sizeof(r->get_info);
		break;
	case EC_HOST_GPIO_GET_COUNT:
		/* handled above, should be unreachable */
		assert(0);
		break;
	}

	return EC_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_HOST_GPIO_QUERY, host_gpio_query, EC_VER_MASK(0));
DECLARE_PRIVATE_HOST_COMMAND(EC_CMD_HOST_GPIO_QUERY_PRIVATE, host_gpio_query,
			     EC_VER_MASK(0));

static enum ec_status host_gpio_set(struct host_cmd_handler_args *args)
{
	const struct ec_params_host_gpio_set *p = args->params;
	const struct host_control_gpio *gpio;

	if (p->index >= HOST_CONTROL_GPIO_COUNT)
		return EC_RES_INVALID_PARAM;

	gpio = &host_control_gpios[p->index];

	host_control_gpio_set(gpio, p->val);

	return EC_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_HOST_GPIO_SET, host_gpio_set, EC_VER_MASK(0));
DECLARE_PRIVATE_HOST_COMMAND(EC_CMD_HOST_GPIO_SET_PRIVATE, host_gpio_set,
			     EC_VER_MASK(0));
