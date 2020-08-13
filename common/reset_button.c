
#include "reset_button.h"
#include "button.h"
#include "chipset.h"
#include "gpio.h"
#include "hooks.h"
#include "timer.h"

static int debounced_reset_pressed;
static enum gpio_signal reset_gpio = GPIO_RESET_BUTTON_L;

static int reset_button_pressed(void)
{
	return !gpio_get_level(reset_gpio);
}

static void reset_button_init(void)
{
	debounced_reset_pressed = reset_button_pressed();
	gpio_enable_interrupt(reset_gpio);
}
DECLARE_HOOK(HOOK_INIT, reset_button_init, HOOK_PRIO_INIT_POWER_BUTTON);

static void reset_button_change_deferred(void)
{
	int pressed = reset_button_pressed();

	if (pressed == debounced_reset_pressed)
		return;

	debounced_reset_pressed = pressed;

	if (pressed) {
		ccprintf("Issuing reset..\n");
		chipset_reset(CHIPSET_RESET_KB_SYSRESET);
	}
}
DECLARE_DEFERRED(reset_button_change_deferred);

void reset_button_interrupt(enum gpio_signal signal)
{
	hook_call_deferred(&reset_button_change_deferred_data,
			   BUTTON_DEBOUNCE_US);
}
