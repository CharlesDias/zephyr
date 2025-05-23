/*
 * Copyright (c) 2024 Silicon Laboratories Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * This file was generated by the script gen_clock_control.py in the hal_silabs module.
 * Do not manually edit.
 */

#ifndef ZEPHYR_INCLUDE_DT_BINDINGS_CLOCK_SILABS_XG29_CLOCK_H_
#define ZEPHYR_INCLUDE_DT_BINDINGS_CLOCK_SILABS_XG29_CLOCK_H_

#include <zephyr/dt-bindings/dt-util.h>
#include "common-clock.h"

/*
 * DT macros for clock tree nodes.
 * Defined as:
 *  0..5 - Bit within CLKEN register
 *  6..8 - CLKEN register number
 * Must stay in sync with equivalent SL_BUS_*_VALUE constants in the Silicon Labs HAL to be
 * interpreted correctly by the clock control driver.
 */
#define CLOCK_ACMP0         (FIELD_PREP(CLOCK_REG_MASK, 1) | FIELD_PREP(CLOCK_BIT_MASK, 19))
#define CLOCK_AGC           (FIELD_PREP(CLOCK_REG_MASK, 1) | FIELD_PREP(CLOCK_BIT_MASK, 0))
#define CLOCK_AMUXCP0       (FIELD_PREP(CLOCK_REG_MASK, 0) | FIELD_PREP(CLOCK_BIT_MASK, 11))
#define CLOCK_BUFC          (FIELD_PREP(CLOCK_REG_MASK, 1) | FIELD_PREP(CLOCK_BIT_MASK, 11))
#define CLOCK_BURAM         (FIELD_PREP(CLOCK_REG_MASK, 0) | FIELD_PREP(CLOCK_BIT_MASK, 28))
#define CLOCK_BURTC         (FIELD_PREP(CLOCK_REG_MASK, 0) | FIELD_PREP(CLOCK_BIT_MASK, 29))
#define CLOCK_SEMAILBOX     (FIELD_PREP(CLOCK_REG_MASK, 1) | FIELD_PREP(CLOCK_BIT_MASK, 21))
#define CLOCK_DMEM          (FIELD_PREP(CLOCK_REG_MASK, 1) | FIELD_PREP(CLOCK_BIT_MASK, 22))
#define CLOCK_DCDC          (FIELD_PREP(CLOCK_REG_MASK, 0) | FIELD_PREP(CLOCK_BIT_MASK, 31))
#define CLOCK_DPLL0         (FIELD_PREP(CLOCK_REG_MASK, 0) | FIELD_PREP(CLOCK_BIT_MASK, 17))
#define CLOCK_ETAMPDET      (FIELD_PREP(CLOCK_REG_MASK, 1) | FIELD_PREP(CLOCK_BIT_MASK, 28))
#define CLOCK_EUSART0       (FIELD_PREP(CLOCK_REG_MASK, 1) | FIELD_PREP(CLOCK_BIT_MASK, 20))
#define CLOCK_EUSART1       (FIELD_PREP(CLOCK_REG_MASK, 1) | FIELD_PREP(CLOCK_BIT_MASK, 23))
#define CLOCK_FRC           (FIELD_PREP(CLOCK_REG_MASK, 1) | FIELD_PREP(CLOCK_BIT_MASK, 3))
#define CLOCK_FSRCO         (FIELD_PREP(CLOCK_REG_MASK, 0) | FIELD_PREP(CLOCK_BIT_MASK, 20))
#define CLOCK_GPCRC0        (FIELD_PREP(CLOCK_REG_MASK, 0) | FIELD_PREP(CLOCK_BIT_MASK, 3))
#define CLOCK_GPIO          (FIELD_PREP(CLOCK_REG_MASK, 0) | FIELD_PREP(CLOCK_BIT_MASK, 26))
#define CLOCK_HFRCO0        (FIELD_PREP(CLOCK_REG_MASK, 0) | FIELD_PREP(CLOCK_BIT_MASK, 18))
#define CLOCK_HFXO0         (FIELD_PREP(CLOCK_REG_MASK, 0) | FIELD_PREP(CLOCK_BIT_MASK, 19))
#define CLOCK_I2C0          (FIELD_PREP(CLOCK_REG_MASK, 0) | FIELD_PREP(CLOCK_BIT_MASK, 14))
#define CLOCK_I2C1          (FIELD_PREP(CLOCK_REG_MASK, 0) | FIELD_PREP(CLOCK_BIT_MASK, 15))
#define CLOCK_IADC0         (FIELD_PREP(CLOCK_REG_MASK, 0) | FIELD_PREP(CLOCK_BIT_MASK, 10))
#define CLOCK_ICACHE0       (FIELD_PREP(CLOCK_REG_MASK, 1) | FIELD_PREP(CLOCK_BIT_MASK, 16))
#define CLOCK_IFADCDEBUG    (FIELD_PREP(CLOCK_REG_MASK, 1) | FIELD_PREP(CLOCK_BIT_MASK, 12))
#define CLOCK_LDMA0         (FIELD_PREP(CLOCK_REG_MASK, 0) | FIELD_PREP(CLOCK_BIT_MASK, 0))
#define CLOCK_LDMAXBAR0     (FIELD_PREP(CLOCK_REG_MASK, 0) | FIELD_PREP(CLOCK_BIT_MASK, 1))
#define CLOCK_LETIMER0      (FIELD_PREP(CLOCK_REG_MASK, 0) | FIELD_PREP(CLOCK_BIT_MASK, 12))
#define CLOCK_LFRCO         (FIELD_PREP(CLOCK_REG_MASK, 0) | FIELD_PREP(CLOCK_BIT_MASK, 21))
#define CLOCK_LFXO          (FIELD_PREP(CLOCK_REG_MASK, 0) | FIELD_PREP(CLOCK_BIT_MASK, 22))
#define CLOCK_MODEM         (FIELD_PREP(CLOCK_REG_MASK, 1) | FIELD_PREP(CLOCK_BIT_MASK, 1))
#define CLOCK_MSC           (FIELD_PREP(CLOCK_REG_MASK, 1) | FIELD_PREP(CLOCK_BIT_MASK, 17))
#define CLOCK_PDM           (FIELD_PREP(CLOCK_REG_MASK, 0) | FIELD_PREP(CLOCK_BIT_MASK, 25))
#define CLOCK_PRORTC        (FIELD_PREP(CLOCK_REG_MASK, 1) | FIELD_PREP(CLOCK_BIT_MASK, 10))
#define CLOCK_PROTIMER      (FIELD_PREP(CLOCK_REG_MASK, 1) | FIELD_PREP(CLOCK_BIT_MASK, 4))
#define CLOCK_PRS           (FIELD_PREP(CLOCK_REG_MASK, 0) | FIELD_PREP(CLOCK_BIT_MASK, 27))
#define CLOCK_RAC           (FIELD_PREP(CLOCK_REG_MASK, 1) | FIELD_PREP(CLOCK_BIT_MASK, 5))
#define CLOCK_RADIOAES      (FIELD_PREP(CLOCK_REG_MASK, 0) | FIELD_PREP(CLOCK_BIT_MASK, 2))
#define CLOCK_RDMAILBOX0    (FIELD_PREP(CLOCK_REG_MASK, 1) | FIELD_PREP(CLOCK_BIT_MASK, 8))
#define CLOCK_RDMAILBOX1    (FIELD_PREP(CLOCK_REG_MASK, 1) | FIELD_PREP(CLOCK_BIT_MASK, 9))
#define CLOCK_RDSCRATCHPAD  (FIELD_PREP(CLOCK_REG_MASK, 1) | FIELD_PREP(CLOCK_BIT_MASK, 7))
#define CLOCK_RFCRC         (FIELD_PREP(CLOCK_REG_MASK, 1) | FIELD_PREP(CLOCK_BIT_MASK, 2))
#define CLOCK_RFSENSE       (FIELD_PREP(CLOCK_REG_MASK, 1) | FIELD_PREP(CLOCK_BIT_MASK, 14))
#define CLOCK_RTCC          (FIELD_PREP(CLOCK_REG_MASK, 0) | FIELD_PREP(CLOCK_BIT_MASK, 30))
#define CLOCK_SMU           (FIELD_PREP(CLOCK_REG_MASK, 1) | FIELD_PREP(CLOCK_BIT_MASK, 15))
#define CLOCK_SYNTH         (FIELD_PREP(CLOCK_REG_MASK, 1) | FIELD_PREP(CLOCK_BIT_MASK, 6))
#define CLOCK_SYSCFG        (FIELD_PREP(CLOCK_REG_MASK, 0) | FIELD_PREP(CLOCK_BIT_MASK, 16))
#define CLOCK_TIMER0        (FIELD_PREP(CLOCK_REG_MASK, 0) | FIELD_PREP(CLOCK_BIT_MASK, 4))
#define CLOCK_TIMER1        (FIELD_PREP(CLOCK_REG_MASK, 0) | FIELD_PREP(CLOCK_BIT_MASK, 5))
#define CLOCK_TIMER2        (FIELD_PREP(CLOCK_REG_MASK, 0) | FIELD_PREP(CLOCK_BIT_MASK, 6))
#define CLOCK_TIMER3        (FIELD_PREP(CLOCK_REG_MASK, 0) | FIELD_PREP(CLOCK_BIT_MASK, 7))
#define CLOCK_TIMER4        (FIELD_PREP(CLOCK_REG_MASK, 1) | FIELD_PREP(CLOCK_BIT_MASK, 18))
#define CLOCK_ULFRCO        (FIELD_PREP(CLOCK_REG_MASK, 0) | FIELD_PREP(CLOCK_BIT_MASK, 23))
#define CLOCK_USART0        (FIELD_PREP(CLOCK_REG_MASK, 0) | FIELD_PREP(CLOCK_BIT_MASK, 8))
#define CLOCK_USART1        (FIELD_PREP(CLOCK_REG_MASK, 0) | FIELD_PREP(CLOCK_BIT_MASK, 9))
#define CLOCK_WDOG0         (FIELD_PREP(CLOCK_REG_MASK, 0) | FIELD_PREP(CLOCK_BIT_MASK, 13))

#endif /* ZEPHYR_INCLUDE_DT_BINDINGS_CLOCK_SILABS_XG29_CLOCK_H_ */
