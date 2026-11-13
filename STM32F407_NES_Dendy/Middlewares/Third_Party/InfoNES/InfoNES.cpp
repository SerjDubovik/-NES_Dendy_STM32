// Modified for STM32F407 from the original InfoNES source.
/*===================================================================*/
/*                                                                   */
/*  InfoNES.cpp : NES Emulator for Win32, Linux(x86), Linux(PS2)     */
/*                                                                   */
/*  2000/05/18  InfoNES Project ( based on pNesX )                   */
/*                                                                   */
/*===================================================================*/

/*-------------------------------------------------------------------
 * File List :
 *
 * [NES Hardware]
 *   InfoNES.cpp
 *   InfoNES.h
 *   K6502_rw.h
 *
 * [Mapper function]
 *   InfoNES_Mapper.cpp
 *   InfoNES_Mapper.h
 *
 * [The function which depends on a system]
 *   InfoNES_System_ooo.cpp (ooo is a system name. win, ...)
 *   InfoNES_System.h
 *
 * [CPU]
 *   K6502.cpp
 *   K6502.h
 *
 * [Others]
 *   InfoNES_Types.h
 *
 --------------------------------------------------------------------*/

/*-------------------------------------------------------------------*/
/*  Include files                                                    */
/*-------------------------------------------------------------------*/

#include "InfoNES.h"
#include "InfoNES_System.h"
#include "InfoNES_Mapper.h"
#include "K6502.h"
#include "nes_module.h"

/*-------------------------------------------------------------------*/
/*  NES resources                                                    */
/*-------------------------------------------------------------------*/

/* RAM */
BYTE RAM[ RAM_SIZE ];

/* SRAM */
BYTE SRAM[ SRAM_SIZE ] __attribute__((section(".nes_ccm"), aligned(4)));

/* ROM */
BYTE *ROM;

/* SRAM BANK ( 8Kb ) */
BYTE *SRAMBANK;

/* ROM BANK ( 8Kb * 4 ) */
BYTE *ROMBANK0;
BYTE *ROMBANK1;
BYTE *ROMBANK2;
BYTE *ROMBANK3;

/*-------------------------------------------------------------------*/
/*  PPU resources                                                    */
/*-------------------------------------------------------------------*/

/* PPU RAM */
BYTE PPURAM[ PPURAM_SIZE ] __attribute__((section(".nes_ccm"), aligned(4)));

/* VROM */
BYTE *VROM;

/* PPU BANK ( 1Kb * 16 ) */
BYTE *PPUBANK[ 16 ];

/* Sprite RAM */
BYTE SPRRAM[ SPRRAM_SIZE ];

/* PPU Register */
BYTE PPU_R0;
BYTE PPU_R1;
BYTE PPU_R2;
BYTE PPU_R3;
BYTE PPU_R7;
BYTE PPU_OpenBus;

/* Fine X scroll (the other scroll bits are stored in the VRAM addresses). */
BYTE PPU_FineX;

/* PPU Address */
WORD PPU_Addr;

/* PPU Address */
WORD PPU_Temp;

/* The increase value of the PPU Address */
WORD PPU_Increment;

/* Current Scanline */
WORD PPU_Scanline;

/* Remaining PPU dots from the preceding scanline. */
static unsigned PpuPhase;

/* BG Base Address */
BYTE *PPU_BG_Base;

/* Sprite Base Address */
BYTE *PPU_SP_Base;

/* Sprite Height */
WORD PPU_SP_Height;

/* VRAM Write Enable ( 0: Disable, 1: Enable ) */
BYTE byVramWriteEnable;

/* PPU Address and Scroll Latch Flag*/
BYTE PPU_Latch_Flag;

/* Frame IRQ ( 0: Disabled, 1: Enabled )*/
BYTE FrameIRQ_Enable;
WORD FrameStep;

/*-------------------------------------------------------------------*/
/*  Display and Others resouces                                      */
/*-------------------------------------------------------------------*/

/* Frame Skip */
WORD FrameSkip;
WORD FrameCnt;

/* Palette indices with a transparency flag, converted to RGB565 before output. */
alignas(4) static WORD Scanline[ NES_DISP_WIDTH ];

/* Character Buffer */
BYTE ChrBuf[ 256 * 2 * 8 * 8 ];

