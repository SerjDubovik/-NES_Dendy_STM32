// Modified for STM32F407 from the original InfoNES source.
/*===================================================================*/
/*                                                                   */
/*  InfoNES_Mapper.cpp : InfoNES Mapper Function                     */
/*                                                                   */
/*  2000/05/16  InfoNES Project ( based on NesterJ and pNesX )       */
/*                                                                   */
/*===================================================================*/

/*-------------------------------------------------------------------*/
/*  Include files                                                    */
/*-------------------------------------------------------------------*/

#include "InfoNES.h"
#include "InfoNES_System.h"
#include "InfoNES_Mapper.h"
#include "K6502.h"
#include "nes_module.h"

#include "mapper/InfoNES_Mapper_000.cpp"

#if defined(INFONES_UXROM) || defined(INFONES_AXROM) || defined(INFONES_CNROM) || defined(INFONES_MMC1) || defined(INFONES_MMC3)
static unsigned Submapper()
{
    return (NesHeader.byInfo2 & 0x0C) == 8 ? NesHeader.byReserve[0] >> 4 : 0;
}
#endif

#ifdef INFONES_UXROM
bool InfoNES_ReadPrgBank(BYTE bank, BYTE *destination);
static BYTE selectedBank;

static void Map2_Write(WORD address, BYTE value)
{
    if (Submapper() == 2) value &= ROM[address - 0x8000U];
    const BYTE bank = value & (NesHeader.byRomSize - 1);
    if (bank != selectedBank && InfoNES_ReadPrgBank(bank, ROM)) selectedBank = bank;
}

static void Map2_Init()
{
    Map0_Init();
    MapperWrite = Map2_Write;
    selectedBank = 0;
}
#endif

#if defined(INFONES_MMC1) || defined(INFONES_CNROM) || defined(INFONES_MMC3) || defined(INFONES_AXROM)
bool InfoNES_ReadCart(bool chr, uint32_t offset, BYTE *destination, unsigned length);
#endif

#if defined(INFONES_AXROM) || defined(INFONES_MMC1) || defined(INFONES_MMC3)
// Four ways per set let code and data in different 32 KiB banks coexist.
static constexpr unsigned PageSize = 512;
static constexpr unsigned Sets = 16;
static constexpr unsigned Ways = 4;
static uint16_t pageTags[Sets][Ways];
static BYTE nextWay[Sets];

static unsigned lastPage;
static BYTE *lastData;

static BYTE ReadPrg(unsigned offset)
{
    const unsigned page = offset / PageSize;
    if (page == lastPage) return lastData[offset % PageSize];
    const unsigned set = (page ^ (page >> 4) ^ (page >> 8)) & (Sets - 1);
    for (unsigned way = 0; way < Ways; ++way) {
        if (pageTags[set][way] == page) {
            lastPage = page;
            lastData = ROM + (set * Ways + way) * PageSize;
            return lastData[offset % PageSize];
        }
    }

    const unsigned way = nextWay[set];
    BYTE *data = ROM + (set * Ways + way) * PageSize;
    if (!InfoNES_ReadCart(false, page * PageSize, data, PageSize)) return 0;
    pageTags[set][way] = page;
    nextWay[set] = (way + 1) % Ways;
    lastPage = page;
    lastData = data;
    return data[offset % PageSize];
}

static void InitPrgCache()
{
    lastPage = ~0U;
    InfoNES_MemorySet(pageTags, 0xFF, sizeof(pageTags));
    InfoNES_MemorySet(nextWay, 0, sizeof(nextWay));
}
#endif

#ifdef INFONES_AXROM
static BYTE activeBank;

BYTE InfoNES_ReadMappedPrg(WORD address)
{
    return ReadPrg(activeBank * 32768U + (address & 0x7FFF));
}

static void Map7_Write(WORD address, BYTE value)
{
    if (Submapper() == 2) value &= InfoNES_ReadMappedPrg(address);
    activeBank = (value & 7U) & (NesHeader.byRomSize / 2U - 1U);
    InfoNES_Mirroring((value & 0x10U) ? 2 : 3);
}

static void Map7_Init()
{
    Map0_Init();
    MapperWrite = Map7_Write;
    activeBank = 0;
    InitPrgCache();
    InfoNES_Mirroring(3);
}
#endif

#if defined(INFONES_CNROM) || defined(INFONES_MMC1) || defined(INFONES_MMC3)
static uint16_t chrPages[8];

static void SetChrPages(unsigned first, unsigned count, unsigned bank)
{
    const unsigned total = NesHeader.byVRomSize ? NesHeader.byVRomSize * 8U : 8U;
    bank &= total - 1;
    if (NesHeader.byVRomSize) {
        bool changed = false;
        for (unsigned i = 0; i < count; ++i) changed |= chrPages[first + i] != bank + i;
        if (!changed || !InfoNES_ReadCart(true, bank * 1024U, PPURAM + first * 1024U,
                                         count * 1024U)) return;
    }
    for (unsigned i = 0; i < count; ++i) {
        const unsigned slot = first + i;
        if (chrPages[slot] == bank + i) continue;
        chrPages[slot] = bank + i;
        PPUBANK[slot] = PPURAM + (NesHeader.byVRomSize ? slot : bank + i) * 1024U;
        ChrBufUpdate |= 1U << slot;
    }
}

