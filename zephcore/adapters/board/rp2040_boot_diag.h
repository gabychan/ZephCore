/*
 * SPDX-License-Identifier: MIT
 *
 * RP2040 bring-up diagnostic: stage markers on the board LED while there is no
 * console. Each stage blinks its own number after a 1 s dark gap. Off unless
 * CONFIG_ZEPHCORE_RP2040_BOOT_DIAG=y.
 *
 *   1  kernel devices initialised (PRE_KERNEL_2)
 *   2  kernel running (APPLICATION init, before main)
 *   3  server_main entered
 *   4  USB init OK     (failure: 10 fast blinks, then |error| slow blinks)
 *   5  RepeaterDataStore begin done (after the first-boot format, if any)
 *   6  identity loaded or generated
 *   7  about to enter the event loop
 */

#ifndef ZEPHCORE_RP2040_BOOT_DIAG_H
#define ZEPHCORE_RP2040_BOOT_DIAG_H

#ifdef CONFIG_ZEPHCORE_RP2040_BOOT_DIAG

#ifdef __cplusplus
extern "C" {
#endif

void rp2040_diag_blink(int n);
void rp2040_diag_result(int rc);

#ifdef __cplusplus
}
#endif

#define ZC_DIAG_BLINK(n)	rp2040_diag_blink(n)
#define ZC_DIAG_RESULT(expr)	rp2040_diag_result(expr)

#else

#define ZC_DIAG_BLINK(n)	((void)0)
#define ZC_DIAG_RESULT(expr)	((void)(expr))

#endif

#endif /* ZEPHCORE_RP2040_BOOT_DIAG_H */