/* Update flag for ChrBuf */
BYTE ChrBufUpdate;

/* Palette Table */
WORD PalTable[ 32 ];

/* Table for Mirroring */
BYTE PPU_MirrorTable[][ 4 ] =
{
  { NAME_TABLE0, NAME_TABLE0, NAME_TABLE1, NAME_TABLE1 },
  { NAME_TABLE0, NAME_TABLE1, NAME_TABLE0, NAME_TABLE1 },
  { NAME_TABLE1, NAME_TABLE1, NAME_TABLE1, NAME_TABLE1 },
  { NAME_TABLE0, NAME_TABLE0, NAME_TABLE0, NAME_TABLE0 },
  { NAME_TABLE0, NAME_TABLE1, NAME_TABLE2, NAME_TABLE3 },
  { NAME_TABLE0, NAME_TABLE0, NAME_TABLE0, NAME_TABLE1 }
};

/*-------------------------------------------------------------------*/
/*  APU and Pad resources                                            */
/*-------------------------------------------------------------------*/

/* APU Register */
BYTE APU_Reg[ 0x18 ];

/* APU Mute ( 0:OFF, 1:ON ) */

/* Pad data */
DWORD PAD1_Latch;
DWORD PAD2_Latch;
DWORD PAD_System;
DWORD PAD1_Bit;
DWORD PAD2_Bit;

/*-------------------------------------------------------------------*/
/*  Mapper Function                                                  */
/*-------------------------------------------------------------------*/

/* Initialize Mapper */
void (*MapperInit)();
/* Write to Mapper */
void (*MapperWrite)( WORD wAddr, BYTE byData );
/* Write to SRAM */
void (*MapperSram)( WORD wAddr, BYTE byData );
/* Write to Apu */
void (*MapperApu)( WORD wAddr, BYTE byData );
/* Read from Apu */
BYTE (*MapperReadApu)( WORD wAddr );
/* Callback at VSync */
void (*MapperVSync)();
/* Callback at HSync */
void (*MapperHSync)();
/* Callback at PPU read/write */
void (*MapperPPU)( WORD wAddr );
/* Callback at Rendering Screen 1:BG, 0:Sprite */
void (*MapperRenderScreen)( BYTE byMode );

/*-------------------------------------------------------------------*/
/*  ROM information                                                  */
/*-------------------------------------------------------------------*/

/* .nes File Header */
struct NesHeader_tag NesHeader;

/* Mapper Number */
BYTE MapperNo;

/* Mirroring 0:Horizontal 1:Vertical */
BYTE ROM_Mirroring;
/* It has SRAM */
BYTE ROM_SRAM;
/* It has Trainer */
BYTE ROM_Trainer;
/* Four screen VRAM  */
BYTE ROM_FourScr;

/*===================================================================*/
/*                                                                   */
/*                InfoNES_Init() : Initialize InfoNES                */
/*                                                                   */
/*===================================================================*/
void InfoNES_Init()
{
/*
 *  Initialize InfoNES
 *
 *  Remarks
 *    Initialize K6502.
 */
  K6502_Init();
}

/*===================================================================*/
/*                                                                   */
/*                InfoNES_Fin() : Completion treatment               */
/*                                                                   */
/*===================================================================*/
void InfoNES_Fin()
{
/*
 *  Completion treatment
 *
 *  Remarks
 *    Release resources
 */
  // Release a memory for ROM
  InfoNES_ReleaseRom();
}

/*===================================================================*/
/*                                                                   */
/*                  InfoNES_Load() : Load a cassette                 */
/*                                                                   */
/*===================================================================*/
int InfoNES_Load( const char *pszFileName )
{
/*
 *  Load a cassette
 *
 *  Parameters
 *    const char *pszFileName            (Read)
 *      File name of ROM image
 *
 *  Return values
 *     0 : It was finished normally.
 *    -1 : An error occurred.
 *
 *  Remarks
 *    Read a ROM image in the memory. 
 *    Reset InfoNES.
 */

  // Release a memory for ROM
  InfoNES_ReleaseRom();

  // Read a ROM image in the memory
  if ( InfoNES_ReadRom( pszFileName ) < 0 )
    return -1;

  // Reset InfoNES
  if ( InfoNES_Reset() < 0 )
  {
    InfoNES_ReleaseRom();
    return -1;
  }

  // Successful
  return 0;
}

