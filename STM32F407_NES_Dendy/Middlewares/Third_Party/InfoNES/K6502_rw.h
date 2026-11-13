// Modified for STM32F407 from the original InfoNES source.
/*===================================================================*/
/*                                                                   */
/*  K6502_RW.h : 6502 Reading/Writing Operation for NES              */
/*               This file is included in K6502.cpp                  */
/*                                                                   */
/*  2000/5/23   InfoNES Project ( based on pNesX )                   */
/*                                                                   */
/*===================================================================*/

#ifndef K6502_RW_H_INCLUDED
#define K6502_RW_H_INCLUDED

/*-------------------------------------------------------------------*/
/*  Include files                                                    */
/*-------------------------------------------------------------------*/

#include "InfoNES.h"
#include "InfoNES_System.h"

/*===================================================================*/
/*                                                                   */
/*            K6502_ReadZp() : Reading from the zero page            */
/*                                                                   */
/*===================================================================*/
static inline BYTE K6502_ReadZp( BYTE byAddr )
{
/*
 *  Reading from the zero page
 *
 *  Parameters
 *    BYTE byAddr              (Read)
 *      An address inside the zero page
 *
 *  Return values
 *    Read Data
 */

#ifdef INFONES_AXROM
  return CpuDataBus = RAM[byAddr];
#else
  return RAM[byAddr];
#endif
}

/*===================================================================*/
/*                                                                   */
/*               K6502_Read() : Reading operation                    */
/*                                                                   */
/*===================================================================*/
#ifdef INFONES_AXROM
static inline BYTE K6502_Read(WORD address)
{
  return CpuDataBus = K6502_ReadBus(address);
}

