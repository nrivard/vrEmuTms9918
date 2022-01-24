/*
 * Troy's TMS9918 Emulator - Core interface
 *
 * Copyright (c) 2021 Troy Schrapel
 *
 * This code is licensed under the MIT license
 *
 * https://github.com/visrealm/VrEmuTms9918a
 *
 */

#include "tms9918_core.h"
#include <stdlib.h>
#include <memory.h>
#include <math.h>

#define VRAM_SIZE (1 << 14) /* 16KB */

#define GRAPHICS_NUM_COLS 32
#define GRAPHICS_NUM_ROWS 24
#define GRAPHICS_CHAR_WIDTH 8

#define TEXT_NUM_COLS 40
#define TEXT_NUM_ROWS 24
#define TEXT_CHAR_WIDTH 6

#define MAX_SPRITES 32
#define SPRITE_ATTR_BYTES 4
#define LAST_SPRITE_VPOS 0xD0
#define MAX_SCANLINE_SPRITES 4

#define STATUS_INT 0x80
#define STATUS_5S  0x40
#define STATUS_COL 0x20

 /* PRIVATE DATA STRUCTURE
  * ---------------------------------------- */
struct vrEmuTMS9918a_s
{
  uint8_t vram[VRAM_SIZE];

  uint8_t registers[TMS_NUM_REGISTERS];
  
  uint8_t status;

  uint8_t lastMode;

  unsigned short currentAddress;

  vrEmuTms9918aMode mode;

  uint8_t rowSpriteBits[TMS9918A_PIXELS_X];
};


/* Function:  tmsMode
  * --------------------
  * return the current mode
  */
static vrEmuTms9918aMode tmsMode(VrEmuTms9918a* tms9918a)
{
  if (tms9918a->registers[TMS_REG_0] & 0x02)
  {
    return TMS_MODE_GRAPHICS_II;
  }

  switch ((tms9918a->registers[TMS_REG_1] & 0x18) >> 3)
  {
    case 0:
      return TMS_MODE_GRAPHICS_I;

    case 1:
      return TMS_MODE_MULTICOLOR;

    case 2:
      return TMS_MODE_TEXT;
  }
  return TMS_MODE_GRAPHICS_I;
}


/* Function:  tmsSpriteSize
  * --------------------
  * sprite size (0 = 8x8, 1 = 16x16)
  */
static inline int tmsSpriteSize(VrEmuTms9918a* tms9918a)
{
  return (tms9918a->registers[TMS_REG_1] & 0x02) >> 1;
}

/* Function:  tmsSpriteMagnification
  * --------------------
  * sprite size (0 = 1x, 1 = 2x)
  */
static inline int tmsSpriteMag(VrEmuTms9918a* tms9918a)
{
  return tms9918a->registers[TMS_REG_1] & 0x01;
}

/* Function:  tmsNameTableAddr
  * --------------------
  * name table base address
  */
static inline unsigned short tmsNameTableAddr(VrEmuTms9918a* tms9918a)
{
  return (tms9918a->registers[TMS_REG_2] & 0x0f) << 10;
}

/* Function:  tmsColorTableAddr
  * --------------------
  * color table base address
  */
static inline unsigned short tmsColorTableAddr(VrEmuTms9918a* tms9918a)
{
  if (tms9918a->mode == TMS_MODE_GRAPHICS_II)
    return (tms9918a->registers[TMS_REG_3] & 0x80) << 6;
  return tms9918a->registers[TMS_REG_3] << 6;
}

/* Function:  tmsPatternTableAddr
  * --------------------
  * pattern table base address
  */
static inline unsigned short tmsPatternTableAddr(VrEmuTms9918a* tms9918a)
{
  if (tms9918a->mode == TMS_MODE_GRAPHICS_II)
    return (tms9918a->registers[TMS_REG_4] & 0x04) << 11;
  return (tms9918a->registers[TMS_REG_4] & 0x07) << 11;
}

/* Function:  tmsSpriteAttrTableAddr
  * --------------------
  * sprite attribute table base address
  */
static inline unsigned short tmsSpriteAttrTableAddr(VrEmuTms9918a* tms9918a)
{
  return (tms9918a->registers[TMS_REG_5] & 0x7f) << 7;
}

/* Function:  tmsSpritePatternTableAddr
  * --------------------
  * sprite pattern table base address
  */
static inline unsigned short tmsSpritePatternTableAddr(VrEmuTms9918a* tms9918a)
{
  return (tms9918a->registers[TMS_REG_6] & 0x07) << 11;
}

/* Function:  tmsBgColor
  * --------------------
  * background color
  */
