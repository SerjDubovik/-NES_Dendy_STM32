#ifndef NES_GAME_H
#define NES_GAME_H

#include <stdint.h>

typedef struct {
    uint32_t fps_times_10;
    uint32_t frame_ms_times_10;
    uint32_t dma_errors;
} NesGameTiming;

void NesGame_Init(void);
void NesGame_Poll(void);
const char *NesGame_Status(void);
NesGameTiming NesGame_GetTiming(void);

#endif
