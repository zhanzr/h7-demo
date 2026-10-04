/* qspi_xip.S - position-independent function used to demonstrate executing
 * code from the QSPI memory-mapped space (XIP).
 *
 * Contract:  uint32_t w25q64_xip_fn(uint32_t seed) -> seed*0x1234 + 0x5678
 *
 * The function uses only registers and immediates (movw/mul/add) - no
 * PC-relative literal loads, no global data - so it is fully position
 * independent and yields identical results whether executed from its linked
 * location in internal flash or from 0x90000000 after the bytes are copied
 * into the W25Q64 and the QSPI is switched to memory-mapped mode.
 */

    .syntax unified
    .cpu   cortex-m7
    .thumb

    .section .qspi_code,"ax",%progbits
    .align 4
    .global w25q64_xip_fn
    .thumb_func
    .type   w25q64_xip_fn, %function
w25q64_xip_fn:
    movw    r2, #0x1234
    mul     r0, r2, r0          /* r0 = seed * 0x1234 */
    movw    r2, #0x5678
    add     r0, r0, r2          /* r0 = seed * 0x1234 + 0x5678 */
    bx      lr
    .size   w25q64_xip_fn, .-w25q64_xip_fn

    .end