static inline vrEmuTms9918aColor tmsMainBgColor(VrEmuTms9918a* tms9918a)
{
  return (vrEmuTms9918aColor)((vrEmuTms9918aDisplayEnabled(tms9918a) 
            ? tms9918a->registers[TMS_REG_7] 
            : TMS_BLACK) & 0x0f);
}

/* Function:  tmsFgColor
  * --------------------
  * foreground color
  */
static inline vrEmuTms9918aColor tmsMainFgColor(VrEmuTms9918a* tms9918a)
{
  vrEmuTms9918aColor c = (vrEmuTms9918aColor)(tms9918a->registers[TMS_REG_7] >> 4);
  return c == TMS_TRANSPARENT ? tmsMainBgColor(tms9918a) : c;
}

/* Function:  tmsFgColor
  * --------------------
  * foreground color
  */
static inline vrEmuTms9918aColor tmsFgColor(VrEmuTms9918a* tms9918a, uint8_t colorByte)
{
  vrEmuTms9918aColor c = (vrEmuTms9918aColor)(colorByte >> 4);
  return c == TMS_TRANSPARENT ? tmsMainBgColor(tms9918a) : c;
}

/* Function:  tmsBgColor
  * --------------------
  * background color
  */
static inline vrEmuTms9918aColor tmsBgColor(VrEmuTms9918a* tms9918a, uint8_t colorByte)
{
  vrEmuTms9918aColor c = (vrEmuTms9918aColor)(colorByte & 0x0f);
  return c == TMS_TRANSPARENT ? tmsMainBgColor(tms9918a) : c;
}


/* Function:  vrEmuTms9918aNew
  * --------------------
  * create a new TMS9918A
  */
VR_EMU_TMS9918A_DLLEXPORT VrEmuTms9918a* vrEmuTms9918aNew()
{
  VrEmuTms9918a* tms9918a = (VrEmuTms9918a*)malloc(sizeof(VrEmuTms9918a));
  if (tms9918a != NULL)
  {
    vrEmuTms9918aReset(tms9918a);
 }

  return tms9918a;
}

/* Function:  vrEmuTms9918aReset
  * --------------------
  * reset the new TMS9918A
  */
VR_EMU_TMS9918A_DLLEXPORT void vrEmuTms9918aReset(VrEmuTms9918a* tms9918a)
{
  if (tms9918a)
  {
    /* initialization */
    tms9918a->currentAddress = 0;
    tms9918a->lastMode = 0;
    tms9918a->status = 0;
    memset(tms9918a->registers, 0, sizeof(tms9918a->registers));
    memset(tms9918a->vram, 0xff, sizeof(tms9918a->vram));
  }
}


/* Function:  vrEmuTms9918aDestroy
 * --------------------
 * destroy a TMS9918A
 *
 * tms9918a: tms9918a object to destroy / clean up
 */
VR_EMU_TMS9918A_DLLEXPORT void vrEmuTms9918aDestroy(VrEmuTms9918a* tms9918a)
{
  if (tms9918a)
  {
    /* destruction */
    free(tms9918a);
  }
}

/* Function:  vrEmuTms9918aWriteAddr
 * --------------------
 * write an address (mode = 1) to the tms9918a
 *
 * data: the data (DB0 -> DB7) to send
 */
VR_EMU_TMS9918A_DLLEXPORT void vrEmuTms9918aWriteAddr(VrEmuTms9918a* tms9918a, uint8_t data)
{
  if (tms9918a->lastMode)
  {
    /* second address byte */

    if (data & 0x80) /* register */
    {
      tms9918a->registers[data & 0x07] = tms9918a->currentAddress & 0xff;

      tms9918a->mode = tmsMode(tms9918a);
    }
    else /* address */
    {
      tms9918a->currentAddress |= ((data & 0x3f) << 8);
    }
    tms9918a->lastMode = 0;
  }
  else
  {
    tms9918a->currentAddress = data;
    tms9918a->lastMode = 1;
  }
}

/* Function:  vrEmuTms9918aWriteData
 * --------------------
 * write data (mode = 0) to the tms9918a
 *
 * data: the data (DB0 -> DB7) to send
 */
VR_EMU_TMS9918A_DLLEXPORT void vrEmuTms9918aWriteData(VrEmuTms9918a* tms9918a, uint8_t data)
{
  tms9918a->vram[(tms9918a->currentAddress++) & 0x3fff] = data;
}

/* Function:  vrEmuTms9918aReadStatus
 * --------------------
 * read from the status register
 */