/*===================================================================*/
/*                                                                   */
/*                 InfoNES_Reset() : Reset InfoNES                   */
/*                                                                   */
/*===================================================================*/
int InfoNES_Reset()
{
/*
 *  Reset InfoNES
 *
 *  Return values
 *     0 : Normally
 *    -1 : Non support mapper
 *
 *  Remarks
 *    Initialize Resources, PPU and Mapper.
 *    Reset CPU.
 */

  int nIdx;

  /*-------------------------------------------------------------------*/
  /*  Get information on the cassette                                  */
  /*-------------------------------------------------------------------*/

  // Get Mapper Number
  MapperNo = NesHeader.byInfo1 >> 4;

  // Check bit counts of Mapper No.
  for ( nIdx = 4; nIdx < 8 && NesHeader.byReserve[ nIdx ] == 0; ++nIdx )
    ;

  if ( nIdx == 8 )
  {
    // Mapper Number is 8bits
    MapperNo |= ( NesHeader.byInfo2 & 0xf0 );
  }

  // Get information on the ROM
  ROM_Mirroring = NesHeader.byInfo1 & 1;
  ROM_SRAM = NesHeader.byInfo1 & 2;
  ROM_Trainer = NesHeader.byInfo1 & 4;
  ROM_FourScr = NesHeader.byInfo1 & 8;

  /*-------------------------------------------------------------------*/
  /*  Initialize resources                                             */
  /*-------------------------------------------------------------------*/

  // Clear RAM
  InfoNES_MemorySet( RAM, 0, sizeof RAM );

  // Reset frame skip and frame count
  FrameSkip = 0;
  FrameCnt = 0;

  // Reset update flag of ChrBuf
  ChrBufUpdate = 0xff;

  // Palette entries at multiples of four are transparent during compositing.
  for ( nIdx = 0; nIdx < 32; ++nIdx )
    PalTable[ nIdx ] = ( nIdx & 3 ) ? 0 : 0x8000;

  // Reset APU register
  InfoNES_MemorySet( APU_Reg, 0, sizeof APU_Reg );

  // Reset joypad
  PAD1_Latch = PAD2_Latch = PAD_System = 0;
  PAD1_Bit = PAD2_Bit = 0;

  /*-------------------------------------------------------------------*/
  /*  Initialize PPU                                                   */
  /*-------------------------------------------------------------------*/

  InfoNES_SetupPPU();

  /*-------------------------------------------------------------------*/
  /*  Initialize Mapper                                                */
  /*-------------------------------------------------------------------*/

  // Get Mapper Table Index
  for ( nIdx = 0; MapperTable[ nIdx ].nMapperNo != -1; ++nIdx )
  {
    if ( MapperTable[ nIdx ].nMapperNo == MapperNo )
      break;
  }

  if ( MapperTable[ nIdx ].nMapperNo == -1 )
  {
    // Non support mapper
    InfoNES_MessageBox( "Mapper #%d is unsupported.\n", MapperNo );
    return -1;
  }

  // Set up a mapper initialization function
  MapperTable[ nIdx ].pMapperInit();

  /*-------------------------------------------------------------------*/
  /*  Reset CPU                                                        */
  /*-------------------------------------------------------------------*/

  K6502_Reset();

  // Successful
  return 0;
}

/*===================================================================*/
/*                                                                   */
/*                InfoNES_SetupPPU() : Initialize PPU                */
/*                                                                   */
/*===================================================================*/
void InfoNES_SetupPPU()
{
/*
 *  Initialize PPU
 *
 */
  int nPage;

  // Clear PPU and Sprite Memory
  InfoNES_MemorySet( PPURAM, 0, sizeof PPURAM );
  InfoNES_MemorySet( SPRRAM, 0, sizeof SPRRAM );

  // Reset PPU Register
  PPU_R0 = PPU_R1 = PPU_R2 = PPU_R3 = PPU_R7 = PPU_OpenBus = 0;

  // Reset latch flag
  PPU_Latch_Flag = 0;

  FrameStep = 0;
  FrameIRQ_Enable = 0;

  PPU_FineX = 0;
  PPU_Addr = 0;
  PPU_Temp = 0;
  PPU_Scanline = 0;
  PpuPhase = 0;

  // Reset information on PPU_R0
  PPU_Increment = 1;
  PPU_BG_Base = ChrBuf;
  PPU_SP_Base = ChrBuf;
  PPU_SP_Height = 8;

  // Reset PPU banks
  for ( nPage = 0; nPage < 16; ++nPage )
    PPUBANK[ nPage ] = &PPURAM[ nPage * 0x400 ];

  /* Mirroring of Name Table */
  InfoNES_Mirroring( ROM_FourScr ? 4 : ROM_Mirroring );

  /* Reset VRAM Write Enable */
  byVramWriteEnable = ( NesHeader.byVRomSize == 0 ) ? 1 : 0;
}

