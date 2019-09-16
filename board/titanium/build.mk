# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Board specific files build

# the IC is STmicro STM32F412VEH6
CHIP:=stm32
CHIP_FAMILY:=stm32f4
CHIP_VARIANT:=stm32f412

board-y := board.o
board-y += power.o
board-y += tlv_eeprom.o
board-y += eeproms.o
board-y += db_pwr.o
board-y += led.o