VR_EMU_TMS9918A_DLLEXPORT uint8_t vrEmuTms9918aReadStatus(VrEmuTms9918a* tms9918a)
{
  uint8_t tmpStatus = tms9918a->status;
  tms9918a->status &= ~(STATUS_INT | STATUS_COL);
  return tmpStatus;
}

/* Function:  vrEmuTms9918aReadData
 * --------------------
 * read data (mode = 0) from the tms9918a
 */
VR_EMU_TMS9918A_DLLEXPORT uint8_t vrEmuTms9918aReadData(VrEmuTms9918a* tms9918a)
{
  return tms9918a->vram[(tms9918a->currentAddress++) & 0x3fff];
}

/* Function:  vrEmuTms9918aReadDataNoInc
 * --------------------
 * read data (mode = 0) from the tms9918a
 */
VR_EMU_TMS9918A_DLLEXPORT uint8_t vrEmuTms9918aReadDataNoInc(VrEmuTms9918a* tms9918a)
{
  return tms9918a->vram[tms9918a->currentAddress & 0x3fff];
}

/* Function:  vrEmuTms9918aOutputSprites
 * ----------------------------------------
 * Output Sprites to a scanline
 */
static void vrEmuTms9918aOutputSprites(VrEmuTms9918a* tms9918a, uint8_t y, uint8_t pixels[TMS9918A_PIXELS_X])
{
  int spriteSizePx = (tmsSpriteSize(tms9918a) ? 16 : 8) * (tmsSpriteMag(tms9918a) ? 2 : 1);
  unsigned short spriteAttrTableAddr = tmsSpriteAttrTableAddr(tms9918a);
  unsigned short spritePatternAddr = tmsSpritePatternTableAddr(tms9918a);

  int spritesShown = 0;

  if (y == 0)
  {
    tms9918a->status = 0;
  }

  for (int i = 0; i < MAX_SPRITES; ++i)
  {
    int spriteAttrAddr = spriteAttrTableAddr + i * SPRITE_ATTR_BYTES;

    int vPos = tms9918a->vram[spriteAttrAddr];

    /* stop processing when vPos == LAST_SPRITE_VPOS */
    if (vPos == LAST_SPRITE_VPOS)
    {
      if ((tms9918a->status & STATUS_5S) == 0)
      {
        tms9918a->status |= i;
      }
      break;
    }

    /* check if sprite position is in the -31 to 0 range */
    if (vPos > (uint8_t)-32)
    {
      vPos -= 256;
    }

    vPos += 1;

    int patternRow = y - vPos;
    if (tmsSpriteMag(tms9918a))
    {
      patternRow /= 2;
    }

    /* check if sprite is visible on this line */
    if (patternRow < 0 || patternRow >= (tmsSpriteSize(tms9918a) ? 16 : 8))
      continue;

    vrEmuTms9918aColor spriteColor = tms9918a->vram[spriteAttrAddr + 3] & 0x0f;

    if (spritesShown == 0)
    {
      /* if we're showing the first sprite, clear the bit buffer */
      memset(tms9918a->rowSpriteBits, 0, TMS9918A_PIXELS_X);
    }

    /* have we exceeded the scanline sprite limit? */
    if (++spritesShown > MAX_SCANLINE_SPRITES)
    {
      if ((tms9918a->status & STATUS_5S) == 0)
      {
        tms9918a->status |= STATUS_5S | i;
      }
      break;
    }

    /* sprite is visible on this line */
    uint8_t patternName = tms9918a->vram[spriteAttrAddr + 2];

    unsigned short patternOffset = spritePatternAddr + patternName * 8 + patternRow;

    int hPos = tms9918a->vram[spriteAttrAddr + 1];
    if (tms9918a->vram[spriteAttrAddr + 3] & 0x80)  /* check early clock bit */
    {
      hPos -= 32;
    }

    uint8_t patternByte = tms9918a->vram[patternOffset];

    int screenBit  = 0;
    int patternBit = 0;

    for (int screenX = hPos; screenX < (hPos + spriteSizePx); ++screenX, ++screenBit)
    {
      if (screenX >= TMS9918A_PIXELS_X)
      {
        break;
      }

      if (screenX >= 0)
      {
        if (patternByte & (0x80 >> patternBit))
        {
          /* we still process transparent sprites, since they're used in 5S and collistion checks */
          if (spriteColor != TMS_TRANSPARENT)
          {
            pixels[screenX] = spriteColor;
          }

          if (tms9918a->rowSpriteBits[screenX])
          {
            tms9918a->status |= STATUS_COL;
          }
          tms9918a->rowSpriteBits[screenX] = 1;
        }
      }

      if (!tmsSpriteMag(tms9918a) || (screenBit & 0x01))
      {
        if (++patternBit == 8) /* from A -> C or B -> D of large sprite */
        {
          patternBit = 0;
          patternByte = tms9918a->vram[patternOffset + 16];
        }
      }
    }    
  }

}


