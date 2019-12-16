#ifndef __CROS_EC_HOST_CONTROL_GPIO_H
#define __CROS_EC_HOST_CONTROL_GPIO_H

struct host_control_gpio {
	const char *name;
	int signal;
};

extern const struct host_control_gpio host_control_gpios[];

#endif

