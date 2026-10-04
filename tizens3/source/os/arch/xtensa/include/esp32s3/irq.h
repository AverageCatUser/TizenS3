/******************************************************************
 *
 * Copyright 2019 Samsung Electronics All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ******************************************************************/

/****************************************************************************
 * arch/xtensa/include/esp32/irq.h
 *
 *   Copyright (C) 2016 Gregory Nutt. All rights reserved.
 *   Author: Gregory Nutt <gnutt@nuttx.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name NuttX nor the names of its contributors may be
 *    used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ****************************************************************************/

/* This file should never be included directed but, rather, only indirectly
 * through tinyara/irq.h
 */

#ifndef __ARCH_XTENSA_INCLUDE_ESP32S3_IRQ_H
#define __ARCH_XTENSA_INCLUDE_ESP32S3_IRQ_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <arch/esp32s3/chip.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Interrupt Matrix
 *
 * The Interrupt Matrix embedded in the ESP32 independently allocates
 * peripheral interrupt sources to the two CPUs’ peripheral interrupts. This
 * configuration is highly flexible in order to meet many different needs.
 *
 * Features
 * - Accepts 71 peripheral interrupt sources as input.
 * - Generates 26 peripheral interrupt sources per CPU as output (52 total).
 * - CPU NMI Interrupt Mask.
 * - Queries current interrupt status of peripheral interrupt sources.
 *
 * Peripheral Interrupt Source
 *
 * ESP32 has 71 peripheral interrupt sources in total. 67 of 71 ESP32
 * peripheral interrupt sources can be allocated to either CPU.  The four
 * remaining peripheral interrupt sources are CPU-specific, two per CPU.
 *
 * - GPIO_INTERRUPT_PRO and GPIO_INTERRUPT_PRO_NMI can only be allocated to
 *   PRO_CPU.
 * - GPIO_INTERRUPT_APP and GPIO_INTERRUPT_APP_NMI can only be allocated to
 *   APP_CPU.
 *
 * As a result, PRO_CPU and APP_CPU each have 69 peripheral interrupt
 * sources.
 */

/* PRO_INTR_STATUS_REG_0 / APP_INTR_STATUS_REG_0 */

/* ESP32-S3 peripheral interrupt sources (ETS_*_INTR_SOURCE in ESP-IDF
 * soc/esp32s3/include/soc/periph_defs.h). Only the ones this port uses are
 * named; the interrupt matrix has 99 sources in total.
 */

#define ESP32_PERIPH_MAC            0
#define ESP32_PERIPH_GPIO           16
#define ESP32_PERIPH_UART           27   /* UART0 */
#define ESP32_PERIPH_UART1          28
#define ESP32_PERIPH_UART2          29
#define ESP32_PERIPH_TG_T0_LEVEL    50
#define ESP32_PERIPH_SYSTIMER_T0    57
#define ESP32_PERIPH_CPU_CPU0       79   /* FROM_CPU_INTR0 */
#define ESP32_PERIPH_CPU_CPU1       80
#define ESP32_PERIPH_CPU_CPU2       81
#define ESP32_PERIPH_CPU_CPU3       82
#define ESP32_PERIPH_USB_SERIAL_JTAG 96

#define ESP32_NPERIPHERALS          99

/* Exceptions
 *
 * IRAM Offset  Description
 *   0x0000     Windows
 *   0x0180     Level 2 interrupt
 *   0x01c0     Level 3 interrupt
 *   0x0200     Level 4 interrupt
 *   0x0240     Level 5 interrupt
 *   0x0280     Debug exception
 *   0x02c0     NMI exception
 *   0x0300     Kernel exception
 *   0x0340     User exception
 *   0x03c0     Double exception
 *
 * REVISIT: In more architectures supported by NuttX, exception errors
 * tie into the normal interrupt handling via special IRQ numbers.  I
 * is still to be determined what will be done for the ESP32.
 */

/* IRQ numbers for internal interrupts that are dispatched like peripheral
 * interrupts
 */