static inline BYTE K6502_ReadBus(WORD wAddr)
#else
static inline BYTE K6502_Read(WORD wAddr)
#endif
{
/*
 *  Reading operation
 *
 *  Parameters
 *    WORD wAddr              (Read)
 *      Address to read
 *
 *  Return values
 *    Read data
 *
 *  Remarks
 *    0x0000 - 0x1fff  RAM ( 0x800 - 0x1fff is mirror of 0x0 - 0x7ff )
 *    0x2000 - 0x3fff  PPU
 *    0x4000 - 0x5fff  Sound
 *    0x6000 - 0x7fff  SRAM ( Battery Backed )
 *    0x8000 - 0xffff  ROM
 *
 */
  BYTE byRet;

  switch ( wAddr & 0xe000 )
  {
    case 0x0000:  /* RAM */
      return RAM[ wAddr & 0x7ff ];

    case 0x2000:  /* PPU */
      if ( ( wAddr & 0x7 ) == 0x7 )   /* PPU Memory */
      {
        WORD addr = PPU_Addr & 0x3fff;

        byRet = PPU_R7;
        if (addr >= 0x3f00) {
          unsigned color = addr & 31;
          if ((color & 3) == 0) color &= 15;
          byRet = (PPURAM[0x3f00 + color] & 0x3f) | (PPU_OpenBus & 0xc0);
          addr -= 0x1000;
        }
        PPU_R7 = PPUBANK[addr >> 10][addr & 0x3ff];
        PPU_Addr = (PPU_Addr + PPU_Increment) & 0x7fff;
#ifdef INFONES_MMC3
        InfoNES_Mmc3A12(PPU_Addr, K6502_CycleBase + g_wPassedClocks);
#endif
        return PPU_OpenBus = byRet;
      }
      else
      if ( ( wAddr & 0x7 ) == 0x4 )   /* SPR_RAM I/O Register */
      {
        return PPU_OpenBus = SPRRAM[ PPU_R3 ];
      }
      else                            
      if ( ( wAddr & 0x7 ) == 0x2 )   /* PPU Status */
      {
        byRet = (PPU_R2 & 0xe0) | (PPU_OpenBus & 0x1f);
        PPU_R2 &= ~R2_IN_VBLANK;
        PPU_Latch_Flag = 0;
        return PPU_OpenBus = byRet;
      }
      else /* $2000, $2001, $2003, $2005, $2006 */
      {
	return PPU_OpenBus;
      }
      break;

    case 0x4000:  /* Sound */
      if ( wAddr == 0x4014 ) 
      {
	return wAddr & 0xff;
      }
      else
      if ( wAddr == 0x4015 )
      {
        // APU control
        byRet = APU_Reg[ 0x15 ] & 0x40;

	// FrameIRQ
        APU_Reg[ 0x15 ] &= ~0x40;
        return byRet;
      }
      else
      if ( wAddr == 0x4016 )
      {
        // Set Joypad1 data
        byRet = (PAD1_Bit >= 8 ? 1 : (PAD1_Latch >> PAD1_Bit) & 1) | 0x40;
        if (!(APU_Reg[0x16] & 1) && PAD1_Bit < 8) ++PAD1_Bit;
        return byRet;
      }
      else
      if ( wAddr == 0x4017 )
      {
        // Set Joypad2 data
        byRet = (PAD2_Bit >= 8 ? 1 : (PAD2_Latch >> PAD2_Bit) & 1) | 0x40;
        if (!(APU_Reg[0x16] & 1) && PAD2_Bit < 8) ++PAD2_Bit;
        return byRet;
      }
      else 
      {
        /* Return Mapper Register*/
        return MapperReadApu( wAddr );
      }
      break;
      // The other sound registers are not readable.

    case 0x6000:  /* SRAM */
#if defined(INFONES_AXROM)
      return CpuDataBus;
#elif defined(INFONES_MMC1) || defined(INFONES_MMC3)
      return PrgRamRead ? SRAM[wAddr & PrgRamMask] : (BYTE)(wAddr >> 8);
#else
      if ( ROM_SRAM )
      {
        return SRAM[ wAddr & 0x1fff ];
      } else {    /* SRAM BANK */
        return SRAMBANK[ wAddr & 0x1fff ];
      }

#endif
#if defined(INFONES_AXROM) || defined(INFONES_MMC1) || defined(INFONES_MMC3)
    case 0x8000:
    case 0xa000:
    case 0xc000:
    case 0xe000:
      return InfoNES_ReadMappedPrg(wAddr);
#else
    case 0x8000:  /* ROM BANK 0 */
      return ROMBANK0[ wAddr & 0x1fff ];

    case 0xa000:  /* ROM BANK 1 */
      return ROMBANK1[ wAddr & 0x1fff ];

    case 0xc000:  /* ROM BANK 2 */
      return ROMBANK2[ wAddr & 0x1fff ];

    case 0xe000:  /* ROM BANK 3 */
      return ROMBANK3[ wAddr & 0x1fff ];
#endif
  }

  return ( wAddr >> 8 ); /* when a register is not readable the upper half
                            address is returned. */
}

