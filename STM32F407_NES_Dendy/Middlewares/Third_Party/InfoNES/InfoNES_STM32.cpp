#include "nes_module.h"
#include "InfoNES.h"
#include "InfoNES_System.h"

namespace {
const NesModuleApi *board;
const uint8_t *romHeader;
uint8_t *prgData;
uint8_t *chrData;
const uint8_t *trainerData;

constexpr WORD Rgb555To565(WORD color)
{
    return ((color & 0x7FE0U) << 1) | ((color & 0x0200U) >> 4) | (color & 0x001FU);
}
}

// InfoNES Linux palette, converted from RGB555 to RGB565.
#define RGB(color) Rgb555To565(color)
const WORD NesPalette[64] = {
    RGB(0x39ce), RGB(0x1071), RGB(0x0015), RGB(0x2013), RGB(0x440e), RGB(0x5402), RGB(0x5000), RGB(0x3c20),
    RGB(0x20a0), RGB(0x0100), RGB(0x0140), RGB(0x00e2), RGB(0x0ceb), 0, 0, 0,
    RGB(0x5ef7), RGB(0x01dd), RGB(0x10fd), RGB(0x401e), RGB(0x5c17), RGB(0x700b), RGB(0x6ca0), RGB(0x6521),
    RGB(0x45c0), RGB(0x0240), RGB(0x02a0), RGB(0x0247), RGB(0x0211), 0, 0, 0,
    RGB(0x7fff), RGB(0x1eff), RGB(0x2e5f), RGB(0x223f), RGB(0x79ff), RGB(0x7dd6), RGB(0x7dcc), RGB(0x7e67),
    RGB(0x7ae7), RGB(0x4342), RGB(0x2769), RGB(0x2ff3), RGB(0x03bb), 0, 0, 0,
    RGB(0x7fff), RGB(0x579f), RGB(0x635f), RGB(0x6b3f), RGB(0x7f1f), RGB(0x7f1b), RGB(0x7ef6), RGB(0x7f75),
    RGB(0x7f94), RGB(0x73f4), RGB(0x57d7), RGB(0x5bf9), RGB(0x4ffe), 0, 0, 0
};
#undef RGB


int InfoNES_ReadRom(const char *)
{
    InfoNES_MemoryCopy(&NesHeader, romHeader, sizeof(NesHeader));
    InfoNES_MemorySet(SRAM, 0, SRAM_SIZE);
    if (trainerData) InfoNES_MemoryCopy(SRAM + 0x1000, trainerData, 512);
    ROM = prgData;
    VROM = chrData;
    return 0;
}

void InfoNES_ReleaseRom()
{
    ROM = nullptr;
    VROM = nullptr;
}

void InfoNES_DrawScanline(WORD line, const WORD *pixels)
{
    board->draw_scanline(line, pixels);
}

void InfoNES_PadState(DWORD *pad1, DWORD *pad2, DWORD *system)
{
    *pad1 = board->pad_state();
    *pad2 = 0;
    *system = 0;
}

void *InfoNES_MemoryCopy(void *dest, const void *src, int count)
{
    auto *out = static_cast<uint8_t *>(dest);
    const auto *in = static_cast<const uint8_t *>(src);
    for (int i = 0; i < count; ++i) out[i] = in[i];
    return dest;
}

void *InfoNES_MemorySet(void *dest, int value, int count)
{
    auto *out = static_cast<uint8_t *>(dest);
    for (int i = 0; i < count; ++i) out[i] = static_cast<uint8_t>(value);
    return dest;
}

void InfoNES_MessageBox(const char *, ...) {}

extern "C" int NesModule_Init(const NesModuleApi *api, const uint8_t *header,
                               uint8_t *prg, uint8_t *chr, const uint8_t *trainer)
{
    board = api;
    romHeader = header;
    prgData = prg;
    chrData = chr;
    trainerData = trainer;
    InfoNES_Init();
    const int result = InfoNES_Load(nullptr);
    romHeader = nullptr;
    trainerData = nullptr;
    return result;
}

extern "C" void NesModule_RunFrame()
{
    InfoNES_RunFrame();
}