#define XTENSA_IRQ_TIMER0           0	/* INTERRUPT, bit 6 */
#define XTENSA_IRQ_TIMER1           1	/* INTERRUPT, bit 15 */
#define XTENSA_IRQ_TIMER2           2	/* INTERRUPT, bit 16 */
#define XTENSA_IRQ_SYSCALL          3	/* User interrupt w/EXCCAUSE=syscall */

#define XTENSA_NIRQ_INTERNAL        4	/* Number of dispatch internal interrupts */
#define XTENSA_IRQ_FIRSTPERIPH      4	/* First peripheral IRQ number */

/* IRQ numbers for peripheral interrupts coming throught the Interrupt
 * Matrix.
 */

#define ESP32_IRQ2PERIPH(irq)       ((irq) - XTENSA_IRQ_FIRSTPERIPH)

#define ESP32_IRQ_MAC               (XTENSA_IRQ_FIRSTPERIPH + ESP32_PERIPH_MAC)
#define ESP32_IRQ_UART              (XTENSA_IRQ_FIRSTPERIPH + ESP32_PERIPH_UART)
#define ESP32_IRQ_USB_SERIAL_JTAG   (XTENSA_IRQ_FIRSTPERIPH + ESP32_PERIPH_USB_SERIAL_JTAG)
#define ESP32_IRQ_CPU_CPU0          (XTENSA_IRQ_FIRSTPERIPH + ESP32_PERIPH_CPU_CPU0)
#define ESP32_IRQ_CPU_CPU1          (XTENSA_IRQ_FIRSTPERIPH + ESP32_PERIPH_CPU_CPU1)

#define ESP32_NIRQ_PERIPH           ESP32_NPERIPHERALS

/* Second level GPIO interrupts.  GPIO interrupts are decoded and dispatched as
 * a second level of decoding:  The first level dispatches to the GPIO interrupt
 * handler.  The second to the decoded GPIO interrupt handler.
 */

#ifdef CONFIG_ESP32_GPIO_IRQ
#define ESP32_NIRQ_GPIO           40
#define ESP32_FIRST_GPIOIRQ       (XTENSA_NIRQ_INTERNAL + ESP32_NIRQ_PERIPH)
#define ESP32_LAST_GPIOIRQ        (ESP32_FIRST_GPIOIRQ + ESP32_NIRQ_GPIO-1)
#define ESP32_PIN2IRQ(p)          ((p) + ESP32_FIRST_GPIOIRQ)
#define ESP32_IRQ2PIN(i)          ((i) - ESP32_FIRST_GPIOIRQ)
#else
#define ESP32_NIRQ_GPIO           0
#endif

/* Total number of interrupts */

#define NR_IRQS                     (XTENSA_NIRQ_INTERNAL + ESP32_NIRQ_PERIPH + ESP32_NIRQ_GPIO)

/* Xtensa CPU Interrupts.
 *
 * Each of the two CPUs (PRO and APP) have 32 interrupts each, of which
 * 26 can be mapped to peripheral interrupts:
 *
 *   Level triggered peripherals (21 total):
 *     0-5, 8-9, 12-13, 17-18 - Priority 1
 *     19-21                  - Priority 2
 *     23, 27                 - Priority 3
 *     24-25                  - Priority 4
 *     26, 31                 - Priority 5
 *   Edge triggered peripherals (4 total):
 *     10                     - Priority 1
 *     22                     - Priority 3
 *     28, 30                 - Priority 4
 *   NMI (1 total):
 *     14                     - NMI
 *
 * CPU peripheral interrupts can be a assigned to a CPU interrupt using the
 * PRO_*_MAP_REG or APP_*_MAP_REG.  There are a pair of these registers for
 * each peripheral source.  Multiple peripheral interrupt sources can be
 * mapped to the same.
 *
 * The remaining, five, internal CPU interrupts are:
 *
 *   6   Timer0    - Priority 1
 *   7   Software  - Priority 1
 *   11  Profiling - Priority 3
 *   15  Timer1    - Priority 3
 *   16  Timer2    - Priority 5
 *   29  Software  - Priority 3
 *
 * A peripheral interrupt can be disabled
 */