/*===================================================================*/
/*                                                                   */
/*       InfoNES_Mirroring() : Set up a Mirroring of Name Table      */
/*                                                                   */
/*===================================================================*/
void InfoNES_Mirroring( int nType )
{
/*
 *  Set up a Mirroring of Name Table
 *
 *  Parameters
 *    int nType          (Read)
 *      Mirroring Type
 *        0 : Horizontal
 *        1 : Vertical
 *        2 : One Screen 0x2400
 *        3 : One Screen 0x2000
 *        4 : Four Screen
 *        5 : Special for Mapper #233
 */

  PPUBANK[ NAME_TABLE0 ] = &PPURAM[ PPU_MirrorTable[ nType ][ 0 ] * 0x400 ];
  PPUBANK[ NAME_TABLE1 ] = &PPURAM[ PPU_MirrorTable[ nType ][ 1 ] * 0x400 ];
  PPUBANK[ NAME_TABLE2 ] = &PPURAM[ PPU_MirrorTable[ nType ][ 2 ] * 0x400 ];
  PPUBANK[ NAME_TABLE3 ] = &PPURAM[ PPU_MirrorTable[ nType ][ 3 ] * 0x400 ];
  for (unsigned page = 8; page < 12; ++page)
    PPUBANK[page + 4] = PPUBANK[page];
}

/*===================================================================*/
/*                                                                   */
/*              InfoNES_RunFrame() : The loop of emulation              */
/*                                                                   */
/*===================================================================*/
static void InfoNES_DrawLine();

static const BYTE *SpritePattern(const BYTE *oam, unsigned row)
{
  const unsigned tile = oam[ SPR_CHR ];
  const unsigned height = PPU_SP_Height;
  const unsigned y = ( oam[ SPR_ATTR ] & SPR_ATTR_V_FLIP ) ? height - row - 1 : row;
  const BYTE *pattern = ( height == 16 )
      ? ChrBuf + ( tile & 1 ) * 256 * 64 + ( tile & 0xfe ) * 64
      : PPU_SP_Base + tile * 64;
  return pattern + y * 8;
}

/* Find the first opaque sprite-zero pixel that overlaps an opaque background
 * pixel. The result is sampled at scanline granularity. */
