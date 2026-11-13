// Modified for STM32F407 from the original InfoNES source.
/*===================================================================*/
/*                                                                   */
/*  InfoNES.h : NES Emulator for Win32, Linux(x86), Linux(PS2)       */
/*                                                                   */
/*  2000/05/14  InfoNES Project ( based on pNesX )                   */
/*                                                                   */
/*===================================================================*/

#ifndef InfoNES_H_INCLUDED
#define InfoNES_H_INCLUDED

/*-------------------------------------------------------------------*/
/*  Include files                                                    */
/*-------------------------------------------------------------------*/

#include "InfoNES_Types.h"

/*-------------------------------------------------------------------*/
/*  NES resources                                                    */
/*-------------------------------------------------------------------*/

#define RAM_SIZE     0x0800
#define SRAM_SIZE    0x2000
#define PPURAM_SIZE  0x4000
#define SPRRAM_SIZE  256

/* RAM */
extern BYTE RAM[];

/* SRAM */
extern BYTE SRAM[];

/* ROM */
extern BYTE *ROM;

/* SRAM BANK ( 8Kb ) */
extern BYTE *SRAMBANK;

/* ROM BANK ( 8Kb * 4 ) */
extern BYTE *ROMBANK0;
extern BYTE *ROMBANK1;
extern BYTE *ROMBANK2;
extern BYTE *ROMBANK3;

/*-------------------------------------------------------------------*/
/*  PPU resources                                                    */
/*-------------------------------------------------------------------*/

/* PPU RAM */
extern BYTE PPURAM[];

/* VROM */
extern BYTE *VROM;

/* PPU BANK ( 1Kb * 16 ) */
extern BYTE *PPUBANK[];

#define NAME_TABLE0  8
#define NAME_TABLE1  9
#define NAME_TABLE2  10
#define NAME_TABLE3  11

#define NAME_TABLE_V_MASK 2
#define NAME_TABLE_H_MASK 1

/* Sprite RAM */
extern BYTE SPRRAM[];

#define SPR_Y    0
#define SPR_CHR  1
#define SPR_ATTR 2
#define SPR_X    3
#define SPR_ATTR_COLOR  0x3
#define SPR_ATTR_V_FLIP 0x80
#define SPR_ATTR_H_FLIP 0x40
#define SPR_ATTR_PRI    0x20

/* PPU Register */
extern BYTE PPU_R0;
extern BYTE PPU_R1;
extern BYTE PPU_R2;
extern BYTE PPU_R3;
extern BYTE PPU_R7;
extern BYTE PPU_OpenBus;

extern BYTE PPU_FineX;
extern WORD PPU_Addr;
extern WORD PPU_Temp;
extern WORD PPU_Increment;

extern BYTE PPU_Latch_Flag;

#define R0_NMI_VB      0x80
#define R0_SP_SIZE     0x20
#define R0_BG_ADDR     0x10
#define R0_SP_ADDR     0x08
#define R0_INC_ADDR    0x04
#define R0_NAME_ADDR   0x03

#define R1_BACKCOLOR   0xe0
#define R1_SHOW_SP     0x10
#define R1_SHOW_SCR    0x08
#define R1_CLIP_SP     0x04
#define R1_CLIP_BG     0x02
#define R1_MONOCHROME  0x01

#define R2_IN_VBLANK   0x80
#define R2_HIT_SP      0x40
#define R2_MAX_SP      0x20
#define R2_WRITE_FLAG  0x10

#define SCAN_VBLANK_START             241
#define SCAN_PRE_RENDER               261
#define STEP_PER_FRAME                29828

/* Current Scanline */
extern WORD PPU_Scanline;

/* BG Base Address */
extern BYTE *PPU_BG_Base;

/* Sprite Base Address */
extern BYTE *PPU_SP_Base;

/* Sprite Height */
extern WORD PPU_SP_Height;

/* NES display size */
#define NES_DISP_WIDTH      256
#define NES_DISP_HEIGHT     240

/* VRAM Write Enable ( 0: Disable, 1: Enable ) */
extern BYTE byVramWriteEnable;

