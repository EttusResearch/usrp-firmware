#ifndef __CROS_EC_RESET_BUTTON_H
#define __CROS_EC_RESET_BUTTON_H

#include "common.h"

void reset_button_interrupt(enum gpio_signal signal);

#endif