static int SpriteZeroHitX()
{
  if ( ( PPU_R2 & R2_HIT_SP ) ||
       ( PPU_R1 & ( R1_SHOW_SCR | R1_SHOW_SP ) ) != ( R1_SHOW_SCR | R1_SHOW_SP ) )
    return -1;

  const unsigned row = (unsigned)( PPU_Scanline - SPRRAM[ SPR_Y ] - 1 );
  if ( row >= PPU_SP_Height )
    return -1;

  const BYTE *pattern = SpritePattern( SPRRAM, row );
  const unsigned first = SPRRAM[ SPR_X ];
  const unsigned flip = ( SPRRAM[ SPR_ATTR ] & SPR_ATTR_H_FLIP ) ? 7 : 0;
  const bool left = ( PPU_R1 & ( R1_CLIP_BG | R1_CLIP_SP ) ) == ( R1_CLIP_BG | R1_CLIP_SP );
  const unsigned rowOffset = ( ( PPU_Addr >> 5 ) & 31 ) * 32;
  const unsigned fineY = ( ( PPU_Addr >> 12 ) & 7 ) * 8;
  for ( unsigned col = 0; col < 8 && first + col < NES_DISP_WIDTH - 1; ++col )
  {
    const unsigned x = first + col;
    if ( ( x < 8 && !left ) || !pattern[ col ^ flip ] )
      continue;
    const unsigned tileX = ( ( x + PPU_FineX ) >> 3 ) + ( PPU_Addr & 31 );
    const unsigned page = NAME_TABLE0 + ( ( ( PPU_Addr >> 10 ) & 3 ) ^ ( tileX >> 5 ) );
    const unsigned tile = PPUBANK[ page ][ rowOffset + ( tileX & 31 ) ];
    if ( PPU_BG_Base[ tile * 64 + fineY + ( ( x + PPU_FineX ) & 7 ) ] )
      return (int)x;
  }
#ifdef INFONES_AXROM
  // Battletoads & Double Dragon places its split-screen marker at x=254.
  // Its late rendering start can put the scanline renderer's background
  // up to one tile row out of phase with the sprite. Check nearby rows for
  // the marker pixel before the sprite leaves the current scanline range.
  if (first == 254 && row + 2 == PPU_SP_Height && pattern[flip]) {
    const unsigned tileX = ((first + PPU_FineX) >> 3) + (PPU_Addr & 31);
    const unsigned table = ((PPU_Addr >> 10) & 3) ^ (tileX >> 5);
    const unsigned fineX = (first + PPU_FineX) & 7;
    const int y = ((PPU_Addr >> 5) & 31) * 8 + fineY / 8;
    // The normal overlap test above already checked the current row.
    for (int delta = -8; delta <= 8; ++delta) {
      const int sample = y + delta;
      if (delta == 0 || sample < 0 || sample >= 240) continue;
      const BYTE tile = PPUBANK[NAME_TABLE0 + table][(sample / 8) * 32 + (tileX & 31)];
      if (PPU_BG_Base[tile * 64 + (sample % 8) * 8 + fineX])
        return (int)first;
    }
  }
#endif
  return -1;
}

/* Increment the vertical component of the current VRAM address at dot 256. */
static void AdvanceVerticalScroll()
{
  if ( ( PPU_Addr & 0x7000 ) != 0x7000 )
  {
    PPU_Addr += 0x1000;
    return;
  }

  unsigned row = ( PPU_Addr >> 5 ) & 31;
  PPU_Addr &= ~0x73e0;
  if ( row == 29 )
    PPU_Addr ^= 0x0800;
  else if ( row != 31 )
    PPU_Addr |= ( row + 1 ) << 5;
}