static void InitChr()
{
    InfoNES_MemorySet(chrPages, 0xFF, sizeof(chrPages));
}
#endif

#ifdef INFONES_CNROM
static void Map3_Write(WORD address, BYTE value)
{
    if (Submapper() != 1) value &= ROM[address & (NesHeader.byRomSize * 16384U - 1U)];
    SetChrPages(0, 8, value * 8U);
}

static void Map3_Init()
{
    InfoNES_ReadCart(false, 0, ROM, NesHeader.byRomSize * 16384U);
    Map0_Init();
    InitChr();
    SetChrPages(0, 8, 0);
    MapperWrite = Map3_Write;
}
#endif

#if defined(INFONES_MMC1) || defined(INFONES_MMC3)
static uint16_t prgPages[4];
WORD PrgRamMask;
bool PrgRamRead;
bool PrgRamWrite;

static void SetPrgPages(unsigned first, unsigned count, unsigned bank)
{
    bank &= NesHeader.byRomSize * 2U - 1;
    for (unsigned i = 0; i < count; ++i) prgPages[first + i] = bank + i;
}

BYTE InfoNES_ReadMappedPrg(WORD address)
{
    return ReadPrg(prgPages[(address >> 13) & 3] * 8192U + (address & 0x1FFF));
}

static void InitBanks()
{
    Map0_Init();
    ROMBANK0 = ROM;
    ROMBANK1 = ROM + 8192;
    ROMBANK2 = ROM + 16384;
    ROMBANK3 = ROM + 24576;
    InitChr();
    InitPrgCache();
    const bool nes2 = (NesHeader.byInfo2 & 0x0C) == 8;
    const unsigned size = NesHeader.byReserve[2];
    const unsigned shift = size & 15 ? size & 15 : size >> 4;
    PrgRamMask = nes2 ? (shift ? (64U << shift) - 1 : 0) : 8191;
    if (ROM_Trainer && PrgRamMask == 0) PrgRamMask = 8191;
    PrgRamRead = PrgRamWrite = PrgRamMask != 0;
}
#endif

#ifdef INFONES_MMC1
static BYTE mmc1Shift, mmc1Control, mmc1Chr0, mmc1Chr1, mmc1Prg;

static void Map1_Update()
{
    const unsigned outer = NesHeader.byRomSize > 16 ? mmc1Chr0 & 0x10U : 0U;
    const unsigned selected = outer + (mmc1Prg & 15);
    const unsigned mode = (mmc1Control >> 2) & 3;
    if (Submapper() == 5) {
        SetPrgPages(0, 4, 0);
    } else if (mode < 2 && NesHeader.byRomSize > 1) {
        SetPrgPages(0, 4, (selected & ~1U) * 2U);
    } else {
        const unsigned last = outer + ((NesHeader.byRomSize - 1) & 15);
        SetPrgPages(0, 2, (mode == 2 ? outer : selected) * 2U);
        SetPrgPages(2, 2, (mode == 2 ? selected : last) * 2U);
    }
    if (mmc1Control & 0x10) {
        SetChrPages(0, 4, mmc1Chr0 * 4U);
        SetChrPages(4, 4, mmc1Chr1 * 4U);
    } else {
        SetChrPages(0, 8, (mmc1Chr0 & ~1U) * 4U);
    }
    constexpr BYTE mirror[] = {3, 2, 1, 0};
    InfoNES_Mirroring(mirror[mmc1Control & 3]);
    PrgRamRead = PrgRamWrite = PrgRamMask != 0 && !(mmc1Prg & 0x10);
}

static void Map1_Write(WORD address, BYTE value)
{
    if (value & 0x80) {
        mmc1Shift = 0x10;
        mmc1Control |= 0x0C;
        Map1_Update();
        return;
    }
    const bool commit = mmc1Shift & 1;
    mmc1Shift = (mmc1Shift >> 1) | ((value & 1) << 4);
    if (!commit) return;
    switch ((address >> 13) & 3) {
        case 0: mmc1Control = mmc1Shift; break;
        case 1: mmc1Chr0 = mmc1Shift; break;
        case 2: mmc1Chr1 = mmc1Shift; break;
        case 3: mmc1Prg = mmc1Shift; break;
    }
    mmc1Shift = 0x10;
    Map1_Update();
}

static void Map1_Init()
{
    InitBanks();
    mmc1Control = 0x0C;
    mmc1Shift = 0x10;
    mmc1Chr0 = mmc1Chr1 = mmc1Prg = 0;
    MapperWrite = Map1_Write;
    Map1_Update();
}
#endif

