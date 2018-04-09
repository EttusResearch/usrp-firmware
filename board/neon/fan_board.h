#ifndef CAPTURE_H
#define CAPTURE_H

#include "common.h"
#include "gpio.h"

void fan_tach_interrupt(enum gpio_signal sig);

#endif /* CAPTURE_H */