void InfoNES_RunFrame()
{
  // Three PPU dots per CPU cycle. Carry the remaining dots to the next line.
  static constexpr unsigned PpuDotsPerLine = 341;
  do
  {
    const bool visible = PPU_Scanline < NES_DISP_HEIGHT;
    const unsigned timing = NesRomTiming(reinterpret_cast<const uint8_t *>(&NesHeader));
    const bool pal = timing == 1;
    const bool dendy = timing == 3;
    const unsigned dotsNumerator = pal ? 5U : 1U;
    const unsigned dotsDenominator = pal ? 16U : 3U;
    const bool preRender = PPU_Scanline == ((pal || dendy) ? 311 : SCAN_PRE_RENDER);
    if ( preRender )
      PPU_R2 &= ~( R2_IN_VBLANK | R2_HIT_SP | R2_MAX_SP );
    else if ( PPU_Scanline == (dendy ? 291 : SCAN_VBLANK_START) )
    {
      PPU_R2 |= R2_IN_VBLANK;
      InfoNES_PadState( &PAD1_Latch, &PAD2_Latch, &PAD_System );
      if ( PPU_R0 & R0_NMI_VB )
        NMI_REQ;
    }

    const unsigned phase = PpuPhase;
    const unsigned lineTime = PpuDotsPerLine * dotsNumerator + phase;
    const unsigned cycles = lineTime / dotsDenominator;
    PpuPhase = lineTime % dotsDenominator;
    unsigned ran = 0;
    auto stepTo = [&](unsigned dot) {
      const unsigned deadline = (dot * dotsNumerator + phase) / dotsDenominator;
      if (deadline > ran) {
        K6502_Step(deadline - ran);
        ran = deadline;
      }
    };
    if ( visible )
    {
      if ( ChrBufUpdate )
        InfoNES_SetupChr();

      const int hit = SpriteZeroHitX();
      // Render from the scroll state latched for this line, before CPU writes
      // caused by the sprite-zero hit update the next line's scroll.
      InfoNES_DrawLine();
      if ( hit >= 0 )
      {
        stepTo((unsigned)hit + 2);
        if ( ( PPU_R1 & ( R1_SHOW_SCR | R1_SHOW_SP ) ) == ( R1_SHOW_SCR | R1_SHOW_SP ) )
          PPU_R2 |= R2_HIT_SP;
      }

      for ( WORD &pixel : Scanline )
        pixel = NesPalette[ pixel & 0x3f ];
      InfoNES_DrawScanline( PPU_Scanline, Scanline );
    }
    if (visible || preRender)
    {
      stepTo(256);
      if (visible && (PPU_R1 & (R1_SHOW_SCR | R1_SHOW_SP)))
        AdvanceVerticalScroll();
      stepTo(257);
      if (PPU_R1 & (R1_SHOW_SCR | R1_SHOW_SP))
        PPU_Addr = (PPU_Addr & ~0x041f) | (PPU_Temp & 0x041f);
#ifdef INFONES_MMC3
      const unsigned irqDot = (PPU_R0 & R0_BG_ADDR) ? 324 : 260;
      if (irqDot < 304) {
        stepTo(irqDot);
        InfoNES_Mmc3Scanline(preRender);
      }
#endif
      if (preRender)
      {
        stepTo(304);
        if (PPU_R1 & (R1_SHOW_SCR | R1_SHOW_SP))
          PPU_Addr = (PPU_Addr & ~0x7be0) | (PPU_Temp & 0x7be0);
      }
#ifdef INFONES_MMC3
      if (irqDot >= 304) {
        stepTo(irqDot);
        InfoNES_Mmc3Scanline(preRender);
      }
#endif
    }
    if (cycles > ran) K6502_Step(cycles - ran);

    FrameStep += cycles;
    if ( FrameStep > STEP_PER_FRAME && FrameIRQ_Enable )
    {
      FrameStep %= STEP_PER_FRAME;
      IRQ_REQ;
      APU_Reg[ 0x15 ] |= 0x40;
    }

    PPU_Scanline = preRender ? 0 : PPU_Scanline + 1;
  } while ( PPU_Scanline != 0 );
}