#ifdef INFONES_MMC3
static BYTE mmc3Select, mmc3Banks[8], irqLatch, irqCounter;
static bool irqEnabled, irqReload, a12High;
static uint32_t a12LowSince;
bool MapperIrqPending;

static void Map4_Update()
{
    const unsigned last = NesHeader.byRomSize * 2U - 1;
    const bool reverse = mmc3Select & 0x40;
    SetPrgPages(0, 1, reverse ? last - 1 : mmc3Banks[6] & 63);
    SetPrgPages(1, 1, mmc3Banks[7] & 63);
    SetPrgPages(2, 1, reverse ? mmc3Banks[6] & 63 : last - 1);
    SetPrgPages(3, 1, last);
    const unsigned swap = mmc3Select & 0x80 ? 4 : 0;
    SetChrPages(swap, 2, mmc3Banks[0] & 0xFE);
    SetChrPages(swap + 2, 2, mmc3Banks[1] & 0xFE);
    for (unsigned i = 0; i < 4; ++i) SetChrPages((swap ^ 4) + i, 1, mmc3Banks[2 + i]);
}

static void Map4_ClockIrq()
{
    const BYTE before = irqCounter;
    const bool reload = irqReload;
    irqCounter = (irqCounter == 0 || reload) ? irqLatch : irqCounter - 1;
    irqReload = false;
    // NEC's counter does not keep asserting an IRQ while it stays at zero.
    if (irqEnabled && irqCounter == 0 && (Submapper() != 4 || before != 0 || reload))
        MapperIrqPending = true;
}

void InfoNES_Mmc3A12(WORD address, uint32_t cpuCycle)
{
    const bool high = address & 0x1000;
    if (high && !a12High && cpuCycle - a12LowSince >= 3) Map4_ClockIrq();
    if (!high && a12High) a12LowSince = cpuCycle;
    a12High = high;
}

void InfoNES_Mmc3Scanline(bool preRender)
{
    if (!(PPU_R1 & (R1_SHOW_SCR | R1_SHOW_SP))) return;
    const bool backgroundHigh = PPU_R0 & R0_BG_ADDR;
    bool spriteHigh = PPU_R0 & R0_SP_ADDR;
    if (PPU_SP_Height == 16) {
        spriteHigh = false;
        unsigned count = 0;
        if (!preRender) {
            for (unsigned i = 0; i < 64 && count < 8; ++i) {
                const BYTE *sprite = SPRRAM + i * 4;
                if ((unsigned)(PPU_Scanline - sprite[SPR_Y]) >= 16) continue;
                spriteHigh |= (sprite[SPR_CHR] & 1) != 0;
                ++count;
            }
        }
        // Empty sprite slots fetch tile $FF in 8x16 mode.
        spriteHigh |= count < 8;
    }
    if (backgroundHigh != spriteHigh) Map4_ClockIrq();
    a12High = backgroundHigh;
}

static void Map4_Write(WORD address, BYTE value)
{
    switch (address & 0xE001) {
        case 0x8000:
            if ((mmc3Select ^ value) & 0xC0) {
                mmc3Select = value;
                Map4_Update();
            } else {
                mmc3Select = value;
            }
            break;
        case 0x8001:
            mmc3Banks[mmc3Select & 7] = value;
            Map4_Update();
            break;
        case 0xA000:
            if (!ROM_FourScr && Submapper() != 2) InfoNES_Mirroring((value & 1) ? 0 : 1);
            break;
        case 0xA001:
            PrgRamRead = PrgRamMask != 0 && (value & 0x80);
            PrgRamWrite = PrgRamRead && !(value & 0x40);
            break;
        case 0xC000: irqLatch = value; break;
        case 0xC001: irqReload = true; break;
        case 0xE000: irqEnabled = MapperIrqPending = false; break;
        case 0xE001: irqEnabled = true; break;
    }
}

static void Map4_Init()
{
    InitBanks();
    mmc3Select = irqLatch = irqCounter = 0;
    irqEnabled = irqReload = MapperIrqPending = a12High = false;
    a12LowSince = 0;
    InfoNES_MemorySet(mmc3Banks, 0, sizeof(mmc3Banks));
    mmc3Banks[7] = 1;
    MapperWrite = Map4_Write;
    Map4_Update();
}
#endif

struct MapperTable_tag MapperTable[] =
{
#if defined(INFONES_MMC1)
    {1, Map1_Init},
#elif defined(INFONES_UXROM)
    {2, Map2_Init},
#elif defined(INFONES_CNROM)
    {3, Map3_Init},
#elif defined(INFONES_MMC3)
    {4, Map4_Init},
#elif defined(INFONES_AXROM)
    {7, Map7_Init},
#else
    {0, Map0_Init},
#endif
    {-1, nullptr}
};