/*===================================================================*/
/*                                                                   */
/*               K6502_Write() : Writing operation                    */
/*                                                                   */
/*===================================================================*/
static inline void K6502_Write( WORD wAddr, BYTE byData )
{
/*
 *  Writing operation
 *
 *  Parameters
 *    WORD wAddr              (Read)
 *      Address to write
 *
 *    BYTE byData             (Read)
 *      Data to write
 *
 *  Remarks
 *    0x0000 - 0x1fff  RAM ( 0x800 - 0x1fff is mirror of 0x0 - 0x7ff )
 *    0x2000 - 0x3fff  PPU
 *    0x4000 - 0x5fff  Sound
 *    0x6000 - 0x7fff  SRAM ( Battery Backed )
 *    0x8000 - 0xffff  ROM
 *
 */

#ifdef INFONES_AXROM
  CpuDataBus = byData;
#endif
  switch ( wAddr & 0xe000 )
  {
    case 0x0000:  /* RAM */
      RAM[ wAddr & 0x7ff ] = byData;
      break;

    case 0x2000:  /* PPU */
      PPU_OpenBus = byData;
      switch ( wAddr & 0x7 )
      {
        case 0:    /* 0x2000 */
        {
          const BYTE previous = PPU_R0;
          PPU_R0 = byData;
          PPU_Increment = ( PPU_R0 & R0_INC_ADDR ) ? 32 : 1;
          PPU_BG_Base = ( PPU_R0 & R0_BG_ADDR ) ? ChrBuf + 256 * 64 : ChrBuf;
          PPU_SP_Base = ( PPU_R0 & R0_SP_ADDR ) ? ChrBuf + 256 * 64 : ChrBuf;
          PPU_SP_Height = ( PPU_R0 & R0_SP_SIZE ) ? 16 : 8;
          PPU_Temp = ( PPU_Temp & ~0x0c00 ) | ( ( byData & R0_NAME_ADDR ) << 10 );
          if ( !( previous & R0_NMI_VB ) && ( byData & R0_NMI_VB ) && ( PPU_R2 & R2_IN_VBLANK ) )
            NMI_REQ;
          break;
        }
        case 1:   /* 0x2001 */
          PPU_R1 = byData;
          break;

        case 2:   /* 0x2002 */
#if 0	  
          PPU_R2 = byData;     // 0x2002 is not writable
#endif
          break;

        case 3:   /* 0x2003 */
          // Sprite RAM Address
          PPU_R3 = byData;
          break;

        case 4:   /* 0x2004 */
          // Write data to Sprite RAM
          SPRRAM[ PPU_R3++ ] = byData;
          break;

        case 5:   /* 0x2005: write fine X and coarse X, then fine Y and coarse Y. */
          if ( PPU_Latch_Flag )
          {
            PPU_Temp = ( PPU_Temp & ~0x73e0 ) |
                       ( ( byData & 0xf8 ) << 2 ) | ( ( byData & 7 ) << 12 );
          }
          else
          {
            PPU_FineX = byData & 7;
            PPU_Temp = ( PPU_Temp & ~0x001f ) | ( byData >> 3 );
          }
          PPU_Latch_Flag ^= 1;
          break;

        case 6:   /* 0x2006: second write copies the temporary address into v. */
          if ( PPU_Latch_Flag )
          {
            PPU_Temp = ( PPU_Temp & 0x7f00 ) | byData;
            PPU_Addr = PPU_Temp;
#ifdef INFONES_MMC3
            InfoNES_Mmc3A12(PPU_Addr, K6502_CycleBase + g_wPassedClocks);
#endif
          }
          else
          {
            PPU_Temp = ( PPU_Temp & 0x00ff ) | ( ( byData & 0x3f ) << 8 );
          }
          PPU_Latch_Flag ^= 1;
          break;

        case 7:   /* 0x2007 */
          {
            WORD addr = PPU_Addr;
            
            // Increment PPU Address
            PPU_Addr = ( PPU_Addr + PPU_Increment ) & 0x7fff;
            addr &= 0x3fff;
#ifdef INFONES_MMC3
            InfoNES_Mmc3A12(PPU_Addr, K6502_CycleBase + g_wPassedClocks);
#endif

            // Write to PPU Memory
            if ( addr < 0x2000 )
            {
              // Pattern Data can only be written on CHR RAM cartridges.
              if ( byVramWriteEnable )
              {
                ChrBufUpdate |= ( 1 << ( addr >> 10 ) );
                PPUBANK[ addr >> 10 ][ addr & 0x3ff ] = byData;
              }
            }
            else
            if ( addr < 0x3f00 )  /* 0x2000 - 0x3eff */
            {
              // Name Table and mirror
              PPUBANK[addr >> 10][addr & 0x3ff] = byData;
            }
            else
            if ( !( addr & 0xf ) )  /* 0x3f00 or 0x3f10 */
            {
              // Palette mirror
              PPURAM[ 0x3f10 ] = PPURAM[ 0x3f14 ] = PPURAM[ 0x3f18 ] = PPURAM[ 0x3f1c ] = 
              PPURAM[ 0x3f00 ] = PPURAM[ 0x3f04 ] = PPURAM[ 0x3f08 ] = PPURAM[ 0x3f0c ] = byData;
              PalTable[ 0x00 ] = PalTable[ 0x04 ] = PalTable[ 0x08 ] = PalTable[ 0x0c ] =
              PalTable[ 0x10 ] = PalTable[ 0x14 ] = PalTable[ 0x18 ] = PalTable[ 0x1c ] = ( byData & 0x3f ) | 0x8000;
            }
            else
	    if ( addr & 3 )
            {
              // Palette
              PPURAM[ addr ] = byData;
              PalTable[ addr & 0x1f ] = byData & 0x3f;
            }
          }
          break;
      }
      break;

    case 0x4000:  /* Sound */
      switch ( wAddr & 0x1f )
      {
        case 0x00:
        case 0x01:
        case 0x02:
        case 0x03:          
        case 0x04:
        case 0x05:
        case 0x06:
        case 0x07:
        case 0x08:
        case 0x09:
        case 0x0a:
        case 0x0b:
        case 0x0c:
        case 0x0d:
        case 0x0e:
        case 0x0f:
        case 0x10:
        case 0x11:	  
        case 0x12:
        case 0x13:
          // The STM32 port does not synthesize sound.
          break;

        case 0x14:  /* 0x4014 */
        {
          const WORD source = (WORD)byData << 8;
          for (unsigned i = 0; i < SPRRAM_SIZE; ++i)
            SPRRAM[PPU_R3++] = K6502_Read(source + i);
          CLK(513 + ((K6502_CycleBase + g_wPassedClocks) & 1));
          break;
        }

        case 0x16:  /* 0x4016 */
	  // For VS-Unisystem
	  MapperApu( wAddr, byData );
          // Reset joypad
          if ( ( APU_Reg[ 0x16 ] | byData ) & 1 )
          {
            PAD1_Bit = 0;
            PAD2_Bit = 0;
          }
          break;

        case 0x17:  /* 0x4017 */
          // Frame IRQ
          FrameStep = 0;
          if ( !( byData & 0xc0 ) )
          {
            FrameIRQ_Enable = 1;
          } else {
            FrameIRQ_Enable = 0;
          }
          break;
      }

      if ( wAddr <= 0x4017 )
      {
        /* Write to APU Register */
        APU_Reg[ wAddr & 0x1f ] = wAddr == 0x4015 ? APU_Reg[ 0x15 ] & 0x40 : byData;
      }
      else
      {
        /* Write to APU */
        MapperApu( wAddr, byData );
      }
      break;

    case 0x6000:  /* SRAM */
#if defined(INFONES_AXROM)
      break; // AxROM has no PRG RAM.
#elif defined(INFONES_MMC1) || defined(INFONES_MMC3)
      if (PrgRamWrite) SRAM[wAddr & PrgRamMask] = byData;
#else
      SRAM[ wAddr & 0x1fff ] = byData;

      /* Write to SRAM, when no SRAM */
      if ( !ROM_SRAM )
      {
        MapperSram( wAddr, byData );
      }
#endif
      break;

    case 0x8000:  /* ROM BANK 0 */
    case 0xa000:  /* ROM BANK 1 */
    case 0xc000:  /* ROM BANK 2 */
    case 0xe000:  /* ROM BANK 3 */
      // Write to Mapper
      MapperWrite( wAddr, byData );
      break;
  }
}

// Reading/Writing operation (WORD version)
static inline WORD K6502_ReadW( WORD wAddr )
{
  const WORD low = K6502_Read( wAddr );
  return low | (WORD)K6502_Read( wAddr + 1 ) << 8;
}
static inline void K6502_WriteW( WORD wAddr, WORD wData ){ K6502_Write( wAddr, wData & 0xff ); K6502_Write( wAddr + 1, wData >> 8 ); };
static inline WORD K6502_ReadZpW(BYTE address)
{
  const WORD low = K6502_ReadZp(address);
  return low | ((WORD)K6502_ReadZp(address + 1) << 8);
}

// 6502's indirect absolute jmp(opcode: 6C) has a bug (added at 01/08/15 )
static inline WORD K6502_ReadW2( WORD wAddr )
{ 
  if ( 0x00ff == ( wAddr & 0x00ff ) )
  {
    return K6502_Read( wAddr ) | (WORD)K6502_Read( wAddr - 0x00ff ) << 8;
  } else {
    return K6502_Read( wAddr ) | (WORD)K6502_Read( wAddr + 1 ) << 8;
  }
}

#endif /* !K6502_RW_H_INCLUDED */