/* Frame IRQ ( 0: Disabled, 1: Enabled )*/
extern BYTE FrameIRQ_Enable;
extern WORD FrameStep;

/*-------------------------------------------------------------------*/
/*  Display and Others resouces                                      */
/*-------------------------------------------------------------------*/

/* Frame Skip */
extern WORD FrameSkip;
extern WORD FrameCnt;
extern WORD FrameWait;

extern BYTE ChrBuf[];

extern BYTE ChrBufUpdate;

extern WORD PalTable[];

/*-------------------------------------------------------------------*/
/*  APU and Pad resources                                            */
/*-------------------------------------------------------------------*/

extern BYTE APU_Reg[];

extern DWORD PAD1_Latch;
extern DWORD PAD2_Latch;
extern DWORD PAD_System;
extern DWORD PAD1_Bit;
extern DWORD PAD2_Bit;

#define PAD_SYS_QUIT   1
#define PAD_SYS_OK     2
#define PAD_SYS_CANCEL 4
#define PAD_SYS_UP     8
#define PAD_SYS_DOWN   0x10
#define PAD_SYS_LEFT   0x20
#define PAD_SYS_RIGHT  0x40

#define PAD_PUSH(a,b)  ( ( (a) & (b) ) != 0 )

/*-------------------------------------------------------------------*/
/*  Mapper Function                                                  */
/*-------------------------------------------------------------------*/

/* Initialize Mapper */
extern void (*MapperInit)();
/* Write to Mapper */
extern void (*MapperWrite)( WORD wAddr, BYTE byData );
/* Write to SRAM */
extern void (*MapperSram)( WORD wAddr, BYTE byData );
/* Write to APU */
extern void (*MapperApu)( WORD wAddr, BYTE byData );
/* Read from Apu */
extern BYTE (*MapperReadApu)( WORD wAddr );
/* Callback at VSync */
extern void (*MapperVSync)();
/* Callback at HSync */
extern void (*MapperHSync)();
/* Callback at PPU read/write */
extern void (*MapperPPU)( WORD wAddr );
/* Callback at Rendering Screen 1:BG, 0:Sprite */
extern void (*MapperRenderScreen)( BYTE byMode );

/*-------------------------------------------------------------------*/
/*  ROM information                                                  */
/*-------------------------------------------------------------------*/

/* .nes File Header */
struct NesHeader_tag
{
  BYTE byID[ 4 ];
  BYTE byRomSize;
  BYTE byVRomSize;
  BYTE byInfo1;
  BYTE byInfo2;
  BYTE byReserve[ 8 ];
};

/* .nes File Header */
extern struct NesHeader_tag NesHeader;

/* Mapper No. */
extern BYTE MapperNo;

/* Other */
extern BYTE ROM_Mirroring;
extern BYTE ROM_SRAM;
extern BYTE ROM_Trainer;
extern BYTE ROM_FourScr;
#if defined(INFONES_MMC1) || defined(INFONES_MMC3)
extern WORD PrgRamMask;
extern bool PrgRamRead;
extern bool PrgRamWrite;
#endif
#ifdef INFONES_MMC3
extern bool MapperIrqPending;
void InfoNES_Mmc3A12(WORD address, uint32_t cpuCycle);
void InfoNES_Mmc3Scanline(bool preRender);
#endif
#if defined(INFONES_AXROM) || defined(INFONES_MMC1) || defined(INFONES_MMC3)
BYTE InfoNES_ReadMappedPrg(WORD address);
#endif

/*-------------------------------------------------------------------*/
/*  Function prototypes                                              */
/*-------------------------------------------------------------------*/

/* Initialize InfoNES */
void InfoNES_Init();

/* Completion treatment */
void InfoNES_Fin();

/* Load a cassette */
int InfoNES_Load( const char *pszFileName );

/* Reset InfoNES */
int InfoNES_Reset();

/* Initialize PPU */
void InfoNES_SetupPPU();

/* Set up a Mirroring of Name Table */
void InfoNES_Mirroring( int nType );

/* Run one frame, then return control to the firmware. */
void InfoNES_RunFrame();

/* Develop character data */
void InfoNES_SetupChr();

#endif /* !InfoNES_H_INCLUDED */
