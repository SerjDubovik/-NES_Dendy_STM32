#ifndef NES_MODULE_H
#define NES_MODULE_H

#include <stdbool.h>
#include <stdint.h>

#define NES_MODULE_ADDRESS  0x20010000U
#define NES_MODULE_CAPACITY (63U * 1024U)
/* Last 1 KiB of the reserved SRAM is used by the resident LCD DMA. */
#define NES_LCD_DMA_ADDRESS (NES_MODULE_ADDRESS + NES_MODULE_CAPACITY)
#define NES_MODULE_MAGIC    0x3153454EU
#define NES_MODULE_ABI      1U
#define NES_MODULE_UXROM_ABI 2U
#define NES_MODULE_AXROM_ABI 7U
#define NES_MODULE_MMC1_ABI 4U
#define NES_MODULE_CNROM_ABI 5U
#define NES_MODULE_MMC3_ABI 6U
#define NES_PRG_BANK_BYTES (16U * 1024U)

static inline unsigned NesRomTiming(const uint8_t *header)
{
    return (header[7] & 0x0CU) == 8U ? header[12] & 3U : header[9] & 1U;
}

typedef struct {
    void (*draw_scanline)(uint16_t line, const uint16_t *pixels);
    uint8_t (*pad_state)(void);
    bool (*read_prg_bank)(uint8_t bank, uint8_t *destination);
    bool (*read_rom)(bool chr, uint32_t offset, uint8_t *destination, uint32_t length);
    void (*load_save)(uint8_t *ram);
} NesModuleApi;

/* Written by nes_module.ld at the start of the image. The crc32 field
 * is treated as zero when calculating CRC32 over image_size bytes. */
typedef struct {
    uint32_t magic;
    uint32_t abi;
    uint32_t image_size;
    uint32_t bss_offset;
    uint32_t bss_size;
    uint32_t crc32;
    int (*init)(const NesModuleApi *api, const uint8_t *header,
                uint8_t *prg, uint8_t *chr, const uint8_t *trainer);
    void (*run_frame)(void);
} NesModuleHeader;

#ifdef __cplusplus
extern "C" {
#endif

int NesModule_Init(const NesModuleApi *api, const uint8_t *header,
                   uint8_t *prg, uint8_t *chr, const uint8_t *trainer);
void NesModule_RunFrame(void);

#ifdef __cplusplus
}
#endif

#endif
