/* Copyright (c) 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * TI TCA6416 I/O expander
 */

#ifndef __CROS_EC_IOEXPANDER_TCA6416_H
#define __CROS_EC_IOEXPANDER_TCA6416_H

/* ADDR pin selects either address 0x20 or 0x21 */
#define TCA6416_I2C_ADDR(addr_pin) (0x20 | ((addr_pin) & 1))
#define TCA6408_I2C_ADDR(addr_pin) (0x20 | ((addr_pin) & 1))

extern const struct ioexpander_drv tca6408_ioexpander_drv;
extern const struct ioexpander_drv tca6416_ioexpander_drv;

#endif  /* __CROS_EC_IOEXPANDER_TCA6416_H */
