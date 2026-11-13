#ifndef NES_GAME_H
#define NES_GAME_H

#include <stdbool.h>
#include <stdint.h>
#include "ff.h"

typedef struct {
    uint32_t fps_times_10;
    uint32_t frame_ms_times_10;
    uint32_t dma_errors;
} NesGameTiming;

void NesGame_Start(const TCHAR *rom_path);
void NesGame_Poll(void);
void NesGame_Stop(void);
bool NesGame_UsesSdStream(void);
const char *NesGame_Status(void);
NesGameTiming NesGame_GetTiming(void);

#endif