#define ESP32_CPUINT_LEVELPERIPH_0  0
#define ESP32_CPUINT_LEVELPERIPH_1  1
#define ESP32_CPUINT_LEVELPERIPH_2  2
#define ESP32_CPUINT_LEVELPERIPH_3  3
#define ESP32_CPUINT_LEVELPERIPH_4  4
#define ESP32_CPUINT_LEVELPERIPH_5  5
#define ESP32_CPUINT_LEVELPERIPH_6  8
#define ESP32_CPUINT_LEVELPERIPH_7  9
#define ESP32_CPUINT_LEVELPERIPH_8  12
#define ESP32_CPUINT_LEVELPERIPH_9  13
#define ESP32_CPUINT_LEVELPERIPH_10 17
#define ESP32_CPUINT_LEVELPERIPH_11 18
#define ESP32_CPUINT_LEVELPERIPH_12 19
#define ESP32_CPUINT_LEVELPERIPH_13 20
#define ESP32_CPUINT_LEVELPERIPH_14 21
#define ESP32_CPUINT_LEVELPERIPH_15 23
#define ESP32_CPUINT_LEVELPERIPH_16 24
#define ESP32_CPUINT_LEVELPERIPH_17 25
#define ESP32_CPUINT_LEVELPERIPH_18 26
#define ESP32_CPUINT_LEVELPERIPH_19 27
#define ESP32_CPUINT_LEVELPERIPH_20 31

#define ESP32_CPUINT_NLEVELPERIPHS  21
#define EPS32_CPUINT_LEVELSET       0x8fbe333f

#define ESP32_CPUINT_EDGEPERIPH_0   10
#define ESP32_CPUINT_EDGEPERIPH_1   22
#define ESP32_CPUINT_EDGEPERIPH_2   28
#define ESP32_CPUINT_EDGEPERIPH_3   30

#define ESP32_CPUINT_NEDGEPERIPHS   4
#define EPS32_CPUINT_EDGESET        0x50400400

#define ESP32_CPUINT_NNMIPERIPHS    1
#define EPS32_CPUINT_NMISET         0x00004000

#define ESP32_CPUINT_TIMER0         6
#define ESP32_CPUINT_SOFTWARE0      7
#define ESP32_CPUINT_PROFILING      11
#define ESP32_CPUINT_TIMER1         15
#define ESP32_CPUINT_TIMER2         16
#define ESP32_CPUINT_SOFTWARE1      29
#define ESP32_CPUINT_WIFI           0

#define ESP32_CPUINT_NINTERNAL      6

#define ESP32_NCPUINTS              32
#define ESP32_CPUINT_MAX            (ESP32_NCPUINTS - 1)
#define EPS32_CPUINT_PERIPHSET      0xdffe773e
#define EPS32_CPUINT_INTERNALSET    0x200188c0

/* Priority 1:   0-10, 12-13, 17-18    (15)
 * Priority 2:   19-21                 (3)
 * Priority 3:   11, 15, 22-23, 27, 29 (6)
 * Priority 4:   24-25, 28, 30         (4)
 * Priority 5:   16, 26, 31            (3)
 * Priority NMI: 14                    (1)
 */

#define ESP32_INTPRI1_MASK          0x000637ff
#define ESP32_INTPRI2_MASK          0x00380000
#define ESP32_INTPRI3_MASK          0x28c08800
#define ESP32_INTPRI4_MASK          0x53000000
#define ESP32_INTPRI5_MASK          0x84010000
#define ESP32_INTNMI_MASK           0x00004000

/****************************************************************************
 * Public Types
 ****************************************************************************/

#ifndef __ASSEMBLY__

/****************************************************************************
 * Inline functions
 ****************************************************************************/

/****************************************************************************
 * Public Data
 ****************************************************************************/

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

#ifdef __cplusplus
#define EXTERN extern "C"
extern "C" {
#else
#define EXTERN extern
#endif

#undef EXTERN
#ifdef __cplusplus
}
#endif

#endif							/* __ASSEMBLY__ */
#endif							/* __ARCH_XTENSA_INCLUDE_ESP32S3_IRQ_H */
