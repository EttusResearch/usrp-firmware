/* Copyright 2011 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Zynqmp functions for Chrome EC */

#ifndef __CROS_EC_ZYNQMP_H
#define __CROS_EC_ZYNQMP_H

/**
 * convert the bootmode from integer to string
 */
const char *zynqmp_bootmode_to_str(int bm);

/**
 * convert the bootmode from string to integer
 */
int zynqmp_str_to_bootmode(const char *boot_mode);

#endif  /* __CROS_EC_ZYNQMP_H */
