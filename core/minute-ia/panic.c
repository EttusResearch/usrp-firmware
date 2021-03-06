/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "console.h"
#include "cpu.h"
#include "host_command.h"
#include "panic.h"
#include "printf.h"
#include "system.h"
#include "task.h"
#include "timer.h"
#include "util.h"
#include "watchdog.h"

/* Whether bus fault is ignored */
static int bus_fault_ignored;

/*
 * Print panic data
 */
void panic_data_print(const struct panic_data *pdata)
{
}

void __keep report_panic(void)
{
}

/**
 * Default exception handler, which reports a panic.
 *
 * Declare this as a naked call so we can extract raw LR and IPSR values.
 */
void __keep exception_panic(void);
void exception_panic(void)
{
}

#ifdef CONFIG_SOFTWARE_PANIC
void software_panic(uint32_t reason, uint32_t info)
{
	/* TODO: store panic log */
	while (1)
		;
}

void panic_set_reason(uint32_t reason, uint32_t info, uint8_t exception)
{
}

void panic_get_reason(uint32_t *reason, uint32_t *info, uint8_t *exception)
{
}
#endif

void bus_fault_handler(void)
{
	if (!bus_fault_ignored)
		exception_panic();
}

void ignore_bus_fault(int ignored)
{
	bus_fault_ignored = ignored;
}
