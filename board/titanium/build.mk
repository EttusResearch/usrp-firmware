# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Board specific files build

#
# Build with BOARD=titanium for Rev 6 and later
# or BOARD=titanium-rev5 for Rev 5

CHIP:=stm32
CHIP_FAMILY:=stm32f4

# the IC is STmicro STM32F412VEH6 for Rev 5 and earlier,
# STM32F411VEH6 for Rev 6 and later
ifeq ($(BOARD),titanium)
	CHIP_VARIANT:=stm32f411
endif

ifeq ($(BOARD),titanium-rev5)
	CHIP_VARIANT:=stm32f412
endif

board-y := board.o
board-y += power.o
board-y += tlv_eeprom.o
board-y += eeproms.o
board-y += db_pwr.o
board-y += led.o
board-$(CONFIG_FANS) += fan.o
board-y += mcu_flags.o
board-y += thermal.o