static void InfoNES_DrawLine()
{
/*
 *  Render a scanline
 *
 */

  int nX;
  int nY;
  int nY4;
  int nYBit;
  WORD *pPalTbl;
  BYTE *pAttrBase;
  WORD *pPoint;
  int nNameTable;
  BYTE *pbyNameTable;
  BYTE *pbyChrData;
  int nIdx;

  /*-------------------------------------------------------------------*/
  /*  Render Background                                                */
  /*-------------------------------------------------------------------*/

  // Pointer to the render position
  pPoint = Scanline;

  // Clear a scanline if screen is off
  if ( !( PPU_R1 & R1_SHOW_SCR ) )
  {
    for ( WORD &pixel : Scanline )
      pixel = PalTable[ 0 ];
  }
  else
  {
    nNameTable = NAME_TABLE0 + ( ( PPU_Addr >> 10 ) & 3 );
    nY = ( PPU_Addr >> 5 ) & 31;
    nYBit = ( ( PPU_Addr >> 12 ) & 7 ) * 8;
    const int firstTile = PPU_Addr & 31;
    nX = firstTile;

    nY4 = ( ( nY & 2 ) << 1 );

    /*-------------------------------------------------------------------*/
    /*  Rendering of the block of the left end                           */
    /*-------------------------------------------------------------------*/

    pbyNameTable = PPUBANK[ nNameTable ] + nY * 32 + nX;
    pbyChrData = PPU_BG_Base + ( *pbyNameTable << 6 ) + nYBit;
    pAttrBase = PPUBANK[ nNameTable ] + 0x3c0 + ( nY / 4 ) * 8;
    pPalTbl =  &PalTable[ ( ( ( pAttrBase[ nX >> 2 ] >> ( ( nX & 2 ) + nY4 ) ) & 3 ) << 2 ) ];

    for ( nIdx = PPU_FineX; nIdx < 8; ++nIdx )
    {
      *( pPoint++ ) = pPalTbl[ pbyChrData[ nIdx ] ];
    }

    ++nX;
    ++pbyNameTable;

    /*-------------------------------------------------------------------*/
    /*  Rendering of the left table                                      */
    /*-------------------------------------------------------------------*/

    for ( ; nX < 32; ++nX )
    {
      pbyChrData = PPU_BG_Base + ( *pbyNameTable << 6 ) + nYBit;
      pPalTbl = &PalTable[ ( ( ( pAttrBase[ nX >> 2 ] >> ( ( nX & 2 ) + nY4 ) ) & 3 ) << 2 ) ];

      pPoint[ 0 ] = pPalTbl[ pbyChrData[ 0 ] ]; 
      pPoint[ 1 ] = pPalTbl[ pbyChrData[ 1 ] ];
      pPoint[ 2 ] = pPalTbl[ pbyChrData[ 2 ] ];
      pPoint[ 3 ] = pPalTbl[ pbyChrData[ 3 ] ];
      pPoint[ 4 ] = pPalTbl[ pbyChrData[ 4 ] ];
      pPoint[ 5 ] = pPalTbl[ pbyChrData[ 5 ] ];
      pPoint[ 6 ] = pPalTbl[ pbyChrData[ 6 ] ];
      pPoint[ 7 ] = pPalTbl[ pbyChrData[ 7 ] ];
      pPoint += 8;

      ++pbyNameTable;
    }

    // Holizontal Mirror
    nNameTable ^= NAME_TABLE_H_MASK;

    pbyNameTable = PPUBANK[ nNameTable ] + nY * 32;
    pAttrBase = PPUBANK[ nNameTable ] + 0x3c0 + ( nY / 4 ) * 8;

    /*-------------------------------------------------------------------*/
    /*  Rendering of the right table                                     */
    /*-------------------------------------------------------------------*/

    for ( nX = 0; nX < firstTile; ++nX )
    {
      pbyChrData = PPU_BG_Base + ( *pbyNameTable << 6 ) + nYBit;
      pPalTbl = &PalTable[ ( ( ( pAttrBase[ nX >> 2 ] >> ( ( nX & 2 ) + nY4 ) ) & 3 ) << 2 ) ];

      pPoint[ 0 ] = pPalTbl[ pbyChrData[ 0 ] ]; 
      pPoint[ 1 ] = pPalTbl[ pbyChrData[ 1 ] ];
      pPoint[ 2 ] = pPalTbl[ pbyChrData[ 2 ] ];
      pPoint[ 3 ] = pPalTbl[ pbyChrData[ 3 ] ];
      pPoint[ 4 ] = pPalTbl[ pbyChrData[ 4 ] ];
      pPoint[ 5 ] = pPalTbl[ pbyChrData[ 5 ] ];
      pPoint[ 6 ] = pPalTbl[ pbyChrData[ 6 ] ];
      pPoint[ 7 ] = pPalTbl[ pbyChrData[ 7 ] ];
      pPoint += 8;

      ++pbyNameTable;
    }

    /*-------------------------------------------------------------------*/
    /*  Rendering of the block of the right end                          */
    /*-------------------------------------------------------------------*/

    pbyChrData = PPU_BG_Base + ( *pbyNameTable << 6 ) + nYBit;
    pPalTbl = &PalTable[ ( ( ( pAttrBase[ nX >> 2 ] >> ( ( nX & 2 ) + nY4 ) ) & 3 ) << 2 ) ];
    for ( nIdx = 0; nIdx < PPU_FineX; ++nIdx )
    {
      pPoint[ nIdx ] = pPalTbl[ pbyChrData[ nIdx ] ];
    }

    /*-------------------------------------------------------------------*/
    /*  Backgroud Clipping                                               */
    /*-------------------------------------------------------------------*/
    if ( !( PPU_R1 & R1_CLIP_BG ) )
    {
      WORD *pPointTop;

      pPointTop = Scanline;
      for ( int x = 0; x < 8; ++x )
        pPointTop[ x ] = PalTable[ 0 ];
    }

  }

  /*-------------------------------------------------------------------*/
  /*  Render a sprite                                                  */
  /*-------------------------------------------------------------------*/

  /*
   * NROM has no rendering callbacks. Draw sprites in OAM order and track which
   * pixels are already claimed, including opaque sprites behind the background.
   */
  if ( PPU_R1 & R1_SHOW_SP )
  {
    DWORD claimed[ NES_DISP_WIDTH / 32 ] = {};
    const unsigned left = ( PPU_R1 & R1_CLIP_SP ) ? 0 : 8;
    const unsigned height = PPU_SP_Height;
    unsigned count = 0;
    for ( int sprite = 0; sprite < 64; ++sprite )
    {
      const BYTE *oam = SPRRAM + sprite * 4;
      const unsigned row = (unsigned)( PPU_Scanline - oam[ SPR_Y ] - 1 );
      if ( row >= height )
        continue;
      if (++count > 8) break;

      const unsigned first = oam[ SPR_X ];
      const unsigned attr = oam[ SPR_ATTR ];
      const BYTE *pattern = SpritePattern( oam, row );
      const WORD *palette = PalTable + 0x10 + ( attr & SPR_ATTR_COLOR ) * 4;
      const unsigned flip = ( attr & SPR_ATTR_H_FLIP ) ? 7 : 0;
      for ( unsigned col = 0; col < 8 && first + col < NES_DISP_WIDTH; ++col )
      {
        const unsigned x = first + col;
        const BYTE color = pattern[ col ^ flip ];
        if ( color == 0 || x < left )
          continue;
        DWORD &pixels = claimed[ x >> 5 ];
        const DWORD bit = (DWORD)1 << ( x & 31 );
        if ( pixels & bit )
          continue;
        pixels |= bit;
        if ( !( attr & SPR_ATTR_PRI ) || ( Scanline[ x ] & 0x8000 ) )
          Scanline[ x ] = palette[ color ];
      }
    }
    if ( count > 8 )
      PPU_R2 |= R2_MAX_SP;
  }
}

