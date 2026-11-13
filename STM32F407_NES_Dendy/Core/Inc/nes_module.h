#ifndef NES_MODULE_H
#define NES_MODULE_H

#include <stdint.h>

#define NES_MODULE_ADDRESS  0x20010000U
#define NES_MODULE_CAPACITY (64U * 1024U)
#define NES_MODULE_MAGIC    0x3153454EU
#define NES_MODULE_ABI      1U

typedef struct {
    void (*draw_scanline)(uint16_t line, const uint16_t *pixels);
    uint8_t (*pad_state)(void);
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
