# InfoNES for STM32F407

This directory contains the CPU, PPU, and NROM mapper from [InfoNES](https://github.com/jay-kumogata/InfoNES/tree/fe3295c0a86bf5bbecc22aeab3b3af11f6f47908) under the accompanying Apache 2.0 license.

The STM32 port runs one frame per call and emits 256 RGB565 pixels per visible scanline. It keeps CHR decoding cached, puts PPU and cartridge RAM in CCM, and omits sound synthesis. It supports mapper 0 (NROM) and uses InfoNES's approximate NTSC scanline timing.
