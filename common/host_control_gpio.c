#include "host_control_gpio.h"
#include "host_command.h"
#include "gpio.h"
#include "ioexpander.h"
#include <string.h>
#include "assert.h"

static enum ec_status host_gpio_query(struct host_cmd_handler_args *args)
{
	const struct ec_params_host_gpio_query *p = args->params;
	struct ec_response_host_gpio_query *r = args->response;
	const struct host_control_gpio *gpio;
	int val;

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

		if (signal_is_gpio(gpio->signal)) {
			r->get_info.flags = gpio_get_default_flags(gpio->signal);
		} else if (signal_is_ioex(gpio->signal)) {
			ioex_get_flags(gpio->signal, &val);
			r->get_info.flags = val;
		} else {
			assert(0);
		}

		args->response_size = sizeof(r->get_info);
		break;
	case EC_HOST_GPIO_GET_STATE:
		if (signal_is_gpio(gpio->signal)) {
			r->get_state.val = gpio_get_level(gpio->signal);
		} else if (signal_is_ioex(gpio->signal)) {
			ioex_get_level(gpio->signal, &val);
			r->get_state.val = val;
		} else {
			assert(0);
		}

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

static enum ec_status host_gpio_set(struct host_cmd_handler_args *args)
{
	const struct ec_params_host_gpio_set *p = args->params;
	const struct host_control_gpio *gpio;

	if (p->index >= HOST_CONTROL_GPIO_COUNT)
		return EC_RES_INVALID_PARAM;

	gpio = &host_control_gpios[p->index];

	if (signal_is_gpio(gpio->signal))
		gpio_set_level(gpio->signal, p->val);
	else if (signal_is_ioex(gpio->signal))
		ioex_set_level(gpio->signal, p->val);
	else
		assert(0);

	return EC_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_HOST_GPIO_SET, host_gpio_set, EC_VER_MASK(0));