/* Function:  vrEmuTms9918aGraphicsIScanLine
 * ----------------------------------------
 * generate a Graphics I mode scanline
 */
static void vrEmuTms9918aGraphicsIScanLine(VrEmuTms9918a* tms9918a, uint8_t y, uint8_t pixels[TMS9918A_PIXELS_X])
{
  int textRow = y / 8;
  int patternRow = y % 8;

  unsigned short namesAddr = tmsNameTableAddr(tms9918a) + textRow * GRAPHICS_NUM_COLS;

  unsigned short patternBaseAddr = tmsPatternTableAddr(tms9918a);
  unsigned short colorBaseAddr = tmsColorTableAddr(tms9918a);

  int pixelIndex = -1;

  for (int tileX = 0; tileX < GRAPHICS_NUM_COLS; ++tileX)
  {
    int pattern = tms9918a->vram[namesAddr + tileX];
    
    uint8_t patternByte = tms9918a->vram[patternBaseAddr + pattern * 8 + patternRow];

    uint8_t colorByte = tms9918a->vram[colorBaseAddr + pattern / 8];

    vrEmuTms9918aColor fgColor = tmsFgColor(tms9918a, colorByte);
    vrEmuTms9918aColor bgColor = tmsBgColor(tms9918a, colorByte);

    for (int i = 0; i < GRAPHICS_CHAR_WIDTH; ++i)
    {
      pixels[++pixelIndex] = (patternByte & 0x80) ? fgColor : bgColor;
      patternByte <<= 1;
    }
  }

  vrEmuTms9918aOutputSprites(tms9918a, y, pixels);
}

/* Function:  vrEmuTms9918aGraphicsIIScanLine
 * ----------------------------------------
 * generate a Graphics II mode scanline
 */
static void vrEmuTms9918aGraphicsIIScanLine(VrEmuTms9918a* tms9918a, uint8_t y, uint8_t pixels[TMS9918A_PIXELS_X])
{
  int textRow = y / 8;
  int patternRow = y % 8;

  unsigned short namesAddr = tmsNameTableAddr(tms9918a) + textRow * GRAPHICS_NUM_COLS;

  int pageThird = (textRow & 0x18) >> 3; /* which page? 0-2 */
  int pageOffset = pageThird << 11;       /* offset (0, 0x800 or 0x1000) */

  unsigned short patternBaseAddr = tmsPatternTableAddr(tms9918a) + pageOffset;
  unsigned short colorBaseAddr = tmsColorTableAddr(tms9918a) + pageOffset;

  int pixelIndex = -1;

  for (int tileX = 0; tileX < GRAPHICS_NUM_COLS; ++tileX)
  {
    int pattern = tms9918a->vram[namesAddr + tileX];

    uint8_t patternByte = tms9918a->vram[patternBaseAddr + pattern * 8 + patternRow];
    uint8_t colorByte = tms9918a->vram[colorBaseAddr + pattern * 8 + patternRow];

    vrEmuTms9918aColor fgColor = tmsFgColor(tms9918a, colorByte);
    vrEmuTms9918aColor bgColor = tmsBgColor(tms9918a, colorByte);

    for (int i = 0; i < GRAPHICS_CHAR_WIDTH; ++i)
    {
      pixels[++pixelIndex] = (patternByte & 0x80) ? fgColor : bgColor;
      patternByte <<= 1;
    }
  }

  vrEmuTms9918aOutputSprites(tms9918a, y, pixels);
}

/* Function:  vrEmuTms9918aTextScanLine
 * ----------------------------------------
 * generate a Text mode scanline
 */
static void vrEmuTms9918aTextScanLine(VrEmuTms9918a* tms9918a, uint8_t y, uint8_t pixels[TMS9918A_PIXELS_X])
{
  int textRow = y / 8;
  int patternRow = y % 8;

  unsigned short namesAddr = tmsNameTableAddr(tms9918a) + textRow * TEXT_NUM_COLS;

  vrEmuTms9918aColor bgColor = tmsMainBgColor(tms9918a);
  vrEmuTms9918aColor fgColor = tmsMainFgColor(tms9918a);
  
  int pixelIndex = -1;

  /* fill the first 8 pixels with bg color */
  while (++pixelIndex < 8)
  {
    pixels[pixelIndex] = bgColor;
  }
  --pixelIndex;

  for (int tileX = 0; tileX < TEXT_NUM_COLS; ++tileX)
  {
    int pattern = tms9918a->vram[namesAddr + tileX];

    uint8_t patternByte = tms9918a->vram[tmsPatternTableAddr(tms9918a) + pattern * 8 + patternRow];

    uint8_t colorByte = tms9918a->vram[tmsColorTableAddr(tms9918a) + pattern / 8];

    for (int i = 0; i < TEXT_CHAR_WIDTH; ++i)
    {
      pixels[++pixelIndex] = (patternByte & 0x80) ? fgColor : bgColor;
      patternByte <<= 1;
    }
  }

  while (++pixelIndex < TMS9918A_PIXELS_X)
  {
    pixels[pixelIndex] = bgColor;
  }
}