/*===================================================================*/
/*                                                                   */
/*            InfoNES_SetupChr() : Develop character data            */
/*                                                                   */
/*===================================================================*/
void InfoNES_SetupChr()
{
/*
 *  Develop character data
 *
 */

  BYTE *pbyBGData;
  BYTE byData1;
  BYTE byData2;
  int nIdx;
  int nY;
  int nOff;
  static BYTE *pbyPrevBank[ 8 ];
  int nBank;

  for ( nBank = 0; nBank < 8; ++nBank )
  {
    if ( pbyPrevBank[ nBank ] == PPUBANK[ nBank ] && !( ( ChrBufUpdate >> nBank ) & 1 ) )
      continue;  // Next bank

    /*-------------------------------------------------------------------*/
    /*  An address is different from the last time                       */
    /*    or                                                             */
    /*  An update flag is being set                                      */
    /*-------------------------------------------------------------------*/

    for ( nIdx = 0; nIdx < 64; ++nIdx )
    {
      nOff = ( nBank << 12 ) + ( nIdx << 6 );

      for ( nY = 0; nY < 8; ++nY )
      {
        pbyBGData = PPUBANK[ nBank ] + ( nIdx << 4 ) + nY;

        byData1 = ( ( pbyBGData[ 0 ] >> 1 ) & 0x55 ) | ( pbyBGData[ 8 ] & 0xAA );
        byData2 = ( pbyBGData[ 0 ] & 0x55 ) | ( ( pbyBGData[ 8 ] << 1 ) & 0xAA );

        ChrBuf[ nOff ]     = ( byData1 >> 6 ) & 3;
        ChrBuf[ nOff + 1 ] = ( byData2 >> 6 ) & 3;
        ChrBuf[ nOff + 2 ] = ( byData1 >> 4 ) & 3;
        ChrBuf[ nOff + 3 ] = ( byData2 >> 4 ) & 3;
        ChrBuf[ nOff + 4 ] = ( byData1 >> 2 ) & 3;
        ChrBuf[ nOff + 5 ] = ( byData2 >> 2 ) & 3;
        ChrBuf[ nOff + 6 ] = byData1 & 3;
        ChrBuf[ nOff + 7 ] = byData2 & 3;

        nOff += 8;
      }
    }
    // Keep this address
    pbyPrevBank[ nBank ] = PPUBANK[ nBank ];
  }

  // Reset update flag
  ChrBufUpdate = 0;
}
