/**
  * @file    w25q64.h
  * @brief   Minimal OSPI driver for the Winbond W25Q64 (8 MB) NOR flash on the
  *          h723-mini board (OCTOSPI1 port 1: PF8 IO0 / PF9 IO1 / PF7 IO2 /
  *          PF6 IO3 / PF10 CLK / PG6 NCS).
  *
  * Ported from the h750-mini QUADSPI driver to the H723 OCTOSPI HAL, keeping
  * the same interface:
  *   - exposes the read *and* write data-line width as a parameter, so 1/2/4
  *     line reads and 1/4 line page-programs can be benchmarked side by side;
  *   - returns hard error codes instead of silently swallowing HAL errors;
  *   - documents that W25Q64 has NO 2-line write command (page program exists
  *     only in 1-1-1 (0x02) and 1-1-4 (0x32) flavours);
  *   - provides a memory-mapped mode entry/exit pair used for the XIP /
  *     code-space-mapping benchmarks.
  */

#ifndef W25Q64_H
#define W25Q64_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define W25Q64_MEM_BASE     0x90000000UL   /* OCTOSPI1 memory-mapped base      */
#define W25Q64_PAGE_SIZE    256u
#define W25Q64_SECTOR_SIZE  4096u          /* 4K erase sector                */
#define W25Q64_BLOCK32_SIZE (32u * 1024u)  /* 32K erase block                */
#define W25Q64_BLOCK64_SIZE (64u * 1024u)  /* 64K erase block                */
#define W25Q64_FLASH_SIZE   0x800000UL     /* 8 MiB (W25Q64)                 */
#define W25Q64_JEDEC_ID     0xEF4017u      /* Winbond W25Q64                 */

/* Read transfer shapes (command-address-data). Address size is always 24 bit. */
typedef enum {
    W25Q64_READ_1_1_1 = 0,  /* 0x03 Read Data                  no dummy   */
    W25Q64_READ_1_1_2,      /* 0x3B Fast Read Dual Output      8 dummy    */
    W25Q64_READ_1_2_2,      /* 0xBB Fast Read Dual I/O         4 dummy    */
    W25Q64_READ_1_1_4,      /* 0x6B Fast Read Quad Output      8 dummy    */
    W25Q64_READ_1_4_4,      /* 0xEB Fast Read Quad I/O         6 dummy    */
    W25Q64_READ_MODES
} w25q64_read_mode_t;

/* Write (page program) shapes. W25Q64 has NO dual (2-line) page program. */
typedef enum {
    W25Q64_WRITE_1_1_1 = 0, /* 0x02 Page Program              1-1-1       */
    W25Q64_WRITE_1_1_4,     /* 0x32 Quad Input Page Program   1-1-4       */
    W25Q64_WRITE_MODES
} w25q64_write_mode_t;

/* Return codes */
#define W25Q64_OK                0
#define W25Q64_ERR_PARAM        -1
#define W25Q64_ERR_WRITE_ENABLE -2
#define W25Q64_ERR_AUTOPOLL     -3
#define W25Q64_ERR_ERASE        -4
#define W25Q64_ERR_COMMAND      -5
#define W25Q64_ERR_TRANSMIT     -6
#define W25Q64_ERR_RECEIVE      -7
#define W25Q64_ERR_MEMMAP       -8
#define W25Q64_ERR_CLOCK        -9
#define W25Q64_ERR_INIT         -10

int      w25q64_init(void);                      /* clocks + HAL + JEDEC ID  */
int      w25q64_reset_flash(void);               /* 0x66/0x99 full reset      */
uint32_t w25q64_read_id(void);                   /* 24-bit JEDEC ID          */
uint8_t  w25q64_read_sr1(void);                  /* Status Register 1 (0x05) */
uint8_t  w25q64_read_sr2(void);                  /* Status Register 2 (0x35) */

int w25q64_erase_sector(uint32_t addr);          /* 4 KB   */
int w25q64_erase_block32(uint32_t addr);         /* 32 KB  */
int w25q64_erase_block64(uint32_t addr);         /* 64 KB  */

int w25q64_read(uint32_t addr, void *buf, uint32_t len, w25q64_read_mode_t m);
int w25q64_write(uint32_t addr, const void *buf, uint32_t len,
                 w25q64_write_mode_t m);

int  w25q64_memmap_start(w25q64_read_mode_t m);  /* enter 0x90000000 mapping */
void w25q64_memmap_stop(void);                   /* back to command mode    */

const char *w25q64_read_mode_name(w25q64_read_mode_t m);
const char *w25q64_write_mode_name(w25q64_write_mode_t m);

#ifdef __cplusplus
}
#endif

#endif /* W25Q64_H */