/* Function:  vrEmuTms9918aMulticolorScanLine
 * ----------------------------------------
 * generate a Multicolor mode scanline
 */
static void vrEmuTms9918aMulticolorScanLine(VrEmuTms9918a* tms9918a, uint8_t y, uint8_t pixels[TMS9918A_PIXELS_X])
{
  int textRow = y / 8;
  int patternRow = (y / 4) % 2 + (textRow % 4) * 2;

  unsigned short namesAddr = tmsNameTableAddr(tms9918a) + textRow * GRAPHICS_NUM_COLS;

  int pixelIndex = -1;

  for (int tileX = 0; tileX < GRAPHICS_NUM_COLS; ++tileX)
  {
    int pattern = tms9918a->vram[namesAddr + tileX];

    uint8_t colorByte = tms9918a->vram[tmsPatternTableAddr(tms9918a) + pattern * 8 + patternRow];

    for (int i = 0; i < 4; ++i) pixels[++pixelIndex] = tmsFgColor(tms9918a, colorByte);
    for (int i = 0; i < 4; ++i) pixels[++pixelIndex] = tmsBgColor(tms9918a, colorByte);
  }

  vrEmuTms9918aOutputSprites(tms9918a, y, pixels);
}


/* Function:  vrEmuTms9918aScanLine
 * ----------------------------------------
 * generate a scanline
 */
VR_EMU_TMS9918A_DLLEXPORT void vrEmuTms9918aScanLine(VrEmuTms9918a* tms9918a, uint8_t y, uint8_t pixels[TMS9918A_PIXELS_X])
{
  if (tms9918a == NULL)
    return;

  if (!vrEmuTms9918aDisplayEnabled(tms9918a) || y >= TMS9918A_PIXELS_Y)
  {
    memset(pixels, tmsMainBgColor(tms9918a), TMS9918A_PIXELS_X);
    return;
  }

  switch (tms9918a->mode)
  {
    case TMS_MODE_GRAPHICS_I:
      vrEmuTms9918aGraphicsIScanLine(tms9918a, y, pixels);
      break;

    case TMS_MODE_GRAPHICS_II:
      vrEmuTms9918aGraphicsIIScanLine(tms9918a, y, pixels);
      break;

    case TMS_MODE_TEXT:
      vrEmuTms9918aTextScanLine(tms9918a, y, pixels);
      break;

    case TMS_MODE_MULTICOLOR:
      vrEmuTms9918aMulticolorScanLine(tms9918a, y, pixels);
      break;
  }

  if (y == TMS9918A_PIXELS_Y - 1)
  {
    tms9918a->status |= STATUS_INT;
  }
}

/* Function:  vrEmuTms9918aRegValue
 * ----------------------------------------
 * return a reigister value
 */
VR_EMU_TMS9918A_DLLEXPORT
uint8_t vrEmuTms9918aRegValue(VrEmuTms9918a * tms9918a, vrEmuTms9918aRegister reg)
{
  if (tms9918a == NULL)
    return 0;

  return tms9918a->registers[reg & 0x07];
}

/* Function:  vrEmuTms9918aVramValue
 * ----------------------------------------
 * return a value from vram
 */
VR_EMU_TMS9918A_DLLEXPORT
uint8_t vrEmuTms9918aVramValue(VrEmuTms9918a* tms9918a, unsigned short addr)
{
  if (tms9918a == NULL)
    return 0;

  return tms9918a->vram[addr & 0x3fff];
}

/* Function:  vrEmuTms9918aDisplayEnabled
  * --------------------
  * check BLANK flag
  */
VR_EMU_TMS9918A_DLLEXPORT
int vrEmuTms9918aDisplayEnabled(VrEmuTms9918a* tms9918a)
{
  if (tms9918a == NULL)
    return 0;

  return (tms9918a->registers[TMS_REG_1] & 0x40) ? 1 : 0;
}
