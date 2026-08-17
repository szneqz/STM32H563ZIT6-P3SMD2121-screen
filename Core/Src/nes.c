/*
 * nes.c
 *
 *  Created on: Aug 2, 2026
 *      Author: mishcat
 */

#include <string.h>

#include "nes.h"
#include "nes_data.h"

#ifndef PC_DEBUG
#include "main_logic.h"
#include "hub75_ospi.h"

extern uint8_t HUB75_current_draw_frame;
extern ColorBitfield HUB75_s_framebuf[2][HUB75_PANEL_HEIGHT][HUB75_PANEL_WIDTH];
#endif

uint32_t CPU_Time;
uint32_t PPU_Time;
uint32_t Logic_Time;
uint32_t Draw_Time;
uint32_t Frame_Time;
uint32_t tempTime;
uint32_t tempTime2;
uint32_t tempTime3;

uint8_t PRGSIZE;
uint8_t PRGSIZEMASK;
const uint8_t *BASEPRGADDRESS;
uint8_t CHRSIZE;
uint8_t CHRSIZEMASK;
uint8_t *BASECHRADDRESS;
uint8_t MAPPER;
bool MIRRORING;

//uint16_t DisplayBuffer[30][64]; // y x
//uint8_t NESBuffer[241][256][3]; // line dot rgb
uint16_t NESBufferFast[32][64][3]; // line dot rgb
//uint8_t EventBuffer[262][342][3]; // line dot rgb
bool DrawNewFrame = false;

uint16_t ProgramCounter;
uint8_t NES_A;
uint8_t NES_X;
uint8_t NES_Y;
uint8_t StackPointer = 0xFD;

uint32_t Temp;
uint32_t Temp_High;
uint32_t Temp_Low;
int32_t SignedTemp;
uint8_t SmallTemp;

uint16_t addressBus;
uint8_t opcode;
uint8_t OpenBus;

 uint8_t selectedROM = ROM_SuperMario;
// uint8_t selectedROM = ROM_Mapperless;
// uint8_t selectedROM = ROM_WaifuMaterial;
// uint8_t selectedROM = ROM_AccuracyCoinStary;
// uint8_t selectedROM = ROM_AccuracyCoinNowy;
//uint8_t selectedROM = ROM_Contra;
// uint8_t selectedROM = ROM_MicroMachines;

typedef union {
    struct {
        bool Carry : 1;
        bool Zero : 1;
        bool InterruptDisable : 1;
        bool Decimal : 1;
        bool Break : 1;
        bool Expansion : 1;
        bool Overflow : 1;
        bool Negative : 1;
    };
    uint8_t Raw;
} FlagsBitfield;

FlagsBitfield Flags;

bool CPU_Halted = false;
uint32_t Cycles = 0;
uint8_t CyclesLeft = 0;
uint32_t AddOAMCycles = 0;
uint8_t AddDMCCycles = 0;
bool AddAlignmentCycle = false;

uint8_t RAM[0x800];
uint8_t CHRRAM[0x2000];
uint8_t *CHRData;
const uint8_t *PRG0Data;
const uint8_t *PRG1Data;
const uint8_t *HeaderData;

uint16_t ppuDot = 0;
uint16_t ppuScanline = 0;
uint16_t ppuAddressBus;

uint16_t ppuFastDot = 0;
uint16_t ppuFastScanline = 0;

uint8_t VRAM[0x800];
uint8_t PaletteRAM[0x20];
uint8_t OAM[0x100];
uint8_t SecondaryOAM[0x20];

uint16_t ppuShiftRegister_patternL;
uint16_t ppuShiftRegister_patternH;
uint16_t ppuShiftRegister_attributeL;
uint16_t ppuShiftRegister_attributeH;
uint8_t ppu8Step_patternLowBitPlane;
uint8_t ppu8Step_patternHighBitPlane;
uint8_t ppu8Step_attribute;
uint8_t ppu8Step_temp;
uint8_t ppu8Step_NextCharacter;

uint8_t ppuSecondaryOAMAddress;
bool ppuSecondaryOAMFull;
uint8_t ppuSpriteEvalTemp;
uint8_t ppuOAMAddress;
bool ppuSpriteEvaluationOAMOverflowed;
uint8_t ppuSpriteEvalTick;
bool ppuNextScanlineContainsSpriteZero;
bool ppuCurrentScanlineContainsSpriteZero;
uint8_t ppuSecondaryOAMSize;

uint8_t ppu_SpriteShiftRegisterL[8];
uint8_t ppu_SpriteShiftRegisterH[8];

uint8_t ppu_SpriteAttribute[8];
uint8_t ppu_SpritePattern[8];
int16_t ppu_SpriteXposition[8];
int16_t ppu_SpriteYposition[8];

typedef union {
    struct {
        uint8_t Nametable : 2;
        bool Increment : 1;
        bool SpriteTable : 1;
        bool BackgroundTable : 1;
        bool SpriteSize : 1;
        bool Master : 1;
        bool NMI : 1;
    };
    uint8_t Raw;
} PPUCTRLBitfield;

PPUCTRLBitfield PPUCTRL;

typedef union {
    struct {
        bool Greyscale : 1;
        bool ShowBackgroundColumn : 1;
        bool ShowSpriteColumn : 1;
        bool EnableBackground : 1;
        bool EnableSprites : 1;
        bool EmphasizeRed : 1;
        bool EmphasizeGreen : 1;
        bool EmphasizeBlue : 1;
    };
    uint8_t Raw;
} PPUMASKBitfield;

PPUMASKBitfield PPUMASK;

typedef union {
    struct {
        uint8_t OpenBus : 5;
        bool SpriteOverflow : 1;
        bool Sprite0Hit : 1;
        bool VBlank : 1;
    };
    uint8_t Raw;
} PPUSTATUSBitfield;

PPUSTATUSBitfield PPUSTATUS;

uint8_t OAMADDR;
uint8_t OAMDATA;
uint8_t PPUSCROLLX;
uint8_t PPUSCROLLY;
uint8_t ppuScrollFineX;

bool WriteLatch; // PPU w
uint16_t TransferAddress; // PPU t
uint16_t VRAMAddress; // PPU v
uint8_t PPUReadBuffer;

bool NMILevelDetector;
bool DoNMI;
// bool DMCIRQenabled;
bool DoBRK;

uint32_t frameCounter;

uint8_t Controller1Buttons = 0b00000000;
uint8_t Controller2Buttons = 0b00000000;;
uint8_t Controller1ShiftRegister;
uint8_t Controller2ShiftRegister;

uint32_t APUDMC_BytesRemaining;

typedef union {
    struct {
        bool EnablePulse1 : 1;
        bool EnablePulse2 : 1;
        bool EnableTriangle : 1;
        bool EnableNoise : 1;
        bool EnableDMC : 1;
        bool OpenBus : 1;
        bool FrameInterrupt : 1;
        bool DMCInterrupt : 1;
    };
    uint8_t Raw;
} APUSTATUSBitfield;

APUSTATUSBitfield APUSTATUS;

typedef union {
    struct {
        uint8_t RateIndex : 4;
        uint8_t OpenBus : 2;
        bool Loop : 1;
        bool EnableIRQ : 1;
    };
    uint8_t Raw;
} DMCFLAGSBitfield;

DMCFLAGSBitfield DMCFLAGS;

uint8_t DMCDirectLoad;
uint8_t DMCSampleAddress;
uint32_t DMCSampleLength;
uint32_t DMCCycleCount;
uint8_t DMCLoadDelay;
bool DMCBytesRemainingWriteRequest;
bool DMCBytesRemainingReloadRequest;

const uint16_t DMCCycleTable[] = {428, 380, 340, 320, 286, 254, 226, 214, 190, 160, 142, 128, 106,  84,  72,  54};

typedef union {
    struct {
        uint8_t OpenBus : 6;
        bool InterruptInhibit : 1;
        bool SequencerMode : 1;
    };
    uint8_t Raw;
} APUCOUNTERBitfield;

APUCOUNTERBitfield APUCOUNTER;

uint32_t APUFrameCounter;

#ifdef PC_DEBUG
void NES_DebugVRAM() {
    for (uint8_t row = 0; row < 30; row++) {
        for (uint8_t column = 0; column < 32; column++) {

            uint8_t attributeOffset = (uint8_t)((column >> 2) + (row >> 2) * 8);
            uint8_t Attributes = VRAM[0x3C0 + attributeOffset];
            uint8_t Quadrant = (uint8_t)(((column >> 1) & 1) + ((row >> 1) & 1) * 2);
            uint8_t Pair = (uint8_t)((Attributes >> (Quadrant * 2)) & 3);

            for (uint8_t y = 0; y < 8; y++) {
                uint16_t useSecondPatternTable = PPUCTRL.BackgroundTable ? 4096 : 0;
                uint8_t lowByte = CHRData[VRAM[column + row * 32] * 16 + y + useSecondPatternTable];
                uint8_t highByte = CHRData[VRAM[column + row * 32] * 16 + 8 + y + useSecondPatternTable];
                for (uint8_t x = 0; x < 8; x++) {
                    uint8_t TwoBit = ((lowByte >> (7 - x)) & 1) == 1 ? 1 : 0;
                    TwoBit += ((highByte >> (7 - x)) & 1) == 1 ? 2 : 0;
                    if (TwoBit == 0) {
                        NESBuffer[y + row * 8][x + column * 8][0] = Palette[PaletteRAM[0]][0];
                        NESBuffer[y + row * 8][x + column * 8][1] = Palette[PaletteRAM[0]][1];
                        NESBuffer[y + row * 8][x + column * 8][2] = Palette[PaletteRAM[0]][2];
                    } else {
                        NESBuffer[y + row * 8][x + column * 8][0] = Palette[PaletteRAM[TwoBit + Pair * 4]][0];
                        NESBuffer[y + row * 8][x + column * 8][1] = Palette[PaletteRAM[TwoBit + Pair * 4]][1];
                        NESBuffer[y + row * 8][x + column * 8][2] = Palette[PaletteRAM[TwoBit + Pair * 4]][2];
                    }
                }
            }
        }
    }
}
#endif

#ifndef PC_DEBUG
	#ifndef NES_FAST
void NES_PrepareDisplay() {
    for (uint8_t j = 0; j < 30; j++) {
        for (uint8_t i = 0; i < 64; i++) {
//            DisplayBuffer[j][i] = 0;
            uint32_t tempRGB[3] = {0, 0, 0};
            for (uint8_t q = 0; q < 8; q++) {
                for (uint8_t p = 0; p < 4; p++) {
                    tempRGB[0] += NESBuffer[(j*8)+q][(i*4)+p][0];
                    tempRGB[1] += NESBuffer[(j*8)+q][(i*4)+p][1];
                    tempRGB[2] += NESBuffer[(j*8)+q][(i*4)+p][2];
                }
            }
            tempRGB[0] /= 4*8;
            tempRGB[0] >>= 3;
            tempRGB[1] /= 4*8;
            tempRGB[1] >>= 3;
            tempRGB[2] /= 4*8;
            tempRGB[2] >>= 3;
            HUB75_s_framebuf[HUB75_current_draw_frame][j][i].color = (tempRGB[0] << 10) | (tempRGB[1] << 5) | (tempRGB[2] << 0);
        }
    }
}
	#endif

void NES_PrepareDisplayFast() {
    for (uint8_t j = 0; j < 30; j++) {
    	NESBufferFast[j][0][0] = 0;
    	NESBufferFast[j][0][1] = 0;
    	NESBufferFast[j][0][2] = 0;
        for (uint8_t i = 0; i < 64; i++) {
            NESBufferFast[j][i][0] >>= 6; // div 8 and move to 5 bits
            NESBufferFast[j][i][1] >>= 6;
            NESBufferFast[j][i][2] >>= 6;
            HUB75_s_framebuf[HUB75_current_draw_frame][j][i].color = (NESBufferFast[j][i][0] << 10) | (NESBufferFast[j][i][1] << 5) | (NESBufferFast[j][i][2] << 0);
            NESBufferFast[j][i][0] = 0;
            NESBufferFast[j][i][1] = 0;
            NESBufferFast[j][i][2] = 0;
        }
    }
}

#else

void NES_PrepareDisplay() {
    for (uint8_t j = 0; j < 30; j++) {
        for (uint8_t i = 0; i < 64; i++) {
            DisplayBuffer[j][i] = 0;
            uint32_t tempRGB[3] = {0, 0, 0};
            for (uint8_t q = 0; q < 8; q++) {
                for (uint8_t p = 0; p < 4; p++) {
                    tempRGB[0] += NESBuffer[(j*8)+q][(i*4)+p][0];
                    tempRGB[1] += NESBuffer[(j*8)+q][(i*4)+p][1];
                    tempRGB[2] += NESBuffer[(j*8)+q][(i*4)+p][2];
                }
            }
            tempRGB[0] /= 4*8;
            tempRGB[0] >>= 3;
            tempRGB[1] /= 4*8;
            tempRGB[1] >>= 3;
            tempRGB[2] /= 4*8;
            tempRGB[2] >>= 3;
            DisplayBuffer[j][i] = (tempRGB[2] << 10) | (tempRGB[1] << 5) | (tempRGB[0] << 0);
        }
    }
}

void NES_PrepareDisplayFast() {
    for (uint8_t j = 0; j < 30; j++) {
        for (uint8_t i = 0; i < 64; i++) {
            // DisplayBuffer[j][i] = 0;
            // uint32_t tempRGB[3] = {0, 0, 0};
            // for (uint8_t q = 0; q < 8; q++) {
            //     for (uint8_t p = 0; p < 4; p++) {
            //         tempRGB[0] += NESBuffer[(j*8)+q][(i*4)+p][0];
            //         tempRGB[1] += NESBuffer[(j*8)+q][(i*4)+p][1];
            //         tempRGB[2] += NESBuffer[(j*8)+q][(i*4)+p][2];
            //     }
            // }
            NESBufferFast[j][i][0] >>= 6; // div 8 and move to 5 bits
            NESBufferFast[j][i][1] >>= 6;
            NESBufferFast[j][i][2] >>= 6;
            DisplayBuffer[j][i] = (NESBufferFast[j][i][2] << 10) | (NESBufferFast[j][i][1] << 5) | (NESBufferFast[j][i][0] << 0);
            NESBufferFast[j][i][0] = 0;
            NESBufferFast[j][i][1] = 0;
            NESBufferFast[j][i][2] = 0;
        }
    }
}

// ---- Generic 24-bit BMP writer -------------------------------------------
// rgb: row-major, top-to-bottom, 3 bytes (R,G,B) per pixel.
static void WriteBMP(const char* filename, int width, int height, const uint8_t* rgb) {
   int rowSize  = (width * 3 + 3) & ~3;      // rows padded to 4-byte boundary
   int dataSize = rowSize * height;
   int fileSize = 54 + dataSize;

   uint8_t header[54] = {0};
   header[0] = 'B'; header[1] = 'M';
   header[2] = (uint8_t)(fileSize);       header[3] = (uint8_t)(fileSize >> 8);
   header[4] = (uint8_t)(fileSize >> 16); header[5] = (uint8_t)(fileSize >> 24);
   header[10] = 54;                       // pixel data offset
   header[14] = 40;                       // BITMAPINFOHEADER size
   header[18] = (uint8_t)(width);         header[19] = (uint8_t)(width >> 8);
   header[20] = (uint8_t)(width >> 16);   header[21] = (uint8_t)(width >> 24);
   header[22] = (uint8_t)(height);        header[23] = (uint8_t)(height >> 8);
   header[24] = (uint8_t)(height >> 16);  header[25] = (uint8_t)(height >> 24);
   header[26] = 1;                        // planes
   header[28] = 24;                       // bits per pixel
   header[34] = (uint8_t)(dataSize);      header[35] = (uint8_t)(dataSize >> 8);
   header[36] = (uint8_t)(dataSize >> 16);header[37] = (uint8_t)(dataSize >> 24);

   FILE* f = fopen(filename, "wb");
   if (!f) { perror("WriteBMP: fopen"); return; }
   fwrite(header, 1, 54, f);

   uint8_t* row = malloc((size_t)rowSize);
   memset(row, 0, (size_t)rowSize);

   for (int y = height - 1; y >= 0; y--) {          // BMP stores bottom-to-top
       for (int x = 0; x < width; x++) {
           const uint8_t* px = &rgb[((size_t)y * width + x) * 3];
           row[x*3 + 0] = px[2];                     // B
           row[x*3 + 1] = px[1];                     // G
           row[x*3 + 2] = px[0];                     // R
       }
       fwrite(row, 1, (size_t)rowSize, f);
   }
   free(row);
   fclose(f);
}

// ---- Export NESBuffer (256x240, already RGB888) ---------------------------
void SaveNESBufferBMP(const char* filename) {
   WriteBMP(filename, 256, 240, (const uint8_t*)NESBuffer);
}

void SaveNESBufferBMPNumbered(const char* base, uint32_t number) {
    char filename[256];
    snprintf(filename, sizeof(filename), "%s%u.bmp", base, number);
    SaveNESBufferBMP(filename);
}

// ---- Export EventBuffer (256x240, already RGB888) ---------------------------
void SaveEventBufferBMP(const char* filename) {
    WriteBMP(filename, 342, 262, (const uint8_t*)EventBuffer);
}

void SaveEventBufferBMPNumbered(const char* base, uint32_t number) {
    char filename[256];
    snprintf(filename, sizeof(filename), "%s%u.bmp", base, number);
    SaveEventBufferBMP(filename);
}

// ---- Merge NESBuffer (bottom layer) + EventBuffer (top layer, black = transparent) ----
// Canvas is sized to fit the larger buffer (EventBuffer: 342x262).
// NESBuffer (256x240) is placed at top-left as the base layer.
// EventBuffer (342x262) is placed at top-left on top, with black pixels (0,0,0)
// treated as transparent so the NES image shows through.
static uint8_t* MergeNESAndEventBuffers(int* outWidth, int* outHeight) {
    const int nesW = 256, nesH = 240;
    const int evtW = 342, evtH = 262;

    int width  = (evtW > nesW) ? evtW : nesW;   // 342
    int height = (evtH > nesH) ? evtH : nesH;   // 262

    uint8_t* merged = (uint8_t*)malloc((size_t)width * height * 3);
    if (!merged) return NULL;
    memset(merged, 0, (size_t)width * height * 3); // black background

    // --- Bottom layer: NESBuffer, top-left aligned ---
    const uint8_t* nes = (const uint8_t*)NESBuffer;
    for (int y = 0; y < nesH; y++) {
        for (int x = 0; x < nesW; x++) {
            const uint8_t* srcPx = &nes[((size_t)y * nesW + x) * 3];
            uint8_t* dstPx = &merged[((size_t)y * width + x) * 3];
            dstPx[0] = srcPx[0];
            dstPx[1] = srcPx[1];
            dstPx[2] = srcPx[2];
        }
    }

    // --- Top layer: EventBuffer, top-left aligned, black = transparent ---
    const uint8_t* evt = (const uint8_t*)EventBuffer;
    for (int y = 0; y < evtH; y++) {
        for (int x = 0; x < evtW; x++) {
            const uint8_t* srcPx = &evt[((size_t)y * evtW + x) * 3];
            if (srcPx[0] == 0 && srcPx[1] == 0 && srcPx[2] == 0)
                continue; // transparent, let NES buffer (or background) show through

                uint8_t* dstPx = &merged[((size_t)y * width + x) * 3];
            dstPx[0] = srcPx[0];
            dstPx[1] = srcPx[1];
            dstPx[2] = srcPx[2];
        }
    }

    *outWidth  = width;
    *outHeight = height;
    return merged;
}

// ---- Export merged buffer ---------------------------
void SaveMergedBufferBMP(const char* filename) {
    int w, h;
    uint8_t* merged = MergeNESAndEventBuffers(&w, &h);
    if (!merged) { fprintf(stderr, "SaveMergedBufferBMP: allocation failed\n"); return; }
    WriteBMP(filename, w, h, merged);
    free(merged);
}

void SaveMergedBufferBMPNumbered(const char* base, uint32_t number) {
    char filename[256];
    snprintf(filename, sizeof(filename), "%s%u.bmp", base, number);
    SaveMergedBufferBMP(filename);
}

// ---- Export DisplayBuffer (0b0bbbbbgggggrrrrr, 5 bits/channel) ------------
void SaveDisplayBufferBMP(const char* filename) {
   const int width  = 64;   // matches DisplayBuffer[30][64] -> x
   const int height = 30;   // matches DisplayBuffer[30][64] -> y

   uint8_t* rgb = malloc((size_t)width * height * 3);
   for (int y = 0; y < height; y++) {
       for (int x = 0; x < width; x++) {
           uint16_t c = DisplayBuffer[y][x];
           uint8_t r5 = (uint8_t)(c & 0x1F);
           uint8_t g5 = (uint8_t)((c >> 5) & 0x1F);
           uint8_t b5 = (uint8_t)((c >> 10) & 0x1F);

           // 5-bit -> 8-bit, spreading the low bits for a smoother scale
           uint8_t r = (uint8_t)((r5 << 3) | (r5 >> 2));
           uint8_t g = (uint8_t)((g5 << 3) | (g5 >> 2));
           uint8_t b = (uint8_t)((b5 << 3) | (b5 >> 2));

           size_t idx = ((size_t)y * width + x) * 3;
           rgb[idx + 0] = r;
           rgb[idx + 1] = g;
           rgb[idx + 2] = b;
       }
   }
   WriteBMP(filename, width, height, rgb);
   free(rgb);
}
#endif

uint8_t ReadPPU(uint16_t Address) {
    if (Address < 0x2000) {
        return CHRData[Address];
    }
    else if (Address < 0x3F00) {
        if (!MIRRORING) {
            return VRAM[(Address & 0x3FF) | (Address & 0x800) >> 1];
        } else {
            return VRAM[Address & 0x7FF];
        }
    }
    else {
        if ((Address & 3) == 0) {
            return PaletteRAM[Address & 0x0F];
        } else {
            return PaletteRAM[Address & 0x1F];
        }
    }
}

uint8_t Read(uint16_t Address) {
    if (Address < 0x2000) {
        return RAM[Address & 0x7FF];
    }
    else if (Address < 0x4000) {
        Address &= 0x07;
//        EventBuffer[ppuScanline][ppuDot][0] = 0x00;
//        EventBuffer[ppuScanline][ppuDot][1] = 0xFF;
//        EventBuffer[ppuScanline][ppuDot][2] = 0x00;
        switch (Address) {
            case 0x02:
                SmallTemp = PPUSTATUS.Raw;
                PPUSTATUS.VBlank = false;
                WriteLatch = false;
                // if (PPUSTATUS.Sprite0Hit) printf(" %d G ", ppuScanline);
                return SmallTemp;
            case 0x07:
                SmallTemp = PPUReadBuffer;
                if (VRAMAddress > 0x3F00) {
                    SmallTemp = ReadPPU(VRAMAddress);
                } else {
                    PPUReadBuffer = ReadPPU(VRAMAddress);
                }
                VRAMAddress += (uint16_t)(PPUCTRL.Increment ? 32 : 1);
                VRAMAddress &= 0x3FFF;
                return SmallTemp;
            default:
                return 0;
        }
    }
    else if (Address == 0x4015) {
//        printf(" Read $%x\n",Address);
        APUSTATUS.EnableDMC = APUDMC_BytesRemaining > 0;
        SmallTemp = APUSTATUS.Raw;
        APUSTATUS.FrameInterrupt = false;
        // EventBuffer[ppuScanline][ppuDot][0] = 0xFF;
        // EventBuffer[ppuScanline][ppuDot][1] = 0x00;
        // EventBuffer[ppuScanline][ppuDot][2] = 0x00;
        return SmallTemp;
    }
    else if (Address == 0x4016) {
        uint8_t controllerBit = (uint8_t)(Controller1ShiftRegister & 0x01);
        Controller1ShiftRegister >>= 1;
        Controller1ShiftRegister |= 0x80;
        return controllerBit;
    }
    else if (Address == 0x4017) {
        uint8_t controllerBit = (uint8_t)(Controller2ShiftRegister & 0x01);
        Controller2ShiftRegister >>= 1;
        Controller2ShiftRegister |= 0x80;
        return controllerBit;
    }
    else if (Address >= 0xC000) {
        // printf("%x: %x\n", Address - 0xC000, PRG1Data[Address - 0xC000]);
        return PRG1Data[Address - 0xC000];
    }
    else if (Address >= 0x8000) {
        return PRG0Data[Address - 0x8000];
    }
//    printf(" Read %x\n", Address);
    return 0;
}

void Write(uint16_t Address, uint8_t Data) {
    if (Address < 0x2000) {
        RAM[Address & 0x7FF] = Data;
    }
    else if (Address < 0x4000) {
        Address &= 0x7;
//        EventBuffer[ppuScanline][ppuDot][0] = 0xFF;
//        EventBuffer[ppuScanline][ppuDot][1] = 0x00;
//        EventBuffer[ppuScanline][ppuDot][2] = 0x00;

        switch (Address) {
            case 0x00: // PPUCTRL
                PPUCTRL.Raw = Data;
                TransferAddress = (uint16_t)((TransferAddress & 0xF3FF) | ((uint16_t)(Data & 0x03) << 10));
                break;

            case 0x01: // PPUMASK
                PPUMASK.Raw = Data;
                break;

            case 0x02: // PPUSTATUS
                break;

            case 0x03: // OAMADDR
                break;

            case 0x04: // OAMDATA
                break;

            case 0x05: // PPUSCROLL
                if (!WriteLatch) {
                    ppuScrollFineX = (uint8_t)(Data & 7);
                    TransferAddress = (uint16_t)((TransferAddress & 0b0111111111100000) | (Data >> 3));
                } else {
                    TransferAddress = (uint16_t)((TransferAddress & 0b0000110000011111) | ((Data & 0xF8) << 2) | ((Data & 7) << 12));
                }
                WriteLatch = !WriteLatch;
                // printf("%x %x, ", Data, PPUSTATUS.Sprite0Hit);
                break;

            case 0x06: // PPUADDR
                if (!WriteLatch) {
                    TransferAddress = (uint16_t)((TransferAddress & 0x00FF) | ((Data & 0x3F) << 8));
                } else {
                    TransferAddress = (uint16_t)((TransferAddress & 0xFF00) | Data);
                    VRAMAddress = TransferAddress;
                }
                WriteLatch = !WriteLatch;
                break;

            case 0x07: // PPUDATA
                if (VRAMAddress < 0x2000) { // Pattern Table
                    if (CHRSIZE == 0) {
                        CHRData[VRAMAddress] = Data;
                    }
                }
                else if (VRAMAddress < 0x3F00) { // Nametables
                    if (!MIRRORING) { // horizontal mirroring
                        VRAM[(VRAMAddress & 0x3FF) | (VRAMAddress & 0x800) >> 1] = Data;
                    } else { // vertical mirroring
                        VRAM[VRAMAddress & 0x7FF] = Data;
                    }
                }
                else { // Palette RAM
                    if ((VRAMAddress & 3) == 0) {
                        PaletteRAM[VRAMAddress & 0x0F] = Data;
                    } else {
                        PaletteRAM[VRAMAddress & 0x1F] = Data;
                    }
                }

                VRAMAddress += (uint16_t)(PPUCTRL.Increment ? 32 : 1);
                VRAMAddress &= 0x3FFF;
                break;
        }
    }
    else if (Address == 0x4010) {
//        printf("\nBytesRemaining: %x\tAPUSTATUS: %x\tSampleLength: %x\tCycleCount: %x\tDMCFLAGS: %x\n", APUDMC_BytesRemaining, APUSTATUS, DMCSampleLength, DMCCycleCount, DMCFLAGS);
        DMCFLAGS.Raw = Data & 0b11001111;
        DMCCycleCount = DMCCycleTable[DMCFLAGS.RateIndex] << 3; // x 8 bits
        if (DMCFLAGS.EnableIRQ == false) {
            APUSTATUS.DMCInterrupt = false;
            // APUSTATUS.FrameInterrupt = false;
        }
        // DMCIRQenabled = (Data & 0b10000000) > 0;
//        printf(" Write $%x : %x\n",Address, Data);
//        printf("BytesRemaining: %x\tAPUSTATUS: %x\tSampleLength: %x\tCycleCount: %x\tDMCFLAGS: %x\n", APUDMC_BytesRemaining, APUSTATUS, DMCSampleLength, DMCCycleCount, DMCFLAGS);
//        EventBuffer[ppuScanline][ppuDot][0] = 0xFF;
//        EventBuffer[ppuScanline][ppuDot][1] = 0x00;
//        EventBuffer[ppuScanline][ppuDot][2] = 0xFF;
    }
    // else if (Address == 0x4011) {
    //     DMCDirectLoad = Data & 0b01111111;
    //     // printf(" Write $%x : %x\n",Address, Data);
    // }
    // else if (Address == 0x4012) {
    //     DMCSampleAddress = Data;
    //     // printf(" Write $%x : %x\n",Address, Data);
    // }
    else if (Address == 0x4013) {
//        printf("\nBytesRemaining: %x\tAPUSTATUS: %x\tSampleLength: %x\tCycleCount: %x\tDMCFLAGS: %x\n", APUDMC_BytesRemaining, APUSTATUS, DMCSampleLength, DMCCycleCount, DMCFLAGS);
        DMCSampleLength = (Data << 4) | 1;
        // DMCSampleLength = Data;
        // APUDMC_BytesRemaining = DMCSampleLength;
//        printf(" Write $%x : %x\n",Address, Data);
//        printf("BytesRemaining: %x\tAPUSTATUS: %x\tSampleLength: %x\tCycleCount: %x\tDMCFLAGS: %x\n", APUDMC_BytesRemaining, APUSTATUS, DMCSampleLength, DMCCycleCount, DMCFLAGS);
//        EventBuffer[ppuScanline][ppuDot][0] = 0xFF;
//        EventBuffer[ppuScanline][ppuDot][1] = 0x00;
//        EventBuffer[ppuScanline][ppuDot][2] = 0xFF;
    }
    else if (Address == 0x4015) {
//        printf("\nBytesRemaining: %x\tAPUSTATUS: %x\tSampleLength: %x\tCycleCount: %x\tDMCFLAGS: %x\n", APUDMC_BytesRemaining, APUSTATUS, DMCSampleLength, DMCCycleCount, DMCFLAGS);
//        printf(" Write $%x : %x\n",Address, Data);
        APUSTATUS.Raw = Data & 0b00010000;
        APUSTATUS.DMCInterrupt = false;
        if (APUSTATUS.EnableDMC && APUDMC_BytesRemaining == 0) {
            // DMCLoadDelay = 6 + AddAlignmentCycle;
            // if (DMCLoadDelay == 0) DMCLoadDelay = 4 - AddAlignmentCycle;
            if (DMCLoadDelay == 0) DMCLoadDelay = 3 + AddAlignmentCycle;
            // if (DMCLoadDelay == 0) DMCLoadDelay = 1;
            DMCBytesRemainingWriteRequest = true;
            // if (DMCLoadDelay > (4 - AddAlignmentCycle)) {
            //     printf(" BIG DELAY %x\n", DMCLoadDelay);
            //     fflush(stdin);
            //     getc(stdin);
            // }
            // APUDMC_BytesRemaining = DMCSampleLength;
            // DMCCycleCount = (DMCCycleTable[DMCFLAGS.RateIndex] << 3); // x 8 bits
        }
        if (APUSTATUS.EnableDMC == false) {
            APUDMC_BytesRemaining = 0;
            DMCCycleCount = 0;
        }
//        printf("BytesRemaining: %x\tAPUSTATUS: %x\tSampleLength: %x\tCycleCount: %x\tDMCFLAGS: %x\n", APUDMC_BytesRemaining, APUSTATUS, DMCSampleLength, DMCCycleCount, DMCFLAGS);
//        EventBuffer[ppuScanline][ppuDot][0] = 0xFF;
//        EventBuffer[ppuScanline][ppuDot][1] = 0x00;
//        EventBuffer[ppuScanline][ppuDot][2] = 0xFF;
    }
    else if (Address == 0x4017) {
//        printf("\nBytesRemaining: %x\tAPUSTATUS: %x\tSampleLength: %x\tCycleCount: %x\tDMCFLAGS: %x\n", APUDMC_BytesRemaining, APUSTATUS, DMCSampleLength, DMCCycleCount, DMCFLAGS);
        APUCOUNTER.Raw = Data & 0b11000000;
        if (Data & 0b01000000) {
            APUSTATUS.FrameInterrupt = false;
        }
        APUFrameCounter = 0;
//        printf(" Write $%x : %x\n",Address, Data);
//        printf("BytesRemaining: %x\tAPUSTATUS: %x\tSampleLength: %x\tCycleCount: %x\tDMCFLAGS: %x\n", APUDMC_BytesRemaining, APUSTATUS, DMCSampleLength, DMCCycleCount, DMCFLAGS);
        // fflush(stdin);
        // getc(stdin);
    }
    else if (Address == 0x4014) {
        for (uint16_t i = 0; i < 256; i++) {
            OAM[i] = Read((uint16_t)((Data << 8) + i));
        }
        AddOAMCycles = 513 + AddAlignmentCycle;
        // printf(" OAM DMA Write $%x : %x\n",Address, Data);
    }
    else if (Address == 0x4016) {
        Controller1ShiftRegister = Controller1Buttons;
        Controller2ShiftRegister = Controller2Buttons;
    }
    // else if (Address >= 0x6000 && Address < 0x8000) {
    //     printf("%c", Data);
    // }
    else if (Address >= 0x8000) {
//        EventBuffer[ppuScanline][ppuDot][0] = 0x00;
//        EventBuffer[ppuScanline][ppuDot][1] = 0x00;
//        EventBuffer[ppuScanline][ppuDot][2] = 0xFF;
        if (MAPPER == 3) {
            CHRData = BASECHRADDRESS + (0x2000 * (Data & CHRSIZEMASK));
        }
        else if (MAPPER == 2 || MAPPER == 71) {
            PRG0Data = BASEPRGADDRESS + (0x4000 * (Data & PRGSIZEMASK));
        }
    }
}

void Push(uint8_t Value) {
    Write((uint16_t)(0x100 + StackPointer), Value);
    StackPointer--;
}

uint8_t Pull() {
    StackPointer++;
    return Read((uint16_t)(0x100 + StackPointer));
}

void ReadOperands_ZeroPageAddressed() {
    addressBus = Read(ProgramCounter);
    ProgramCounter++;
}

void ReadOperands_ZeroPageAddressed_XIndexed() {
    addressBus = Read(ProgramCounter);
    ProgramCounter++;
    addressBus += NES_X;
    addressBus &= 0xFF;
}

void ReadOperands_ZeroPageAddressed_YIndexed() {
    addressBus = Read(ProgramCounter);
    ProgramCounter++;
    addressBus += NES_Y;
    addressBus &= 0xFF;
}

void ReadOperands_AbsoluteAddressed() {
    addressBus = Read(ProgramCounter);
    ProgramCounter++;
    addressBus = (uint16_t)(Read(ProgramCounter) << 8 | addressBus);
    ProgramCounter++;
}

void ReadOperands_AbsoluteAddressed_XIndexed() {
    addressBus = Read(ProgramCounter);
    ProgramCounter++;
    addressBus = (uint16_t)(Read(ProgramCounter) << 8 | addressBus);
    ProgramCounter++;
    addressBus += NES_X;
}

void ReadOperands_AbsoluteAddressed_YIndexed() {
    addressBus = Read(ProgramCounter);
    ProgramCounter++;
    addressBus = (uint16_t)(Read(ProgramCounter) << 8 | addressBus);
    ProgramCounter++;
    addressBus += NES_Y;
}

void ReadOperands_IndirectAddressed_XIndexed() {
    addressBus = (uint8_t)(Read(ProgramCounter) + NES_X);
    ProgramCounter++;
    uint8_t TempAddress = (uint8_t)(addressBus);
    addressBus = Read(TempAddress);
    TempAddress++;
    addressBus = (uint16_t)(Read(TempAddress) << 8 | addressBus);
}

void ReadOperands_IndirectAddressed_YIndexed() {
    addressBus = Read(ProgramCounter);
    ProgramCounter++;
    uint8_t TempAddress = (uint8_t)(addressBus);
    addressBus = Read(TempAddress);
    TempAddress++;
    addressBus = (uint16_t)(Read(TempAddress) << 8 | addressBus);
    addressBus += NES_Y;
}

void Op_ASL(uint16_t Address, uint8_t Input) {
    Flags.Carry = Input > 127;
    Input <<= 1;
    Write(Address, Input);
    Flags.Zero = Input == 0;
    Flags.Negative = Input > 127;
}

void Op_LSR(uint16_t Address, uint8_t Input) {
    Flags.Carry = (Input & 0x01);
    Input >>= 1;
    Write(Address, Input);
    Flags.Zero = Input == 0;
    Flags.Negative = Input > 127;
}

void Op_ROL(uint16_t Address, uint16_t Input) {
    Input <<= 1;
    Input |= Flags.Carry;
    Flags.Carry = Input > 255;
    Input &= 0xFF;
    Write(Address, Input);
    Flags.Negative = Input > 127;
    Flags.Zero = Input == 0;
}

void Op_ROR(uint16_t Address, uint16_t Input) {
    Input |= (Flags.Carry << 8);
    Flags.Carry = (Input & 0x01);
    Input >>= 1;
    Write(Address, Input);
    Flags.Negative = Input > 127;
    Flags.Zero = Input == 0;
}

void Op_INC(uint16_t Address, uint8_t Input) {
    Input++;
    Write(Address, Input);
    Flags.Negative = Input > 127;
    Flags.Zero = Input == 0;
}

void Op_DEC(uint16_t Address, uint8_t Input) {
    Input--;
    Write(Address, Input);
    Flags.Negative = Input > 127;
    Flags.Zero = Input == 0;
}

void Op_ORA(uint8_t Input) {
    NES_A |= Input;
    Flags.Negative = NES_A > 127;
    Flags.Zero = NES_A == 0;
}

void Op_AND(uint8_t Input) {
    NES_A &= Input;
    Flags.Negative = NES_A > 127;
    Flags.Zero = NES_A == 0;
}

void Op_EOR(uint8_t Input) {
    NES_A ^= Input;
    Flags.Negative = NES_A > 127;
    Flags.Zero = NES_A == 0;
}

void Op_ADC(uint8_t Input) {
    Temp = NES_A + Input + Flags.Carry;
    Flags.Carry = Temp > 255;
    Flags.Overflow = (~(NES_A ^ Input) & (NES_A ^ Temp) & 0x80) > 0;
    NES_A = Temp;
    Flags.Negative = NES_A > 127;
    Flags.Zero = NES_A == 0;
}

void Op_SBC(uint8_t Input) {
    Op_ADC(~Input);
}

void Op_CMP(uint8_t Input) {
    uint8_t Result = (uint8_t)(NES_A - Input);
    Flags.Carry = NES_A >= Input;
    Flags.Negative = Result > 127;
    Flags.Zero = NES_A == Input;
}

void Op_CPX(uint8_t Input) {
    uint8_t Result = (uint8_t)(NES_X - Input);
    Flags.Carry = NES_X >= Input;
    Flags.Negative = Result > 127;
    Flags.Zero = NES_X == Input;
}

void Op_CPY(uint8_t Input) {
    uint8_t Result = (uint8_t)(NES_Y - Input);
    Flags.Carry = NES_Y >= Input;
    Flags.Negative = Result > 127;
    Flags.Zero = NES_Y == Input;
}

void Op_BIT(uint8_t Input) {
    Flags.Overflow = (Input & 0x40) > 0;
    Flags.Negative = (Input & 0x80) > 0;
    Flags.Zero = (NES_A & Input) == 0;
}

void Op_LDA(uint8_t Input) {
    NES_A = Input;
    Flags.Zero = NES_A == 0;
    Flags.Negative = NES_A > 127;
}

void Op_LDX(uint8_t Input) {
    NES_X = Input;
    Flags.Zero = NES_X == 0;
    Flags.Negative = NES_X > 127;
}

void Op_LDY(uint8_t Input) {
    NES_Y = Input;
    Flags.Zero = NES_Y == 0;
    Flags.Negative = NES_Y > 127;
}

void Op_SLO(uint16_t Address, uint8_t Input) {
    Flags.Carry = Input > 127;
    Input <<= 1;
    Write(Address, Input);
    // Flags.Zero = Input == 0;
    // Flags.Negative = Input > 127;
    NES_A |= Input;
    Flags.Negative = NES_A > 127;
    Flags.Zero = NES_A == 0;
}

void Op_RLA(uint16_t Address, uint16_t Input) {
    Input <<= 1;
    Input |= Flags.Carry;
    Flags.Carry = Input > 255;
    Input &= 0xFF;
    Write(Address, Input);
    // Flags.Negative = Input > 127;
    // Flags.Zero = Input == 0;
    NES_A &= Input;
    Flags.Negative = NES_A > 127;
    Flags.Zero = NES_A == 0;
}

void Op_SRE(uint16_t Address, uint8_t Input) {
    Flags.Carry = (Input & 0x01);
    Input >>= 1;
    Write(Address, Input);
    // Flags.Zero = Input == 0;
    // Flags.Negative = Input > 127;
    NES_A ^= Input;
    Flags.Negative = NES_A > 127;
    Flags.Zero = NES_A == 0;
}

void Op_RRA(uint16_t Address, uint16_t Input) {
    Input |= (Flags.Carry << 8);
    Flags.Carry = (Input & 0x01);
    Input >>= 1;
    Write(Address, Input);
    // Flags.Negative = Input > 127;
    // Flags.Zero = Input == 0;
    Temp = NES_A + Input + Flags.Carry;
    Flags.Carry = Temp > 255;
    Flags.Overflow = (~(NES_A ^ Input) & (NES_A ^ Temp) & 0x80) > 0;
    NES_A = Temp;
    Flags.Negative = NES_A > 127;
    Flags.Zero = NES_A == 0;
}

void Op_SAX(uint16_t Address) {
    Write(Address, NES_A & NES_X);
}

void Op_LAX(uint8_t Input) {
    NES_A = Input;
    NES_X = Input;
    Flags.Zero = NES_X == 0;
    Flags.Negative = NES_X > 127;
}

void Op_DCP(uint16_t Address, uint8_t Input) {
    Input--;
    Write(Address, Input);
    // Flags.Negative = Input > 127;
    // Flags.Zero = Input == 0;
    uint8_t Result = (uint8_t)(NES_A - Input);
    Flags.Carry = NES_A >= Input;
    Flags.Negative = Result > 127;
    Flags.Zero = NES_A == Input;
}

void Op_ISC(uint16_t Address, uint8_t Input) {
    Input++;
    Write(Address, Input);
    // Flags.Negative = Input > 127;
    // Flags.Zero = Input == 0;
    Op_ADC(~Input);
}

void Op_LAS(uint8_t Input) {
    NES_A = StackPointer & Input;
    NES_X = NES_A;
    StackPointer = NES_A;
    Flags.Zero = NES_A == 0;
    Flags.Negative = NES_A > 127;
}

void NES_Reset() {
    HeaderData = HEADERLIST[selectedROM];
    PRGSIZE = HeaderData[4];
    if (PRGSIZE > 0) PRGSIZEMASK = PRGSIZE - 1;
    else PRGSIZEMASK = 0;
    CHRSIZE = HeaderData[5];
    if (CHRSIZE > 0) CHRSIZEMASK = CHRSIZE - 1;
    else CHRSIZEMASK = 0;
    MAPPER = (HeaderData[6] >> 4) | (HeaderData[7] & 0xF0);
    MIRRORING = HeaderData[6] & 0x01;
    BASEPRGADDRESS = ROMLIST[selectedROM];
    BASECHRADDRESS = BASEPRGADDRESS + (0x4000 * PRGSIZE);

    if (MAPPER == 0) { // NROM
        PRG0Data = BASEPRGADDRESS;
        PRG1Data = BASEPRGADDRESS + (0x4000 * PRGSIZEMASK);
        if (CHRSIZE == 0) CHRData = CHRRAM;
        else CHRData = BASECHRADDRESS;
    }
    else if (MAPPER == 2 || MAPPER == 71) { // UNROM
        PRG0Data = BASEPRGADDRESS;
        PRG1Data = BASEPRGADDRESS + (0x4000 * PRGSIZEMASK);
        if (CHRSIZE == 0) CHRData = CHRRAM;
        else CHRData = BASECHRADDRESS;
    }
    else if (MAPPER == 3) { // CNROM
        PRG0Data = BASEPRGADDRESS;
        PRG1Data = BASEPRGADDRESS + (0x4000 * PRGSIZEMASK);
        if (CHRSIZE == 0) CHRData = CHRRAM;
        else CHRData = BASECHRADDRESS;
    }
    else if (MAPPER == 7) { // ANROM
        PRG0Data = BASEPRGADDRESS;
        PRG1Data = BASEPRGADDRESS + 0x4000;
        if (CHRSIZE == 0) CHRData = CHRRAM;
        else CHRData = BASECHRADDRESS;
    }

    // PRG1Data = ROMLIST[selectedROM]+0x4000;
    // if (HeaderData[5] == 0) {
    //     CHRData = CHRRAM;
    // } else {
    //     CHRData = ROMLIST[selectedROM]+0x8000;
    // }
    Flags.Carry = false;
    Flags.Zero = false;
    Flags.InterruptDisable = true;
    Flags.Decimal = false;
    Flags.Break = true;
    Flags.Expansion = true;
    Flags.Overflow = false;
    Flags.Negative = false;
    APUSTATUS.Raw = 0;
    APUCOUNTER.Raw = 0;
    DMCFLAGS.Raw = 0;
    DMCSampleLength = 1;
    DMCCycleCount = 0;
    APUDMC_BytesRemaining = 0;
    DMCBytesRemainingWriteRequest = false;
    DMCBytesRemainingReloadRequest = false;

    Temp_Low = Read(0xFFFC);
    Temp_High = Read(0xFFFD);
    ProgramCounter = (uint16_t)((Temp_High * 0x100) + Temp_Low);
    StackPointer = 0xFD;
    // printf("%x\n", ProgramCounter);
}

void Emulate_APU() {
    if (AddAlignmentCycle) {
        APUFrameCounter++;
        if (APUFrameCounter >= 14914) {
            APUFrameCounter = 0;
            if (APUCOUNTER.Raw == 0) {
                APUSTATUS.FrameInterrupt = true;
                // printf(" Frame Counter IRQ\n");
                // fflush(stdin);
                // getc(stdin);
            }
        }
    }

    // if (DMCLoadDelay > 0) {
    //     DMCLoadDelay--;
    //     if (DMCLoadDelay == 0) {
            // APUDMC_BytesRemaining = DMCSampleLength;
            // DMCCycleCount = (DMCCycleTable[DMCFLAGS.RateIndex] << 3); // x 8 bits
    //     }
    // }

    // Cycles += DMCLoadDelay + AddAlignmentCycle;
    // Cycles += DMCLoadDelay;
    // DMCLoadDelay = 0;

    if (DMCLoadDelay > 0) {
        DMCLoadDelay--;
        if (DMCLoadDelay == 0) {
            // Cycles += 3 + AddAlignmentCycle;
            if (DMCBytesRemainingReloadRequest || DMCBytesRemainingWriteRequest) APUDMC_BytesRemaining = DMCSampleLength;
            DMCBytesRemainingWriteRequest = false;
            DMCBytesRemainingReloadRequest = false;
            DMCCycleCount = (DMCCycleTable[DMCFLAGS.RateIndex] << 3); // x 8 bits
        }
//        EventBuffer[ppuScanline][ppuDot][0] = 0xFF;
//        EventBuffer[ppuScanline][ppuDot][1] = 0xFF;
//        EventBuffer[ppuScanline][ppuDot][2] = 0x00;
        // Cycles += 1;
        // DMCLoadDelay = 0;
    }

    else if (DMCCycleCount) {
        DMCCycleCount--;
    }
    // else if (DMCLoadDelay > 0) {
    //     DMCLoadDelay--;
    //     if (DMCLoadDelay == 0) {
    //         // Cycles += 3 + AddAlignmentCycle;
    //         if (DMCFLAGS.Loop) APUDMC_BytesRemaining = DMCSampleLength;
    //         DMCCycleCount = (DMCCycleTable[DMCFLAGS.RateIndex] << 3); // x 8 bits
    //     }
    //     // Cycles += 1;
    //     // DMCLoadDelay = 0;
    // }
    else if (APUSTATUS.EnableDMC) {
        // if (DMCLoadDelay) {
        //     Cycles += DMCLoadDelay + AddAlignmentCycle;
        //     DMCLoadDelay = 0;
        // }
        if (APUDMC_BytesRemaining > 0) {
            APUDMC_BytesRemaining--;
            if (APUDMC_BytesRemaining == 0) {
                if (DMCFLAGS.Loop) {
                    DMCBytesRemainingReloadRequest = true;
                    APUDMC_BytesRemaining = DMCSampleLength;
                    DMCCycleCount = (DMCCycleTable[DMCFLAGS.RateIndex] << 3);
                    // AddDMCCycles = 3 + AddAlignmentCycle;
                    // Cycles += 3 + AddAlignmentCycle;
                    // if (DMCLoadDelay == 0) DMCLoadDelay = 3 + AddAlignmentCycle;
                    if (DMCLoadDelay == 0) DMCLoadDelay = 4;
                    // if (DMCLoadDelay == 0) DMCLoadDelay = 1;
                    // if (DMCLoadDelay > (3 + AddAlignmentCycle)) {
                    //     printf(" BIG DELAY %x\n", DMCLoadDelay);
                    //     fflush(stdin);
                    //     getc(stdin);
                    // }
                    // DMCLoadDelay += 3;
                    // Cycles += 3;
                } else {
                    APUSTATUS.EnableDMC = false;
                }
                if (DMCFLAGS.EnableIRQ && !DMCFLAGS.Loop) {
                    APUSTATUS.DMCInterrupt = true;
//                    EventBuffer[ppuScanline][ppuDot][0] = 0xFF;
//                    EventBuffer[ppuScanline][ppuDot][1] = 0xFF;
//                    EventBuffer[ppuScanline][ppuDot][2] = 0xFF;
                }
            } else {
                DMCCycleCount = (DMCCycleTable[DMCFLAGS.RateIndex] << 3);
                // AddDMCCycles = 3 + AddAlignmentCycle;
                // Cycles += 3 + AddAlignmentCycle;
                // if (DMCLoadDelay == 0) DMCLoadDelay = 3 + AddAlignmentCycle;
                if (DMCLoadDelay == 0) DMCLoadDelay = 4;
                // if (DMCLoadDelay == 0) DMCLoadDelay = 1;
                // if (DMCLoadDelay > (3 + AddAlignmentCycle)) {
                //     printf(" BIG DELAY %x\n", DMCLoadDelay);
                //     fflush(stdin);
                //     getc(stdin);
                // }
                // DMCLoadDelay += 3;
                // Cycles += 3;
            }
        } else {
//            printf(" PANIC PANIC PANIC PANIC PANIC PANIC\n");
//            fflush(stdin);
//            getc(stdin);
        }
        // if (DMCLoadDelay > 0) {
        //     DMCLoadDelay--;
        //     if (DMCLoadDelay == 0) {
        //         // Cycles += 3 + AddAlignmentCycle;
        //         if (DMCFLAGS.Loop) APUDMC_BytesRemaining = DMCSampleLength;
        //         DMCCycleCount = (DMCCycleTable[DMCFLAGS.RateIndex] << 3); // x 8 bits
        //     }
        //     // Cycles += 1;
        //     // DMCLoadDelay = 0;
        // }
    }

    // if (DMCLoadDelay) {
    //     // Cycles += 1 - AddAlignmentCycle;
    //     Cycles += 1;
    //     DMCLoadDelay = 0;
    // }

}

void Emulate_CPU() {
    bool PreviousNMILevelDetector = NMILevelDetector;
    NMILevelDetector = PPUCTRL.NMI && PPUSTATUS.VBlank;
    if (!PreviousNMILevelDetector && NMILevelDetector) DoNMI = true;

    bool DoIRQ = (APUSTATUS.DMCInterrupt || APUSTATUS.FrameInterrupt) && (!Flags.InterruptDisable);

    // if (APUSTATUS.DMCInterrupt) printf("$%x\t%x\t%s\t%x\tA: %x\tX: %x\tY: %x\tFlags: %x\tSP: %x\tPPUSTATUS: %x\tdot: %d  \tline: %d\n", ProgramCounter, opcode, opnames[opcode], addressBus, NES_A, NES_X, NES_Y, Flags.Raw, StackPointer, PPUSTATUS.Raw, ppuDot, ppuScanline);

    if ((DoNMI || DoIRQ)) {
        opcode = 0x00;
    } else {
        opcode = Read(ProgramCounter);
        // printf("$%x\t%x\t%s\t%x\tA: %x\tX: %x\tY: %x\tFlags: %x\tSP: %x\tPPUSTATUS: %x\tdot: %d  \tline: %d\n", ProgramCounter, opcode, opnames[opcode], addressBus, NES_A, NES_X, NES_Y, Flags.Raw, StackPointer, PPUSTATUS.Raw, ppuDot, ppuScanline);
        ProgramCounter++;
    }

    Cycles = 0;

    // printf("\t");
    // printf("%x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x", RAM[0], RAM[1], RAM[2], RAM[3], RAM[4], RAM[5], RAM[6], RAM[7], RAM[8], RAM[9], RAM[10], RAM[11], RAM[12], RAM[13], RAM[14], RAM[15], RAM[16], RAM[17], RAM[18], RAM[19], RAM[20], RAM[21], RAM[22], RAM[23], RAM[24], RAM[25], RAM[26], RAM[27], RAM[28], RAM[29], RAM[30], RAM[31], RAM[32], RAM[33], RAM[34], RAM[35], RAM[36], RAM[37], RAM[38], RAM[39], RAM[40], RAM[41], RAM[42], RAM[43], RAM[44], RAM[45], RAM[46], RAM[47], RAM[48], RAM[49], RAM[50], RAM[51], RAM[52], RAM[53], RAM[54], RAM[55], RAM[56], RAM[57], RAM[58], RAM[59], RAM[60], RAM[61], RAM[62], RAM[63]);
    // printf("\n");



    switch (opcode) {
        case 0x00: // BRK, NMI, IRQ
            if (!(DoNMI || DoIRQ)) {
                ProgramCounter++;
            }
            Push((uint8_t)(ProgramCounter >> 8));
            Push((uint8_t)(ProgramCounter));
            Flags.Break = (DoNMI || DoIRQ) ? false : true;
            Push(Flags.Raw | 0b00100000);
            Flags.InterruptDisable = true;
            Temp_Low = Read((uint16_t)(DoNMI ? 0xFFFA : 0xFFFE));
            Temp_High = Read((uint16_t)(DoNMI ? 0xFFFB : 0xFFFF));
            ProgramCounter = (uint16_t)((Temp_High * 0x100) + Temp_Low);
            // if (DoNMI) printf(" BRK NMI\n");
            if (DoIRQ) printf(" BRK IRQ\n");
            DoNMI = false;
            Cycles = 7;
            break;
        case 0x01: // ORA X-Indexed Zero Page Indirect
            ReadOperands_IndirectAddressed_XIndexed();
            Op_ORA(Read(addressBus));
            Cycles = 6;
            break;
        case 0x02: // HLT
            CPU_Halted = true;
            ProgramCounter--;
            break;
        case 0x03: // SLO X-Indexed Zero Page Indirect
            ReadOperands_IndirectAddressed_XIndexed();
            Op_SLO(addressBus, Read(addressBus));
            Cycles = 8;
            break;
        case 0x04: // NOP Zero Page
            ReadOperands_ZeroPageAddressed();
            OpenBus = Read(addressBus);
            Cycles = 3;
            // ProgramCounter++;
            break;
        case 0x05: // ORA Zero Page
            ReadOperands_ZeroPageAddressed();
            Op_ORA(Read(addressBus));
            Cycles = 3;
            break;
        case 0x06: // ASL Zero Page
            ReadOperands_ZeroPageAddressed();
            Op_ASL(addressBus, Read(addressBus));
            Cycles = 5;
            break;
        case 0x07: // SLO Zero Page
            ReadOperands_ZeroPageAddressed();
            Op_SLO(addressBus, Read(addressBus));
            Cycles = 5;
            break;
        case 0x08: // PHP
            Push(Flags.Raw | 0b00110000);
            Cycles = 3;
            break;
        case 0x09: // ORA Immediate
            Op_ORA(Read(ProgramCounter));
            ProgramCounter++;
            Cycles = 2;
            break;
        case 0x0A: // ASL A
            Flags.Carry = NES_A > 127;
            NES_A <<= 1;
            Flags.Zero = NES_A == 0;
            Flags.Negative = NES_A > 127;
            Cycles = 2;
            break;
        case 0x0B: // ANC Immediate
            Op_AND(Read(ProgramCounter));
            Flags.Carry = Flags.Negative;
            ProgramCounter++;
            Cycles = 2;
            break;
        case 0x0C: // NOP Absolute
            ReadOperands_AbsoluteAddressed();
            OpenBus = Read(addressBus);
            Cycles = 4;
            // ProgramCounter += 2;
            break;
        case 0x0D: // ORA Absolute
            ReadOperands_AbsoluteAddressed();
            Op_ORA(Read(addressBus));
            Cycles = 4;
            break;
        case 0x0E: // ASL Absolute
            ReadOperands_AbsoluteAddressed();
            Op_ASL(addressBus, Read(addressBus));
            Cycles = 6;
            break;
        case 0x0F: // SLO Absolute
            ReadOperands_AbsoluteAddressed();
            Op_SLO(addressBus, Read(addressBus));
            Cycles = 6;
            break;
        case 0x10: // BPL
            SignedTemp = Read(ProgramCounter);
            ProgramCounter++;
            if (Flags.Negative == false) {
                if (SignedTemp > 127) SignedTemp -= 256;
                Temp = ProgramCounter & 0xFF00;
                ProgramCounter = (uint16_t)(ProgramCounter + SignedTemp);
                if ((ProgramCounter & 0xFF00) == Temp) Cycles = 3;
                else Cycles = 4;
            } else {
                Cycles = 2;
            }
            break;
        case 0x11: // ORA Zero Page Indirect Y-Indexed
            ReadOperands_IndirectAddressed_YIndexed();
            Op_ORA(Read(addressBus));
            Cycles = 5; // TODO: +1 if page crossed
            break;
        case 0x13: // SLO Y-Indexed Zero Page Indirect
            ReadOperands_IndirectAddressed_YIndexed();
            Op_SLO(addressBus, Read(addressBus));
            Cycles = 8;
            break;
        case 0x14: // NOP X-Indexed Zero Page
            ReadOperands_ZeroPageAddressed_XIndexed();
            OpenBus = Read(addressBus);
            Cycles = 4;
            // ProgramCounter++;
            break;
        case 0x15: // ORA X-Indexed Zero Page
            ReadOperands_ZeroPageAddressed_XIndexed();
            Op_ORA(Read(addressBus));
            Cycles = 4;
            break;
        case 0x16: // ASL X-Indexed Zero Page
            ReadOperands_ZeroPageAddressed_XIndexed();
            Op_ASL(addressBus, Read(addressBus));
            Cycles = 6;
            break;
        case 0x17: // SLO X-Indexed Zero Page
            ReadOperands_ZeroPageAddressed_XIndexed();
            Op_SLO(addressBus, Read(addressBus));
            Cycles = 6;
            break;
        case 0x18: // CLC
            Flags.Carry = false;
            Cycles = 2;
            break;
        case 0x19: // ORA Y-Indexed Absolute
            ReadOperands_AbsoluteAddressed_YIndexed();
            Op_ORA(Read(addressBus));
            Cycles = 4; // TODO: +1 if page crossed
            break;
        case 0x1A: // NOP
            Cycles = 2;
            break;
        case 0x1B: // SLO Y-Indexed Absolute
            ReadOperands_AbsoluteAddressed_YIndexed();
            Op_SLO(addressBus, Read(addressBus));
            Cycles = 7;
            break;
        case 0x1C: // NOP X-Indexed Absolute
            ReadOperands_AbsoluteAddressed_XIndexed();
            OpenBus = Read(addressBus);
            Cycles = 4;
            // ProgramCounter += 2;
            break;
        case 0x1D: // ORA X-Indexed Absolute
            ReadOperands_AbsoluteAddressed_XIndexed();
            Op_ORA(Read(addressBus));
            Cycles = 4; // TODO: +1 if page crossed
            break;
        case 0x1E: // ASL X-Indexed Absolute
            ReadOperands_AbsoluteAddressed_XIndexed();
            Op_ASL(addressBus, Read(addressBus));
            Cycles = 7;
            break;
        case 0x1F: // SLO X-Indexed Absolute
            ReadOperands_AbsoluteAddressed_XIndexed();
            Op_SLO(addressBus, Read(addressBus));
            Cycles = 7;
            break;
        case 0x20: // JSR
            Temp_Low = Read(ProgramCounter);
            ProgramCounter++;
            Push((uint8_t)(ProgramCounter >> 8)); // push PCH
            Push((uint8_t)(ProgramCounter)); // push PCL
            Temp_High = Read(ProgramCounter);
            ProgramCounter = (uint16_t)((Temp_High * 256) + Temp_Low);
            Cycles = 6;
            break;
        case 0x21: // AND X-Indexed Zero Page Indirect
            ReadOperands_IndirectAddressed_XIndexed();
            Op_AND(Read(addressBus));
            Cycles = 6;
            break;
        case 0x23: // RLA X-Indexed Zero Page Indirect
            ReadOperands_IndirectAddressed_XIndexed();
            Op_RLA(addressBus, Read(addressBus));
            Cycles = 8;
            break;
        case 0x24: // BIT Zero Page
            ReadOperands_ZeroPageAddressed();
            Op_BIT(Read(addressBus));
            Cycles = 3;
            break;
        case 0x25: // AND Zero Page
            ReadOperands_ZeroPageAddressed();
            Op_AND(Read(addressBus));
            Cycles = 3;
            break;
        case 0x26: // ROL Zero Page
            ReadOperands_ZeroPageAddressed();
            Op_ROL(addressBus, Read(addressBus));
            Cycles = 5;
            break;
        case 0x27: // RLA Zero Page
            ReadOperands_ZeroPageAddressed();
            Op_RLA(addressBus, Read(addressBus));
            Cycles = 5;
            break;
        case 0x28: // PLP
            Flags.Raw = Pull() & 0b11001111;
            Cycles = 3;
            break;
        case 0x29: // AND Immediate
            Op_AND(Read(ProgramCounter));
            ProgramCounter++;
            Cycles = 2;
            break;
        case 0x2A: // ROL A
            Temp = NES_A;
            Temp <<= 1;
            Temp |= Flags.Carry;
            Flags.Carry = Temp > 255;
            Temp &= 0xFF;
            NES_A = Temp;
            Flags.Negative = Temp > 127;
            Flags.Zero = Temp == 0;
            Cycles = 2;
            break;
        case 0x2B: // ANC Immediate
            Op_AND(Read(ProgramCounter));
            Flags.Carry = Flags.Negative;
            ProgramCounter++;
            Cycles = 2;
            break;
        case 0x2C: // BIT Absolute
            ReadOperands_AbsoluteAddressed();
            Op_BIT(Read(addressBus));
            Cycles = 4;
            break;
        case 0x2D: // AND Absolute
            ReadOperands_AbsoluteAddressed();
            Op_AND(Read(addressBus));
            Cycles = 4;
            break;
        case 0x2E: // ROL Absolute
            ReadOperands_AbsoluteAddressed();
            Op_ROL(addressBus, Read(addressBus));
            Cycles = 6;
            break;
        case 0x2F: // RLA Absolute
            ReadOperands_AbsoluteAddressed();
            Op_RLA(addressBus, Read(addressBus));
            Cycles = 6;
            break;
        case 0x30: // BMI
            SignedTemp = Read(ProgramCounter);
            ProgramCounter++;
            if (Flags.Negative == true) {
                if (SignedTemp > 127) SignedTemp -= 256;
                Temp = ProgramCounter & 0xFF00;
                ProgramCounter = (uint16_t)(ProgramCounter + SignedTemp);
                if ((ProgramCounter & 0xFF00) == Temp) Cycles = 3;
                else Cycles = 4;
            } else {
                Cycles = 2;
            }
            break;
        case 0x31: // AND Zero Page Indirect Y-Indexed
            ReadOperands_IndirectAddressed_YIndexed();
            Op_AND(Read(addressBus));
            Cycles = 5; // TODO: +1 if page crossed
            break;
        case 0x33: // RLA Y-Indexed Zero Page Indirect
            ReadOperands_IndirectAddressed_YIndexed();
            Op_RLA(addressBus, Read(addressBus));
            Cycles = 8;
            break;
        case 0x34: // NOP X-Indexed Zero Page
            ReadOperands_ZeroPageAddressed_XIndexed();
            OpenBus = Read(addressBus);
            Cycles = 4;
            // ProgramCounter++;
            break;
        case 0x35: // AND X-Indexed Zero Page
            ReadOperands_ZeroPageAddressed_XIndexed();
            Op_AND(Read(addressBus));
            Cycles = 4;
            break;
        case 0x36: // ROL X-Indexed Zero Page
            ReadOperands_ZeroPageAddressed_XIndexed();
            Op_ROL(addressBus, Read(addressBus));
            Cycles = 6;
            break;
        case 0x37: // RLA X-Indexed Zero Page
            ReadOperands_ZeroPageAddressed_XIndexed();
            Op_RLA(addressBus, Read(addressBus));
            Cycles = 6;
            break;
        case 0x38: // SEC
            Flags.Carry = true;
            Cycles = 2;
            break;
        case 0x39: // AND Y-Indexed Absolute
            ReadOperands_AbsoluteAddressed_YIndexed();
            Op_AND(Read(addressBus));
            Cycles = 4; // TODO: +1 if page crossed
            break;
        case 0x3A: // NOP
            Cycles = 2;
            break;
        case 0x3B: // RLA Y-Indexed Absolute
            ReadOperands_AbsoluteAddressed_YIndexed();
            Op_RLA(addressBus, Read(addressBus));
            Cycles = 7;
            break;
        case 0x3C: // NOP X-Indexed Absolute
            ReadOperands_AbsoluteAddressed_XIndexed();
            OpenBus = Read(addressBus);
            Cycles = 4;
            // ProgramCounter += 2;
            break;
        case 0x3D: // AND X-Indexed Absolute
            ReadOperands_AbsoluteAddressed_XIndexed();
            Op_AND(Read(addressBus));
            Cycles = 4; // TODO: +1 if page crossed
            break;
        case 0x3E: // ROL X-Indexed Absolute
            ReadOperands_AbsoluteAddressed_XIndexed();
            Op_ROL(addressBus, Read(addressBus));
            Cycles = 7;
            break;
        case 0x3F: // RLA X-Indexed Absolute
            ReadOperands_AbsoluteAddressed_XIndexed();
            Op_RLA(addressBus, Read(addressBus));
            Cycles = 7;
            break;
        case 0x40: // RTI
            Flags.Raw = Pull() & 0b11001111;
            Temp_Low = Pull();
            Temp_High = Pull();
            ProgramCounter = (uint16_t)((Temp_High * 256) + Temp_Low);
            Cycles = 6;
            break;
        case 0x41: // EOR X-Indexed Zero Page Indirect
            ReadOperands_IndirectAddressed_XIndexed();
            Op_EOR(Read(addressBus));
            Cycles = 6;
            break;
        case 0x43: // SRE X-Indexed Zero Page Indirect
            ReadOperands_IndirectAddressed_XIndexed();
            Op_SRE(addressBus, Read(addressBus));
            Cycles = 8;
            break;
        case 0x44: // NOP Zero Page
            ReadOperands_ZeroPageAddressed();
            OpenBus = Read(addressBus);
            Cycles = 3;
            // ProgramCounter++;
            break;
        case 0x45: // EOR Zero Page
            ReadOperands_ZeroPageAddressed();
            Op_EOR(Read(addressBus));
            Cycles = 3;
            break;
        case 0x46: // LSR Zero Page
            ReadOperands_ZeroPageAddressed();
            Op_LSR(addressBus, Read(addressBus));
            Cycles = 5;
            break;
        case 0x47: // SRE Zero Page
            ReadOperands_ZeroPageAddressed();
            Op_SRE(addressBus, Read(addressBus));
            Cycles = 5;
            break;
        case 0x48: // PHA
            Push(NES_A);
            Cycles = 3;
            break;
        case 0x49: // EOR Immediate
            Op_EOR(Read(ProgramCounter));
            ProgramCounter++;
            Cycles = 2;
            break;
        case 0x4A: // LSR A
            Flags.Carry = (NES_A & 0x01);
            NES_A >>= 1;
            Flags.Zero = NES_A == 0;
            Flags.Negative = NES_A > 127;
            Cycles = 2;
            break;
        case 0x4B: // ASR Immediate
            Op_AND(Read(ProgramCounter));
            Flags.Carry = (NES_A & 0x01);
            NES_A >>= 1;
            Flags.Negative = false;
            Flags.Zero = NES_A == 0;
            ProgramCounter++;
            Cycles = 2;
            break;
        case 0x4C: // JMP
            Temp_Low = Read(ProgramCounter);
            ProgramCounter++;
            Temp_High = Read(ProgramCounter);
            ProgramCounter = (uint16_t)((Temp_High * 256) + Temp_Low);
            Cycles = 3;
            break;
        case 0x4D: // EOR Absolute
            ReadOperands_AbsoluteAddressed();
            Op_EOR(Read(addressBus));
            Cycles = 4;
            break;
        case 0x4E: // LSR Absolute
            ReadOperands_AbsoluteAddressed();
            Op_LSR(addressBus, Read(addressBus));
            Cycles = 6;
            break;
        case 0x4F: // SRE Absolute
            ReadOperands_AbsoluteAddressed();
            Op_SRE(addressBus, Read(addressBus));
            Cycles = 6;
            break;
        case 0x50: // BVC
            SignedTemp = Read(ProgramCounter);
            ProgramCounter++;
            if (Flags.Overflow == false) {
                if (SignedTemp > 127) SignedTemp -= 256;
                Temp = ProgramCounter & 0xFF00;
                ProgramCounter = (uint16_t)(ProgramCounter + SignedTemp);
                if ((ProgramCounter & 0xFF00) == Temp) Cycles = 3;
                else Cycles = 4;
            } else {
                Cycles = 2;
            }
            break;
        case 0x51: // EOR Zero Page Indirect Y-Indexed
            ReadOperands_IndirectAddressed_YIndexed();
            Op_EOR(Read(addressBus));
            Cycles = 5; // TODO: +1 if page crossed
            break;
        case 0x53: // SRE Y-Indexed Zero Page Indirect
            ReadOperands_IndirectAddressed_YIndexed();
            Op_SRE(addressBus, Read(addressBus));
            Cycles = 8;
            break;
        case 0x54: // NOP X-Indexed Zero Page
            ReadOperands_ZeroPageAddressed_XIndexed();
            OpenBus = Read(addressBus);
            Cycles = 4;
            // ProgramCounter++;
            break;
        case 0x55: // EOR X-Indexed Zero Page
            ReadOperands_ZeroPageAddressed_XIndexed();
            Op_EOR(Read(addressBus));
            Cycles = 4;
            break;
        case 0x56: // LSR X-Indexed Zero Page
            ReadOperands_ZeroPageAddressed_XIndexed();
            Op_LSR(addressBus, Read(addressBus));
            Cycles = 6;
            break;
        case 0x57: // SRE X-Indexed Zero Page
            ReadOperands_ZeroPageAddressed_XIndexed();
            Op_SRE(addressBus, Read(addressBus));
            Cycles = 6;
            break;
        case 0x58: // CLI
            Flags.InterruptDisable = false;
//            printf(" CLI\n");
            Cycles = 2;
            break;
        case 0x59: // EOR Y-Indexed Absolute
            ReadOperands_AbsoluteAddressed_YIndexed();
            Op_EOR(Read(addressBus));
            Cycles = 4; // TODO: +1 if page crossed
            break;
        case 0x5A: // NOP
            Cycles = 2;
            break;
        case 0x5B: // SRE Y-Indexed Absolute
            ReadOperands_AbsoluteAddressed_YIndexed();
            Op_SRE(addressBus, Read(addressBus));
            Cycles = 7;
            break;
        case 0x5C: // NOP X-Indexed Absolute
            ReadOperands_AbsoluteAddressed_XIndexed();
            OpenBus = Read(addressBus);
            Cycles = 4;
            // ProgramCounter += 2;
            break;
        case 0x5D: // EOR X-Indexed Absolute
            ReadOperands_AbsoluteAddressed_XIndexed();
            Op_EOR(Read(addressBus));
            Cycles = 4; // TODO: +1 if page crossed
            break;
        case 0x5E: // LSR X-Indexed Absolute
            ReadOperands_AbsoluteAddressed_XIndexed();
            Op_LSR(addressBus, Read(addressBus));
            Cycles = 7;
            break;
        case 0x5F: // SRE X-Indexed Absolute
            ReadOperands_AbsoluteAddressed_XIndexed();
            Op_SRE(addressBus, Read(addressBus));
            Cycles = 7;
            break;
        case 0x60: // RTS
            Temp_Low = Pull();
            Temp_High = Pull();
            ProgramCounter = (uint16_t)((Temp_High * 256) + Temp_Low);
            ProgramCounter++;
            Cycles = 6;
            break;
        case 0x61: // ADC X-Indexed Zero Page Indirect
            ReadOperands_IndirectAddressed_XIndexed();
            Op_ADC(Read(addressBus));
            Cycles = 6;
            break;
        case 0x63: // RRA X-Indexed Zero Page Indirect
            ReadOperands_IndirectAddressed_XIndexed();
            Op_RRA(addressBus, Read(addressBus));
            Cycles = 8;
            break;
        case 0x64: // NOP Zero Page
            ReadOperands_ZeroPageAddressed();
            OpenBus = Read(addressBus);
            Cycles = 3;
            // ProgramCounter++;
            break;
        case 0x65: // ADC Zero Page
            ReadOperands_ZeroPageAddressed();
            Op_ADC(Read(addressBus));
            Cycles = 3;
            break;
        case 0x66: // ROR Zero Page
            ReadOperands_ZeroPageAddressed();
            Op_ROR(addressBus, Read(addressBus));
            Cycles = 5;
            break;
        case 0x67: // RRA Zero Page
            ReadOperands_ZeroPageAddressed();
            Op_RRA(addressBus, Read(addressBus));
            Cycles = 5;
            break;
        case 0x68: // PLA
            NES_A = Pull();
            Flags.Zero = NES_A == 0;
            Flags.Negative = NES_A > 127;
            Cycles = 4;
            break;
        case 0x69: // ADC Immediate
            Op_ADC(Read(ProgramCounter));
            ProgramCounter++;
            Cycles = 2;
            break;
        case 0x6A: // ROR A
            Temp = NES_A;
            Temp |= (Flags.Carry << 8);
            Flags.Carry = (Temp & 0x01);
            Temp >>= 1;
            NES_A = Temp;
            Flags.Negative = NES_A > 127;
            Flags.Zero = NES_A == 0;
            Cycles = 2;
            break;
        case 0x6B: // ARR Immediate
            Op_AND(Read(ProgramCounter));
            NES_A >>= 1;
            NES_A |= (Flags.Carry << 7);
            Flags.Negative = NES_A > 127;
            Flags.Zero = NES_A == 0;
            Flags.Overflow = (NES_A & 0b01000000) != ((NES_A & 0b00100000) << 1);
            Flags.Carry = (NES_A & 0b01000000) > 0;
            ProgramCounter++;
            Cycles = 2;
            break;
        case 0x6C: // JMP Indirect
            Temp_Low = Read(ProgramCounter);
            ProgramCounter++;
            Temp_High = Read(ProgramCounter);
            addressBus = (uint16_t)((Temp_High * 256) + Temp_Low);
            Temp_Low = Read(addressBus);
            addressBus = (uint16_t)((addressBus & 0xFF00) | ((addressBus + 1) & 0x00FF));
            Temp_High = Read(addressBus);
            ProgramCounter = (uint16_t)((Temp_High * 256) + Temp_Low);
            Cycles = 5;
            break;
        case 0x6D: // ADC Absolute
            ReadOperands_AbsoluteAddressed();
            Op_ADC(Read(addressBus));
            Cycles = 4;
            break;
        case 0x6E: // ROR Absolute
            ReadOperands_AbsoluteAddressed();
            Op_ROR(addressBus, Read(addressBus));
            Cycles = 6;
            break;
        case 0x6F: // RRA Absolute
            ReadOperands_AbsoluteAddressed();
            Op_RRA(addressBus, Read(addressBus));
            Cycles = 6;
            break;
        case 0x70: // BVS
            SignedTemp = Read(ProgramCounter);
            ProgramCounter++;
            if (Flags.Overflow == true) {
                if (SignedTemp > 127) SignedTemp -= 256;
                Temp = ProgramCounter & 0xFF00;
                ProgramCounter = (uint16_t)(ProgramCounter + SignedTemp);
                if ((ProgramCounter & 0xFF00) == Temp) Cycles = 3;
                else Cycles = 4;
            } else {
                Cycles = 2;
            }
            break;
        case 0x71: // ADC Zero Page Indirect Y-Indexed
            ReadOperands_IndirectAddressed_YIndexed();
            Op_ADC(Read(addressBus));
            Cycles = 5; // TODO: +1 if page crossed
            break;
        case 0x73: // RRA Y-Indexed Zero Page Indirect
            ReadOperands_IndirectAddressed_YIndexed();
            Op_RRA(addressBus, Read(addressBus));
            Cycles = 8;
            break;
        case 0x74: // NOP X-Indexed Zero Page
            ReadOperands_ZeroPageAddressed_XIndexed();
            OpenBus = Read(addressBus);
            Cycles = 4;
            // ProgramCounter++;
            break;
        case 0x75: // ADC X-Indexed Zero Page
            ReadOperands_ZeroPageAddressed_XIndexed();
            Op_ADC(Read(addressBus));
            Cycles = 4;
            break;
        case 0x76: // ROR X-Indexed Zero Page
            ReadOperands_ZeroPageAddressed_XIndexed();
            Op_ROR(addressBus, Read(addressBus));
            Cycles = 6;
            break;
        case 0x77: // RRA X-Indexed Zero Page
            ReadOperands_ZeroPageAddressed_XIndexed();
            Op_RRA(addressBus, Read(addressBus));
            Cycles = 6;
            break;
        case 0x78: // SEI
            Flags.InterruptDisable = true;
//            printf(" SEI\n");
            Cycles = 2;
            break;
        case 0x79: // ADC Y-Indexed Absolute
            ReadOperands_AbsoluteAddressed_YIndexed();
            Op_ADC(Read(addressBus));
            Cycles = 4; // TODO: +1 if page crossed
            break;
        case 0x7A: // NOP
            Cycles = 2;
            break;
        case 0x7B: // RRA Y-Indexed Absolute
            ReadOperands_AbsoluteAddressed_YIndexed();
            Op_RRA(addressBus, Read(addressBus));
            Cycles = 7;
            break;
        case 0x7C: // NOP X-Indexed Absolute
            ReadOperands_AbsoluteAddressed_XIndexed();
            OpenBus = Read(addressBus);
            Cycles = 4;
            // ProgramCounter += 2;
            break;
        case 0x7D: // ADC X-Indexed Absolute
            ReadOperands_AbsoluteAddressed_XIndexed();
            Op_ADC(Read(addressBus));
            Cycles = 4; // TODO: +1 if page crossed
            break;
        case 0x7E: // ROR X-Indexed Absolute
            ReadOperands_AbsoluteAddressed_XIndexed();
            Op_ROR(addressBus, Read(addressBus));
            Cycles = 7;
            break;
        case 0x7F: // RRA X-Indexed Absolute
            ReadOperands_AbsoluteAddressed_XIndexed();
            Op_RRA(addressBus, Read(addressBus));
            Cycles = 7;
            break;
        case 0x80: // NOP Immediate
            OpenBus = Read(ProgramCounter);
            Cycles = 2;
            ProgramCounter++;
            break;
        case 0x81: // STA X-Indexed Zero Page Indirect
            ReadOperands_IndirectAddressed_XIndexed();
            Write(addressBus, NES_A);
            Cycles = 6;
            break;
        case 0x82: // NOP Immediate
            OpenBus = Read(ProgramCounter);
            Cycles = 2;
            ProgramCounter++;
            break;
        case 0x83: // SAX X-Indexed Zero Page Indirect
            ReadOperands_IndirectAddressed_XIndexed();
            Op_SAX(addressBus);
            Cycles = 6;
            break;
        case 0x84: // STY Zero Page
            ReadOperands_ZeroPageAddressed();
            Write(addressBus, NES_Y);
            Cycles = 3;
            break;
        case 0x85: // STA Zero Page
            ReadOperands_ZeroPageAddressed();
            Write(addressBus, NES_A);
            Cycles = 3;
            break;
        case 0x86: // STX Zero Page
            ReadOperands_ZeroPageAddressed();
            Write(addressBus, NES_X);
            Cycles = 3;
            break;
        case 0x87: // SAX Zero Page
            ReadOperands_ZeroPageAddressed();
            Op_SAX(addressBus);
            Cycles = 3;
            break;
        case 0x88: // DEY
            NES_Y--;
            Flags.Zero = NES_Y == 0;
            Flags.Negative = NES_Y > 127;
            Cycles = 2;
            break;
        case 0x89: // NOP Immediate
            OpenBus = Read(ProgramCounter);
            Cycles = 2;
            ProgramCounter++;
            break;
        case 0x8A: // TXA
            NES_A = NES_X;
            Flags.Zero = NES_A == 0;
            Flags.Negative = NES_A > 127;
            Cycles = 2;
            break;
        case 0x8B: // XAA Immediate
            SmallTemp = Read(ProgramCounter);
            NES_A = (NES_A | 0xEE) & NES_X & SmallTemp;
            Flags.Negative = NES_A > 127;
            Flags.Zero = NES_A == 0;
            // Op_AND(Read(ProgramCounter));
            ProgramCounter++;
            Cycles = 2;
            break;
        case 0x8C: // STY Absolute
            ReadOperands_AbsoluteAddressed();
            Write(addressBus, NES_Y);
            Cycles = 4;
            break;
        case 0x8D: // STA Absolute
            ReadOperands_AbsoluteAddressed();
            Write(addressBus, NES_A);
            Cycles = 4;
            break;
        case 0x8E: // STX Absolute
            ReadOperands_AbsoluteAddressed();
            Write(addressBus, NES_X);
            Cycles = 4;
            break;
        case 0x8F: // SAX Absolute
            ReadOperands_AbsoluteAddressed();
            Op_SAX(addressBus);
            Cycles = 4;
            break;
        case 0x90: // BCC
            SignedTemp = Read(ProgramCounter);
            ProgramCounter++;
            if (Flags.Carry == false) {
                if (SignedTemp > 127) SignedTemp -= 256;
                Temp = ProgramCounter & 0xFF00;
                ProgramCounter = (uint16_t)(ProgramCounter + SignedTemp);
                if ((ProgramCounter & 0xFF00) == Temp) Cycles = 3;
                else Cycles = 4;
            } else {
                Cycles = 2;
            }
            break;
        case 0x91: // STA Zero Page Indirect Y-Indexed
            ReadOperands_IndirectAddressed_YIndexed();
            Write(addressBus, NES_A);
            Cycles = 6;
            break;
        case 0x93: // SHA Y-Indexed Zero Page Indirect
            addressBus = Read(ProgramCounter);
            ProgramCounter++;
            uint8_t TempAddress = (uint8_t)(addressBus);
            addressBus = Read(TempAddress);
            TempAddress++;
            addressBus = (uint16_t)(Read(TempAddress) << 8 | addressBus);
            SmallTemp = (uint8_t)(addressBus >> 8) + 1;
            addressBus += NES_Y;
            Write(addressBus, NES_A & NES_X & SmallTemp);
            Cycles = 6;
            break;
        case 0x94: // STY X-Indexed Zero Page
            ReadOperands_ZeroPageAddressed_XIndexed();
            Write(addressBus, NES_Y);
            Cycles = 4;
            break;
        case 0x95: // STA X-Indexed Zero Page
            ReadOperands_ZeroPageAddressed_XIndexed();
            Write(addressBus, NES_A);
            Cycles = 4;
            break;
        case 0x96: // STX Y-Indexed Zero Page
            ReadOperands_ZeroPageAddressed_YIndexed();
            Write(addressBus, NES_X);
            Cycles = 4;
            break;
        case 0x97: // SAX Y-Indexed Zero Page
            ReadOperands_ZeroPageAddressed_YIndexed();
            Op_SAX(addressBus);
            Cycles = 4;
            break;
        case 0x98: // TYA
            NES_A = NES_Y;
            Flags.Zero = NES_A == 0;
            Flags.Negative = NES_A > 127;
            Cycles = 2;
            break;
        case 0x99: // STA Y-Indexed Absolute
            ReadOperands_AbsoluteAddressed_YIndexed();
            Write(addressBus, NES_A);
            Cycles = 5;
            break;
        case 0x9A: // TXS
            StackPointer = NES_X;
            Cycles = 2;
            break;
        case 0x9B: // SHS Y-Indexed Absolute
            addressBus = Read(ProgramCounter);
            ProgramCounter++;
            addressBus = (uint16_t)(Read(ProgramCounter) << 8 | addressBus);
            ProgramCounter++;
            SmallTemp = (uint8_t)(addressBus >> 8) + 1;
            addressBus += NES_Y;
            StackPointer = NES_A & NES_X;
            Write(addressBus, StackPointer & SmallTemp);
            Cycles = 5;
            break;
        case 0x9C: // SHY X-Indexed Absolute
            ReadOperands_AbsoluteAddressed();
            SmallTemp = (uint8_t)((addressBus >> 8) + 1);
            addressBus += NES_X;
            Write(addressBus, NES_Y & SmallTemp);
            Cycles = 5;
            break;
        case 0x9D: // STA X-Indexed Absolute
            ReadOperands_AbsoluteAddressed_XIndexed();
            Write(addressBus, NES_A);
            Cycles = 5;
            break;
        case 0x9E: // SHX Y-Indexed Absolute
            ReadOperands_AbsoluteAddressed();
            SmallTemp = (uint8_t)((addressBus >> 8) + 1);
            addressBus += NES_Y;
            Write(addressBus, NES_X & SmallTemp);
            Cycles = 5;
            break;
        case 0x9F: // SHA Y-Indexed Absolute
            ReadOperands_AbsoluteAddressed();
            SmallTemp = (uint8_t)(addressBus >> 8) + 1;
            addressBus += NES_Y;
            Write(addressBus, NES_A & NES_X & SmallTemp);
            Cycles = 5;
            break;
        case 0xA0: // LDY Immediate
            Op_LDY(Read(ProgramCounter));
            ProgramCounter++;
            Cycles = 2;
            break;
        case 0xA1: // LDA X-Indexed Zero Page Indirect
            ReadOperands_IndirectAddressed_XIndexed();
            Op_LDA(Read(addressBus));
            Cycles = 6;
            break;
        case 0xA2: // LDX Immediate
            Op_LDX(Read(ProgramCounter));
            ProgramCounter++;
            Cycles = 2;
            break;
        case 0xA3: // LAX X-Indexed Zero Page Indirect
            ReadOperands_IndirectAddressed_XIndexed();
            Op_LAX(Read(addressBus));
            Cycles = 6;
            break;
        case 0xA4: // LDY Zero Page
            ReadOperands_ZeroPageAddressed();
            Op_LDY(Read(addressBus));
            Cycles = 3;
            break;
        case 0xA5: // LDA Zero Page
            ReadOperands_ZeroPageAddressed();
            Op_LDA(Read(addressBus));
            Cycles = 3;
            break;
        case 0xA6: // LDX Zero Page
            ReadOperands_ZeroPageAddressed();
            Op_LDX(Read(addressBus));
            Cycles = 3;
            break;
        case 0xA7: // LAX Zero Page
            ReadOperands_ZeroPageAddressed();
            Op_LAX(Read(addressBus));
            Cycles = 3;
            break;
        case 0xA8: // TAY
            NES_Y = NES_A;
            Flags.Zero = NES_Y == 0;
            Flags.Negative = NES_Y > 127;
            Cycles = 2;
            break;
        case 0xA9: // LDA Immediate
            Op_LDA(Read(ProgramCounter));
            ProgramCounter++;
            Cycles = 2;
            break;
        case 0xAA: // TAX
            NES_X = NES_A;
            Flags.Zero = NES_X == 0;
            Flags.Negative = NES_X > 127;
            Cycles = 2;
            break;
        case 0xAB: // LAX Immediate
            Op_LAX(Read(ProgramCounter));
            ProgramCounter++;
            Cycles = 2;
            break;
        case 0xAC: // LDY Absolute
            ReadOperands_AbsoluteAddressed();
            Op_LDY(Read(addressBus));
            Cycles = 4;
            break;
        case 0xAD: // LDA Absolute
            ReadOperands_AbsoluteAddressed();
            Op_LDA(Read(addressBus));
            Cycles = 4;
            break;
        case 0xAE: // LDX Absolute
            ReadOperands_AbsoluteAddressed();
            Op_LDX(Read(addressBus));
            Cycles = 4;
            break;
        case 0xAF: // LAX Absolute
            ReadOperands_AbsoluteAddressed();
            Op_LAX(Read(addressBus));
            Cycles = 4;
            break;
        case 0xB0: // BCS
            SignedTemp = Read(ProgramCounter);
            ProgramCounter++;
            if (Flags.Carry == true) {
                if (SignedTemp > 127) SignedTemp -= 256;
                Temp = ProgramCounter & 0xFF00;
                ProgramCounter = (uint16_t)(ProgramCounter + SignedTemp);
                if ((ProgramCounter & 0xFF00) == Temp) Cycles = 3;
                else Cycles = 4;
            } else {
                Cycles = 2;
            }
            break;
        case 0xB1: // LDA Zero Page Indirect Y-Indexed
            ReadOperands_IndirectAddressed_YIndexed();
            Op_LDA(Read(addressBus));
            Cycles = 5; // TODO: +1 if page crossed
            break;
        case 0xB3: // LAX Y-Indexed Zero Page Indirect
            ReadOperands_IndirectAddressed_YIndexed();
            Op_LAX(Read(addressBus));
            Cycles = 5;
            break;
        case 0xB4: // LDY X-Indexed Zero Page
            ReadOperands_ZeroPageAddressed_XIndexed();
            Op_LDY(Read(addressBus));
            Cycles = 4;
            break;
        case 0xB5: // LDA X-Indexed Zero Page
            ReadOperands_ZeroPageAddressed_XIndexed();
            Op_LDA(Read(addressBus));
            Cycles = 4;
            break;
        case 0xB6: // LDX Y-Indexed Zero Page
            ReadOperands_ZeroPageAddressed_YIndexed();
            Op_LDX(Read(addressBus));
            Cycles = 4;
            break;
        case 0xB7: // LAX Y-Indexed Zero Page
            ReadOperands_ZeroPageAddressed_YIndexed();
            Op_LAX(Read(addressBus));
            Cycles = 4;
            break;
        case 0xB8: // CLV
            Flags.Overflow = false;
            Cycles = 2;
            break;
        case 0xB9: // LDA Y-Indexed Absolute
            ReadOperands_AbsoluteAddressed_YIndexed();
            Op_LDA(Read(addressBus));
            Cycles = 4; // TODO: +1 if page crossed
            break;
        case 0xBA: // TSX
            NES_X = StackPointer;
            Flags.Zero = NES_X == 0;
            Flags.Negative = NES_X > 127;
            Cycles = 2;
            break;
        case 0xBB: // LAS / LAE Y-Indexed Absolute
            ReadOperands_AbsoluteAddressed_YIndexed();
            Op_LAS(Read(addressBus));
            Cycles = 4;
            break;
        case 0xBC: // LDY X-Indexed Absolute
            ReadOperands_AbsoluteAddressed_XIndexed();
            Op_LDY(Read(addressBus));
            Cycles = 4; // TODO: +1 if page crossed
            break;
        case 0xBD: // LDA X-Indexed Absolute
            ReadOperands_AbsoluteAddressed_XIndexed();
            Op_LDA(Read(addressBus));
            Cycles = 4; // TODO: +1 if page crossed
            break;
        case 0xBE: // LDX Y-Indexed Absolute
            ReadOperands_AbsoluteAddressed_YIndexed();
            Op_LDX(Read(addressBus));
            Cycles = 4; // TODO: +1 if page crossed
            break;
        case 0xBF: // LAX Y-Indexed Absolute
            ReadOperands_AbsoluteAddressed_YIndexed();
            Op_LAX(Read(addressBus));
            Cycles = 4;
            break;
        case 0xC0: // CPY Immediate
            Op_CPY(Read(ProgramCounter));
            ProgramCounter++;
            Cycles = 2;
            break;
        case 0xC1: // CMP X-Indexed Zero Page Indirect
            ReadOperands_IndirectAddressed_XIndexed();
            Op_CMP(Read(addressBus));
            Cycles = 6;
            break;
        case 0xC2: // NOP Immediate
            OpenBus = Read(ProgramCounter);
            Cycles = 2;
            ProgramCounter++;
            break;
        case 0xC3: // DCP X-Indexed Zero Page Indirect
            ReadOperands_IndirectAddressed_XIndexed();
            Op_DCP(addressBus, Read(addressBus));
            Cycles = 8;
            break;
        case 0xC4: // CPY Zero Page
            ReadOperands_ZeroPageAddressed();
            Op_CPY(Read(addressBus));
            Cycles = 3;
            break;
        case 0xC5: // CMP Zero Page
            ReadOperands_ZeroPageAddressed();
            Op_CMP(Read(addressBus));
            Cycles = 3;
            break;
        case 0xC6: // DEC Zero Page
            ReadOperands_ZeroPageAddressed();
            Op_DEC(addressBus, Read(addressBus));
            Cycles = 5;
            break;
        case 0xC7: // DCP Zero Page
            ReadOperands_ZeroPageAddressed();
            Op_DCP(addressBus, Read(addressBus));
            Cycles = 5;
            break;
        case 0xC8: // INY
            NES_Y++;
            Flags.Zero = NES_Y == 0;
            Flags.Negative = NES_Y > 127;
            Cycles = 2;
            break;
        case 0xC9: // CMP Immediate
            Op_CMP(Read(ProgramCounter));
            ProgramCounter++;
            Cycles = 2;
            break;
        case 0xCA: // DEX
            NES_X--;
            Flags.Zero = NES_X == 0;
            Flags.Negative = NES_X > 127;
            Cycles = 2;
            break;
        case 0xCB: // SBX Immediate
            // Temp_High = NES_A;
            NES_X &= NES_A;
            SmallTemp = Read(ProgramCounter);
            Flags.Carry = NES_X >= SmallTemp;
            NES_X -= SmallTemp;
            // Flags.Carry = Temp > 255;
            // Flags.Overflow = (~(NES_X ^ SmallTemp) & (NES_X ^ Temp) & 0x80) > 0;
            // NES_X = Temp;
            Flags.Negative = NES_X > 127;
            Flags.Zero = NES_X == 0;
            // NES_X = NES_A;
            // NES_A = Temp_High;
            ProgramCounter++;
            Cycles = 2;
            break;
        case 0xCC: // CPY Absolute
            ReadOperands_AbsoluteAddressed();
            Op_CPY(Read(addressBus));
            Cycles = 4;
            break;
        case 0xCD: // CMP Absolute
            ReadOperands_AbsoluteAddressed();
            Op_CMP(Read(addressBus));
            Cycles = 4;
            break;
        case 0xCE: // DEC Absolute
            ReadOperands_AbsoluteAddressed();
            Op_DEC(addressBus, Read(addressBus));
            Cycles = 6;
            break;
        case 0xCF: // DCP Absolute
            ReadOperands_AbsoluteAddressed();
            Op_DCP(addressBus, Read(addressBus));
            Cycles = 6;
            break;
        case 0xD0: // BNE
            SignedTemp = Read(ProgramCounter);
            ProgramCounter++;
            if (Flags.Zero == false) {
                if (SignedTemp > 127) SignedTemp -= 256;
                Temp = ProgramCounter & 0xFF00;
                ProgramCounter = (uint16_t)(ProgramCounter + SignedTemp);
                if ((ProgramCounter & 0xFF00) == Temp) Cycles = 3;
                else Cycles = 4;
            } else {
                Cycles = 2;
            }
            break;
        case 0xD1: // CMP Zero Page Indirect Y-Indexed
            ReadOperands_IndirectAddressed_YIndexed();
            Op_CMP(Read(addressBus));
            Cycles = 5; // TODO: +1 if page crossed
            break;
        case 0xD3: // DCP Y-Indexed Zero Page Indirect
            ReadOperands_IndirectAddressed_YIndexed();
            Op_DCP(addressBus, Read(addressBus));
            Cycles = 8;
            break;
        case 0xD4: // NOP X-Indexed Zero Page
            ReadOperands_ZeroPageAddressed_XIndexed();
            OpenBus = Read(addressBus);
            Cycles = 4;
            // ProgramCounter++;
            break;
        case 0xD5: // CMP X-Indexed Zero Page
            ReadOperands_ZeroPageAddressed_XIndexed();
            Op_CMP(Read(addressBus));
            Cycles = 4;
            break;
        case 0xD6: // DEC X-Indexed Zero Page
            ReadOperands_ZeroPageAddressed_XIndexed();
            Op_DEC(addressBus, Read(addressBus));
            Cycles = 6;
            break;
        case 0xD7: // DCP X-Indexed Zero Page
            ReadOperands_ZeroPageAddressed_XIndexed();
            Op_DCP(addressBus, Read(addressBus));
            Cycles = 6;
            break;
        case 0xD8: // CLD
            Flags.Decimal = false;
            Cycles = 2;
            break;
        case 0xD9: // CMP Y-Indexed Absolute
            ReadOperands_AbsoluteAddressed_YIndexed();
            Op_CMP(Read(addressBus));
            Cycles = 4; // TODO: +1 if page crossed
            break;
        case 0xDA: // NOP
            Cycles = 2;
            break;
        case 0xDB: // DCP Y-Indexed Absolute
            ReadOperands_AbsoluteAddressed_YIndexed();
            Op_DCP(addressBus, Read(addressBus));
            Cycles = 7;
            break;
        case 0xDC: // NOP X-Indexed Absolute
            ReadOperands_AbsoluteAddressed_XIndexed();
            OpenBus = Read(addressBus);
            Cycles = 4;
            // ProgramCounter += 2;
            break;
        case 0xDD: // CMP X-Indexed Absolute
            ReadOperands_AbsoluteAddressed_XIndexed();
            Op_CMP(Read(addressBus));
            Cycles = 4; // TODO: +1 if page crossed
            break;
        case 0xDE: // DEC X-Indexed Absolute
            ReadOperands_AbsoluteAddressed_XIndexed();
            Op_DEC(addressBus, Read(addressBus));
            Cycles = 7;
            break;
        case 0xDF: // DCP X-Indexed Absolute
            ReadOperands_AbsoluteAddressed_XIndexed();
            Op_DCP(addressBus, Read(addressBus));
            Cycles = 7;
            break;
        case 0xE0: // CPX Immediate
            Op_CPX(Read(ProgramCounter));
            ProgramCounter++;
            Cycles = 2;
            break;
        case 0xE1: // SBC X-Indexed Zero Page Indirect
            ReadOperands_IndirectAddressed_XIndexed();
            Op_SBC(Read(addressBus));
            Cycles = 6;
            break;
        case 0xE2: // NOP Immediate
            OpenBus = Read(ProgramCounter);
            Cycles = 2;
            ProgramCounter++;
            break;
        case 0xE3: // ISC X-Indexed Zero Page Indirect
            ReadOperands_IndirectAddressed_XIndexed();
            Op_ISC(addressBus, Read(addressBus));
            Cycles = 8;
            break;
        case 0xE4: // CPX Zero Page
            ReadOperands_ZeroPageAddressed();
            Op_CPX(Read(addressBus));
            Cycles = 3;
            break;
        case 0xE5: // SBC Zero Page
            ReadOperands_ZeroPageAddressed();
            Op_SBC(Read(addressBus));
            Cycles = 3;
            break;
        case 0xE6: // INC Zero Page
            ReadOperands_ZeroPageAddressed();
            Op_INC(addressBus, Read(addressBus));
            Cycles = 5;
            break;
        case 0xE7: // ISC Zero Page
            ReadOperands_ZeroPageAddressed();
            Op_ISC(addressBus, Read(addressBus));
            Cycles = 5;
            break;
        case 0xE8: // INX
            NES_X++;
            Flags.Zero = NES_X == 0;
            Flags.Negative = NES_X > 127;
            Cycles = 2;
            break;
        case 0xE9: // SBC Immediate
            Op_SBC(Read(ProgramCounter));
            ProgramCounter++;
            Cycles = 2;
            break;
        case 0xEA: // NOP
            Cycles = 2;
            break;
        case 0xEB: // SBC Immediate
            Op_SBC(Read(ProgramCounter));
            ProgramCounter++;
            Cycles = 2;
            break;
        case 0xEC: // CPX Absolute
            ReadOperands_AbsoluteAddressed();
            Op_CPX(Read(addressBus));
            Cycles = 4;
            break;
        case 0xED: // SBC Absolute
            ReadOperands_AbsoluteAddressed();
            Op_SBC(Read(addressBus));
            Cycles = 4;
            break;
        case 0xEE: // INC Absolute
            ReadOperands_AbsoluteAddressed();
            Op_INC(addressBus, Read(addressBus));
            Cycles = 6;
            break;
        case 0xEF: // ISC Absolute
            ReadOperands_AbsoluteAddressed();
            Op_ISC(addressBus, Read(addressBus));
            Cycles = 6;
            break;
        case 0xF0: // BEQ
            SignedTemp = Read(ProgramCounter);
            ProgramCounter++;
            if (Flags.Zero == true) {
                if (SignedTemp > 127) SignedTemp -= 256;
                Temp = ProgramCounter & 0xFF00;
                ProgramCounter = (uint16_t)(ProgramCounter + SignedTemp);
                if ((ProgramCounter & 0xFF00) == Temp) Cycles = 3;
                else Cycles = 4;
            } else {
                Cycles = 2;
            }
            break;
        case 0xF1: // SBC Zero Page Indirect Y-Indexed
            ReadOperands_IndirectAddressed_YIndexed();
            Op_SBC(Read(addressBus));
            Cycles = 5; // TODO: +1 if page crossed
            break;
        case 0xF3: // ISC Y-Indexed Zero Page Indirect
            ReadOperands_IndirectAddressed_YIndexed();
            Op_ISC(addressBus, Read(addressBus));
            Cycles = 8;
            break;
        case 0xF4: // NOP X-Indexed Zero Page
            ReadOperands_ZeroPageAddressed_XIndexed();
            OpenBus = Read(addressBus);
            Cycles = 4;
            // ProgramCounter++;
            break;
        case 0xF5: // SBC X-Indexed Zero Page
            ReadOperands_ZeroPageAddressed_XIndexed();
            Op_SBC(Read(addressBus));
            Cycles = 4;
            break;
        case 0xF6: // INC X-Indexed Zero Page
            ReadOperands_ZeroPageAddressed_XIndexed();
            Op_INC(addressBus, Read(addressBus));
            Cycles = 6;
            break;
        case 0xF7: // ISC X-Indexed Zero Page
            ReadOperands_ZeroPageAddressed_XIndexed();
            Op_ISC(addressBus, Read(addressBus));
            Cycles = 6;
            break;
        case 0xF8: // SED
            Flags.Decimal = true;
            Cycles = 2;
            break;
        case 0xF9: // SBC Y-Indexed Absolute
            ReadOperands_AbsoluteAddressed_YIndexed();
            Op_SBC(Read(addressBus));
            Cycles = 4; // TODO: +1 if page crossed
            break;
        case 0xFA: // NOP
            Cycles = 2;
            break;
        case 0xFB: // ISC Y-Indexed Absolute
            ReadOperands_AbsoluteAddressed_YIndexed();
            Op_ISC(addressBus, Read(addressBus));
            Cycles = 7;
            break;
        case 0xFC: // NOP X-Indexed Absolute
            ReadOperands_AbsoluteAddressed_XIndexed();
            OpenBus = Read(addressBus);
            Cycles = 4;
            // ProgramCounter += 2;
            break;
        case 0xFD: // SBC X-Indexed Absolute
            ReadOperands_AbsoluteAddressed_XIndexed();
            Op_SBC(Read(addressBus));
            Cycles = 4; // TODO: +1 if page crossed
            break;
        case 0xFE: // INC X-Indexed Absolute
            ReadOperands_AbsoluteAddressed_XIndexed();
            Op_INC(addressBus, Read(addressBus));
            Cycles = 7;
            break;
        case 0xFF: // ISC X-Indexed Absolute
            ReadOperands_AbsoluteAddressed_XIndexed();
            Op_ISC(addressBus, Read(addressBus));
            Cycles = 7;
            break;

        default:
            break;
    }
}

void PPU_IncrementScrollY() {
    if ((VRAMAddress & 0x7000) != 0x7000) {
        VRAMAddress += 0x1000;
    } else {
        VRAMAddress &= 0x0FFF;
        uint16_t y = (VRAMAddress & 0x03E0) >> 5;
        if (y == 29) {
            y = 0;
            VRAMAddress ^= 0x0800;
        } else {
            y++;
            y &= 0x1F;
        }
        VRAMAddress = (uint16_t)((VRAMAddress & 0xFC1F) | (y << 5));
    }
}

void PPU_ResetXScroll() {
    VRAMAddress = (uint16_t)((VRAMAddress & 0b0111101111100000) | (TransferAddress & 0b0000010000011111));
}

void PPU_ResetYScroll() {
    VRAMAddress = (uint16_t)((VRAMAddress & 0b0000010000011111) | (TransferAddress & 0b0111101111100000));
}

uint16_t GetSpritePatternAddress(uint8_t SecondaryOAMSlot) {
    if (!PPUCTRL.SpriteSize) {
        if (((ppu_SpriteAttribute[SecondaryOAMSlot] >> 7) & 1) == 0) {
            return (uint16_t)((PPUCTRL.SpriteTable ? 0x1000 : 0) + (ppu_SpritePattern[SecondaryOAMSlot] << 4) + (ppuScanline - ppu_SpriteYposition[SecondaryOAMSlot]));
        } else {
            return (uint16_t)((PPUCTRL.SpriteTable ? 0x1000 : 0) + (ppu_SpritePattern[SecondaryOAMSlot] << 4) + ((7 - (ppuScanline - ppu_SpriteYposition[SecondaryOAMSlot])) & 7));
        }
    } else {
        if (((ppu_SpriteAttribute[SecondaryOAMSlot] >> 7) & 1) == 0) {
            if ((ppuScanline - ppu_SpriteYposition[SecondaryOAMSlot]) < 8) {
                return (uint16_t)((((ppu_SpritePattern[SecondaryOAMSlot] & 1) == 1) ? 0x1000 : 0) | ((ppu_SpritePattern[SecondaryOAMSlot] & 0xFE) << 4) + (ppuScanline - ppu_SpriteYposition[SecondaryOAMSlot]));
            } else {
                return (uint16_t)((((ppu_SpritePattern[SecondaryOAMSlot] & 1) == 1) ? 0x1000 : 0) | (((ppu_SpritePattern[SecondaryOAMSlot] & 0xFE) << 4) + 16) + ((ppuScanline - ppu_SpriteYposition[SecondaryOAMSlot]) & 7));
            }
        } else {
            if ((ppuScanline - ppu_SpriteYposition[SecondaryOAMSlot]) < 8) {
                return (uint16_t)((((ppu_SpritePattern[SecondaryOAMSlot] & 1) == 1) ? 0x1000 : 0) | (((ppu_SpritePattern[SecondaryOAMSlot] & 0xFE) << 4) + 16) - ((ppuScanline - ppu_SpriteYposition[SecondaryOAMSlot]) & 7) + 7);
            } else {
                return (uint16_t)((((ppu_SpritePattern[SecondaryOAMSlot] & 1) == 1) ? 0x1000 : 0) | (((ppu_SpritePattern[SecondaryOAMSlot] & 0xFE) << 4) + 7) - ((ppuScanline - ppu_SpriteYposition[SecondaryOAMSlot]) & 7));
            }
        }
    }
}

void SpriteEvaluation() {
    if (ppuDot == 0) {
        memset(SecondaryOAM, 0xFF, sizeof(SecondaryOAM));
        ppuSecondaryOAMAddress = 0;
        ppuSecondaryOAMFull = false;
        ppuSpriteEvaluationOAMOverflowed = false;
        ppuNextScanlineContainsSpriteZero = false;
    }
    else if (ppuDot > 64 && ppuDot <= 256) {
        if ((ppuDot & 1) == 1) {
            ppuSpriteEvalTemp = OAM[ppuOAMAddress];
        } else {
            if (!ppuSpriteEvaluationOAMOverflowed) {
                if (!ppuSecondaryOAMFull) {
                    SecondaryOAM[ppuSecondaryOAMAddress] = ppuSpriteEvalTemp;
                }
                if (ppuSpriteEvalTick == 0) {
                    if (((ppuScanline - ppuSpriteEvalTemp) >= 0) && ((ppuScanline - ppuSpriteEvalTemp) < (PPUCTRL.SpriteSize ? 16 : 8))) {
                        if (!ppuSecondaryOAMFull) {
                            ppuSecondaryOAMAddress++;
                            ppuOAMAddress++;
                            if (ppuDot == 66) {
                                ppuNextScanlineContainsSpriteZero = true;
                                // printf("0");
                            }
                        } else {
                            PPUSTATUS.SpriteOverflow = true;
                        }
                        ppuSpriteEvalTick++;
                    } else {
                        ppuOAMAddress += 4;
                    }
                } else {
                    ppuSecondaryOAMAddress++;
                    ppuOAMAddress++;
                    if (ppuSecondaryOAMAddress == 0x20) {
                        ppuSecondaryOAMFull = true;
                    }
                    ppuSpriteEvalTick++;
                    ppuSpriteEvalTick &= 3;
                }
                if (ppuOAMAddress == 0) {
                    ppuSpriteEvaluationOAMOverflowed = true;
                }
            }
        }
    }
    else if (ppuDot > 256 && ppuDot <= 320) {
        ppuOAMAddress = 0;
        ppuCurrentScanlineContainsSpriteZero = ppuNextScanlineContainsSpriteZero;;
        if (ppuDot == 257) {
            ppuSecondaryOAMSize = ppuSecondaryOAMAddress;
            ppuSecondaryOAMAddress = 0;
            ppuSpriteEvalTick = 0;
        }

        switch (ppuSpriteEvalTick) {
            case 0:
                ppu_SpriteYposition[ppuSecondaryOAMAddress / 4] = SecondaryOAM[ppuSecondaryOAMAddress];
                ppuSecondaryOAMAddress++;
                break;
            case 1:
                ppu_SpritePattern[ppuSecondaryOAMAddress / 4] = SecondaryOAM[ppuSecondaryOAMAddress];
                ppuSecondaryOAMAddress++;
                break;
            case 2:
                ppu_SpriteAttribute[ppuSecondaryOAMAddress / 4] = SecondaryOAM[ppuSecondaryOAMAddress];
                ppuSecondaryOAMAddress++;
                break;
            case 3:
                ppu_SpriteXposition[ppuSecondaryOAMAddress / 4] = SecondaryOAM[ppuSecondaryOAMAddress];
                break;
            case 4:
                ppuAddressBus = GetSpritePatternAddress(ppuSecondaryOAMAddress / 4); // TODO: Na pewno?
                break;
            case 5:
                ppuSpriteEvalTemp = ReadPPU(ppuAddressBus);
                if (ppuScanline == 261) {
                    ppuSpriteEvalTemp = 0;
                }
                if (((ppu_SpriteAttribute[ppuSecondaryOAMAddress / 4] >> 6) & 1) == 1) {
                    ppuSpriteEvalTemp = (uint8_t)(((ppuSpriteEvalTemp & 0xF0) >> 4) | ((ppuSpriteEvalTemp & 0x0F) << 4));
                    ppuSpriteEvalTemp = (uint8_t)(((ppuSpriteEvalTemp & 0xCC) >> 2) | ((ppuSpriteEvalTemp & 0x33) << 2));
                    ppuSpriteEvalTemp = (uint8_t)(((ppuSpriteEvalTemp & 0xAA) >> 1) | ((ppuSpriteEvalTemp & 0x55) << 1));
                }
                ppu_SpriteShiftRegisterL[ppuSecondaryOAMAddress / 4] = ppuSpriteEvalTemp;
                break;
            case 6:
                ppuAddressBus += 8;
                break;
            case 7:
                ppuSpriteEvalTemp = ReadPPU(ppuAddressBus);
                if (ppuScanline == 261) {
                    ppuSpriteEvalTemp = 0;
                }
                if (((ppu_SpriteAttribute[ppuSecondaryOAMAddress / 4] >> 6) & 1) == 1) {
                    ppuSpriteEvalTemp = (uint8_t)(((ppuSpriteEvalTemp & 0xF0) >> 4) | ((ppuSpriteEvalTemp & 0x0F) << 4));
                    ppuSpriteEvalTemp = (uint8_t)(((ppuSpriteEvalTemp & 0xCC) >> 2) | ((ppuSpriteEvalTemp & 0x33) << 2));
                    ppuSpriteEvalTemp = (uint8_t)(((ppuSpriteEvalTemp & 0xAA) >> 1) | ((ppuSpriteEvalTemp & 0x55) << 1));
                }
                ppu_SpriteShiftRegisterH[ppuSecondaryOAMAddress / 4] = ppuSpriteEvalTemp;
                ppuSecondaryOAMAddress++;
                break;
        }
        ppuSpriteEvalTick++;
        ppuSpriteEvalTick &= 7;
    }
}
#ifndef NES_FAST
void Emulate_PPU() {

    if (ppuDot == 1 && ppuScanline == 241) {
        PPUSTATUS.VBlank = true;
        DrawNewFrame = true;
        // SaveNESBufferBMP("nesbuffer.bmp");
    }

    if (ppuDot == 1 && ppuScanline == 261) {
        PPUSTATUS.VBlank = false;
        PPUSTATUS.SpriteOverflow = false;
        PPUSTATUS.Sprite0Hit = false;
        // DrawNewFrame = true;
        // printf(" %d C ", ppuScanline);
    }

    if (((ppuScanline < 240) || (ppuScanline == 261)) && (PPUMASK.EnableBackground || PPUMASK.EnableSprites)) {
        SpriteEvaluation();
    }

    if ((ppuScanline < 240) || (ppuScanline == 261)) {
        if (PPUMASK.EnableBackground || PPUMASK.EnableSprites) {
            if (ppuDot > 1 && ppuDot <= 256) {
                for (uint8_t i = 0; i < 8; i++) {
                    if (ppu_SpriteXposition[i] > 0) {
                        ppu_SpriteXposition[i]--;
                    } else {
                        ppu_SpriteShiftRegisterL[i] = (uint8_t)(ppu_SpriteShiftRegisterL[i] << 1);
                        ppu_SpriteShiftRegisterH[i] = (uint8_t)(ppu_SpriteShiftRegisterH[i] << 1);
                    }
                }
            }
            if ((ppuDot > 0 && ppuDot <= 256) || (ppuDot > 320 && ppuDot <= 336)) {
                if (PPUMASK.EnableBackground) {
                    ppuShiftRegister_patternL = (uint16_t)(ppuShiftRegister_patternL << 1);
                    ppuShiftRegister_patternH = (uint16_t)(ppuShiftRegister_patternH << 1);
                    ppuShiftRegister_attributeL = (uint16_t)(ppuShiftRegister_attributeL << 1);
                    ppuShiftRegister_attributeH = (uint16_t)(ppuShiftRegister_attributeH << 1);

                    uint8_t cycleTick = (uint8_t)((ppuDot - 1) & 7);
                    switch (cycleTick) {
                        case 0:
                            ppuShiftRegister_patternL = (uint16_t)((ppuShiftRegister_patternL & 0xFF00) | ppu8Step_patternLowBitPlane);
                            ppuShiftRegister_patternH = (uint16_t)((ppuShiftRegister_patternH & 0xFF00) | ppu8Step_patternHighBitPlane);
                            // if (ppu8Step_patternHighBitPlane > 0) printf("%x %x ", ppu8Step_patternLowBitPlane, ppu8Step_patternHighBitPlane);
                            ppuShiftRegister_attributeL = (uint16_t)((ppuShiftRegister_attributeL & 0xFF00) | ((ppu8Step_attribute & 1) == 1 ? 0xFF : 0));
                            ppuShiftRegister_attributeH = (uint16_t)((ppuShiftRegister_attributeH & 0xFF00) | ((ppu8Step_attribute & 2) == 2 ? 0xFF : 0));
                            ppuAddressBus = (uint16_t)(0x2000 + (VRAMAddress & 0x0FFF));
                            ppu8Step_temp = ReadPPU(ppuAddressBus);
                            break;
                        case 1:
                            ppu8Step_NextCharacter = ppu8Step_temp;
                            break;
                        case 2:
                            ppuAddressBus = (uint16_t)(0x23C0 | (VRAMAddress & 0x0C00) | ((VRAMAddress >> 4) & 0x38) | ((VRAMAddress >> 2) & 0x07));
                            ppu8Step_temp = ReadPPU(ppuAddressBus);
                            break;
                        case 3:
                            ppu8Step_attribute = ppu8Step_temp;
                            if ((VRAMAddress & 3) >= 2) {
                                ppu8Step_attribute = (uint8_t)(ppu8Step_attribute >> 2);
                            }
                            if ((((VRAMAddress & 0b0000001111100000) >> 5) & 3) >= 2) {
                                ppu8Step_attribute = (uint8_t)(ppu8Step_attribute >> 4);
                            }
                            ppu8Step_attribute = (uint8_t)(ppu8Step_attribute & 3);
                            break;
                        case 4:
                            ppuAddressBus = (uint16_t)(((VRAMAddress & 0b0111000000000000) >> 12) | ppu8Step_NextCharacter * 16 | (PPUCTRL.BackgroundTable ? 0x1000 : 0));
                            ppu8Step_temp = ReadPPU(ppuAddressBus);
                            break;
                        case 5:
                            ppu8Step_patternLowBitPlane = ppu8Step_temp;
                            ppuAddressBus += 8;
                            break;
                        case 6:
                            ppu8Step_temp = ReadPPU(ppuAddressBus);
                            // if (ppu8Step_temp > 0) printf("%x %x ", ppu8Step_patternLowBitPlane, ppu8Step_patternHighBitPlane);
                            // printf("%x ", ppuAddressBus);
                            break;
                        case 7:
                            ppu8Step_patternHighBitPlane = ppu8Step_temp;
                            // if (ppu8Step_temp > 0) printf("%x %x ", ppu8Step_patternLowBitPlane, ppu8Step_patternHighBitPlane);
                            // printf("7");
                            if ((VRAMAddress & 0x001F) == 31) {
                                VRAMAddress &= 0xFFE0;
                                VRAMAddress ^= 0x0400;
                            } else {
                                VRAMAddress++;
                            }
                            break;
                    }
                }
            }
            if (ppuDot == 256) {
                PPU_IncrementScrollY();
            }
            else if (ppuDot == 257) {
                PPU_ResetXScroll();
            }
            if (ppuDot >= 280 && ppuDot <= 304 && ppuScanline == 261) {
                PPU_ResetYScroll();
            }
        }
    }

    if (ppuScanline < 241 && ppuDot > 0 && ppuDot <= 256) {
        uint8_t PalHi = 0;
        uint8_t PalLow = 0;
        if (PPUMASK.EnableBackground && (ppuDot > 8 || PPUMASK.ShowBackgroundColumn)) {
            uint8_t col0 = (uint8_t)((ppuShiftRegister_patternL >> (15 - ppuScrollFineX)) & 1);
            uint8_t col1 = (uint8_t)((ppuShiftRegister_patternH >> (15 - ppuScrollFineX)) & 1);
            PalLow = (uint8_t)((col1 << 1) | col0);

            // if (ppuShiftRegister_patternH > 0) printf("%x %x ", ppuShiftRegister_patternL, ppuShiftRegister_patternH);

            uint8_t pal0 = (uint8_t)((ppuShiftRegister_attributeL >> (15 - ppuScrollFineX)) & 1);
            uint8_t pal1 = (uint8_t)((ppuShiftRegister_attributeH >> (15 - ppuScrollFineX)) & 1);
            PalHi = (uint8_t)((pal1 << 1) | pal0);

            if (PalLow == 0 && PalHi != 0) {
                PalHi = 0;
            }
        }

        uint8_t SpritePalHi = 0;
        uint8_t SpritePalLow = 0;
        bool SpritePriority = false;
        if (PPUMASK.EnableSprites && (ppuDot > 8 || PPUMASK.ShowSpriteColumn)) {
            for (uint8_t i = 0; i < 8; i++) {
                if ((ppu_SpriteXposition[i] == 0) && (i < (ppuSecondaryOAMSize / 4))) {
                    bool SpixelL = (ppu_SpriteShiftRegisterL[i] & 0x80) != 0;
                    bool SpixelH = (ppu_SpriteShiftRegisterH[i] & 0x80) != 0;
                    SpritePalLow = 0;
                    if (SpixelL) SpritePalLow = 1;
                    if (SpixelH) SpritePalLow |= 2;

                    SpritePalHi = (uint8_t)((ppu_SpriteAttribute[i] & 0x03) | 0x04);
                    SpritePriority = ((ppu_SpriteAttribute[i] >> 5) & 1) == 0;
                } else {
                    continue;
                }
                if (SpritePalLow != 0) {
                    if (i == 0 && ppuCurrentScanlineContainsSpriteZero && PalLow != 0 && PPUMASK.EnableBackground && ppuDot < 256) {
                        PPUSTATUS.Sprite0Hit = true;
//                        EventBuffer[ppuScanline][ppuDot][0] = 0x00;
//                        EventBuffer[ppuScanline][ppuDot][1] = 0xFF;
//                        EventBuffer[ppuScanline][ppuDot][2] = 0xFF;
                        // printf(" Sprite 0 Hit line: %d dot: %d\n", ppuScanline, ppuDot);
                    }
                    break;
                }
            }
        }

        if ((SpritePriority && SpritePalLow != 0) || PalLow == 0) {
            PalLow = SpritePalLow;
            PalHi = SpritePalHi;
            if (PalLow == 0) PalHi = 0;
        }

        uint8_t *PixelOut = NESBuffer[ppuScanline][ppuDot - 1];
        const uint8_t *Color = Palette[PaletteRAM[PalLow + PalHi * 4]];
        PixelOut[0] = Color[0];
        PixelOut[1] = Color[1];
        PixelOut[2] = Color[2];
    }

    ppuDot++;
    if (ppuDot >= 341) {
        ppuDot = 0;
        ppuScanline++;
        if (ppuScanline > 261) { // NTSC
            ppuScanline = 0;
        }
    }
}
#endif

void PPUFAST_IncrementScrollY() {
    if ((VRAMAddress & 0x7000) != 0x7000) {
        VRAMAddress += 0x1000;
    } else {
        VRAMAddress &= 0x0FFF;
        uint16_t y = (VRAMAddress & 0x03E0) >> 5;
        if (y == 29) {
            y = 0;
            VRAMAddress ^= 0x0800;
        } else {
            y++;
            y &= 0x1F;
        }
        VRAMAddress = (uint16_t)((VRAMAddress & 0xFC1F) | (y << 5));
    }
}

void PPUFAST_ResetXScroll() {
    VRAMAddress = (uint16_t)((VRAMAddress & 0b0111101111100000) | (TransferAddress & 0b0000010000011111));
}

void PPUFAST_ResetYScroll() {
    VRAMAddress = (uint16_t)((VRAMAddress & 0b0000010000011111) | (TransferAddress & 0b0111101111100000));
}

uint16_t GetSpriteFastPatternAddress(uint8_t SecondaryOAMSlot) {
    if (!PPUCTRL.SpriteSize) {
        if (((ppu_SpriteAttribute[SecondaryOAMSlot] >> 7) & 1) == 0) {
            return (uint16_t)((PPUCTRL.SpriteTable ? 0x1000 : 0) + (ppu_SpritePattern[SecondaryOAMSlot] << 4) + (ppuFastScanline - ppu_SpriteYposition[SecondaryOAMSlot]));
        } else {
            return (uint16_t)((PPUCTRL.SpriteTable ? 0x1000 : 0) + (ppu_SpritePattern[SecondaryOAMSlot] << 4) + ((7 - (ppuFastScanline - ppu_SpriteYposition[SecondaryOAMSlot])) & 7));
        }
    } else {
        if (((ppu_SpriteAttribute[SecondaryOAMSlot] >> 7) & 1) == 0) {
            if ((ppuFastScanline - ppu_SpriteYposition[SecondaryOAMSlot]) < 8) {
                return (uint16_t)((((ppu_SpritePattern[SecondaryOAMSlot] & 1) == 1) ? 0x1000 : 0) | ((ppu_SpritePattern[SecondaryOAMSlot] & 0xFE) << 4) + (ppuFastScanline - ppu_SpriteYposition[SecondaryOAMSlot]));
            } else {
                return (uint16_t)((((ppu_SpritePattern[SecondaryOAMSlot] & 1) == 1) ? 0x1000 : 0) | (((ppu_SpritePattern[SecondaryOAMSlot] & 0xFE) << 4) + 16) + ((ppuFastScanline - ppu_SpriteYposition[SecondaryOAMSlot]) & 7));
            }
        } else {
            if ((ppuFastScanline - ppu_SpriteYposition[SecondaryOAMSlot]) < 8) {
                return (uint16_t)((((ppu_SpritePattern[SecondaryOAMSlot] & 1) == 1) ? 0x1000 : 0) | (((ppu_SpritePattern[SecondaryOAMSlot] & 0xFE) << 4) + 16) - ((ppuFastScanline - ppu_SpriteYposition[SecondaryOAMSlot]) & 7) + 7);
            } else {
                return (uint16_t)((((ppu_SpritePattern[SecondaryOAMSlot] & 1) == 1) ? 0x1000 : 0) | (((ppu_SpritePattern[SecondaryOAMSlot] & 0xFE) << 4) + 7) - ((ppuFastScanline - ppu_SpriteYposition[SecondaryOAMSlot]) & 7));
            }
        }
    }
}

void SpriteFastEvaluation() {
    if (ppuDot == 0) {
        memset(SecondaryOAM, 0xFF, sizeof(SecondaryOAM));
        ppuSecondaryOAMAddress = 0;
        ppuSecondaryOAMFull = false;
        ppuSpriteEvaluationOAMOverflowed = false;
        ppuNextScanlineContainsSpriteZero = false;
    }
    else if (ppuDot > 64 && ppuDot <= 256) {
        if ((ppuDot & 1) == 1) {
            ppuSpriteEvalTemp = OAM[ppuOAMAddress];
        } else {
            if (!ppuSpriteEvaluationOAMOverflowed) {
                if (!ppuSecondaryOAMFull) {
                    SecondaryOAM[ppuSecondaryOAMAddress] = ppuSpriteEvalTemp;
                }
                if (ppuSpriteEvalTick == 0) {
                    if (((ppuScanline - ppuSpriteEvalTemp) >= 0) && ((ppuScanline - ppuSpriteEvalTemp) < (PPUCTRL.SpriteSize ? 16 : 8))) {
                        if (!ppuSecondaryOAMFull) {
                            ppuSecondaryOAMAddress++;
                            ppuOAMAddress++;
                            if (ppuDot == 66) {
                                ppuNextScanlineContainsSpriteZero = true;
                                // printf("0");
                            }
                        } else {
                            PPUSTATUS.SpriteOverflow = true;
                        }
                        ppuSpriteEvalTick++;
                    } else {
                        ppuOAMAddress += 4;
                    }
                } else {
                    ppuSecondaryOAMAddress++;
                    ppuOAMAddress++;
                    if (ppuSecondaryOAMAddress == 0x20) {
                        ppuSecondaryOAMFull = true;
                    }
                    ppuSpriteEvalTick++;
                    ppuSpriteEvalTick &= 3;
                }
                if (ppuOAMAddress == 0) {
                    ppuSpriteEvaluationOAMOverflowed = true;
                }
            }
        }
    }
    else if (ppuDot > 256 && ppuDot <= 320) {
        ppuOAMAddress = 0;
        ppuCurrentScanlineContainsSpriteZero = ppuNextScanlineContainsSpriteZero;;
        if (ppuDot == 257) {
            ppuSecondaryOAMSize = ppuSecondaryOAMAddress;
            ppuSecondaryOAMAddress = 0;
            ppuSpriteEvalTick = 0;
        }

        switch (ppuSpriteEvalTick) {
            case 0:
                ppu_SpriteYposition[ppuSecondaryOAMAddress / 4] = SecondaryOAM[ppuSecondaryOAMAddress];
                ppuSecondaryOAMAddress++;
                break;
            case 1:
                ppu_SpritePattern[ppuSecondaryOAMAddress / 4] = SecondaryOAM[ppuSecondaryOAMAddress];
                ppuSecondaryOAMAddress++;
                break;
            case 2:
                ppu_SpriteAttribute[ppuSecondaryOAMAddress / 4] = SecondaryOAM[ppuSecondaryOAMAddress];
                ppuSecondaryOAMAddress++;
                break;
            case 3:
                ppu_SpriteXposition[ppuSecondaryOAMAddress / 4] = SecondaryOAM[ppuSecondaryOAMAddress];
                break;
            case 4:
                ppuAddressBus = GetSpriteFastPatternAddress(ppuSecondaryOAMAddress / 4); // TODO: Na pewno?
                break;
            case 5:
                ppuSpriteEvalTemp = ReadPPU(ppuAddressBus);
                if (ppuScanline == 261) {
                    ppuSpriteEvalTemp = 0;
                }
                if (((ppu_SpriteAttribute[ppuSecondaryOAMAddress / 4] >> 6) & 1) == 1) {
                    ppuSpriteEvalTemp = (uint8_t)(((ppuSpriteEvalTemp & 0xF0) >> 4) | ((ppuSpriteEvalTemp & 0x0F) << 4));
                    ppuSpriteEvalTemp = (uint8_t)(((ppuSpriteEvalTemp & 0xCC) >> 2) | ((ppuSpriteEvalTemp & 0x33) << 2));
                    ppuSpriteEvalTemp = (uint8_t)(((ppuSpriteEvalTemp & 0xAA) >> 1) | ((ppuSpriteEvalTemp & 0x55) << 1));
                }
                ppu_SpriteShiftRegisterL[ppuSecondaryOAMAddress / 4] = ppuSpriteEvalTemp;
                break;
            case 6:
                ppuAddressBus += 8;
                break;
            case 7:
                ppuSpriteEvalTemp = ReadPPU(ppuAddressBus);
                if (ppuScanline == 261) {
                    ppuSpriteEvalTemp = 0;
                }
                if (((ppu_SpriteAttribute[ppuSecondaryOAMAddress / 4] >> 6) & 1) == 1) {
                    ppuSpriteEvalTemp = (uint8_t)(((ppuSpriteEvalTemp & 0xF0) >> 4) | ((ppuSpriteEvalTemp & 0x0F) << 4));
                    ppuSpriteEvalTemp = (uint8_t)(((ppuSpriteEvalTemp & 0xCC) >> 2) | ((ppuSpriteEvalTemp & 0x33) << 2));
                    ppuSpriteEvalTemp = (uint8_t)(((ppuSpriteEvalTemp & 0xAA) >> 1) | ((ppuSpriteEvalTemp & 0x55) << 1));
                }
                ppu_SpriteShiftRegisterH[ppuSecondaryOAMAddress / 4] = ppuSpriteEvalTemp;
                ppuSecondaryOAMAddress++;
                break;
        }
        ppuSpriteEvalTick++;
        ppuSpriteEvalTick &= 7;
    }
}

void Emulate_PPU_Fast() {
    while (Cycles >= 4) {
        Cycles -= 4;

        if (ppuFastDot == 0 && ppuFastScanline == 241) {
            PPUSTATUS.VBlank = true;
            DrawNewFrame = true;
        }

        if (ppuFastDot == 0 && ppuFastScanline == 261) {
            PPUSTATUS.VBlank = false;
            PPUSTATUS.SpriteOverflow = false;
            PPUSTATUS.Sprite0Hit = false;
        }

        if (((ppuFastScanline < 240) || (ppuFastScanline == 261)) && (PPUMASK.EnableBackground || PPUMASK.EnableSprites)) {
            SpriteFastEvaluation();
        }

        if ((ppuFastScanline < 240) || (ppuFastScanline == 261)) {
            if (PPUMASK.EnableBackground || PPUMASK.EnableSprites) {
                if (ppuFastDot <= 63) {
                    for (uint8_t i = 0; i < 8; i++) {
                        if (ppu_SpriteXposition[i] > 0) {
                            ppu_SpriteXposition[i] -= 4;
                        } else {
                            ppu_SpriteShiftRegisterL[i] = (uint8_t)(ppu_SpriteShiftRegisterL[i] << 4);
                            ppu_SpriteShiftRegisterH[i] = (uint8_t)(ppu_SpriteShiftRegisterH[i] << 4);
                        }
                    }
                }
                if ((ppuFastDot <= 63) || (ppuFastDot > 80 && ppuFastDot <= 84)) {
                    if (PPUMASK.EnableBackground) {
                        ppuShiftRegister_patternL = (uint16_t)(ppuShiftRegister_patternL << 4);
                        ppuShiftRegister_patternH = (uint16_t)(ppuShiftRegister_patternH << 4);
                        ppuShiftRegister_attributeL = (uint16_t)(ppuShiftRegister_attributeL << 4);
                        ppuShiftRegister_attributeH = (uint16_t)(ppuShiftRegister_attributeH << 4);

                        uint8_t cycleTick = (uint8_t)(ppuFastDot & 1);
                        switch (cycleTick) {
                            case 0:
                                ppuShiftRegister_patternL = (uint16_t)((ppuShiftRegister_patternL & 0xFF00) | ppu8Step_patternLowBitPlane);
                                ppuShiftRegister_patternH = (uint16_t)((ppuShiftRegister_patternH & 0xFF00) | ppu8Step_patternHighBitPlane);
                                // if (ppu8Step_patternHighBitPlane > 0) printf("%x %x ", ppu8Step_patternLowBitPlane, ppu8Step_patternHighBitPlane);
                                ppuShiftRegister_attributeL = (uint16_t)((ppuShiftRegister_attributeL & 0xFF00) | ((ppu8Step_attribute & 1) == 1 ? 0xFF : 0));
                                ppuShiftRegister_attributeH = (uint16_t)((ppuShiftRegister_attributeH & 0xFF00) | ((ppu8Step_attribute & 2) == 2 ? 0xFF : 0));
                                ppuAddressBus = (uint16_t)(0x2000 + (VRAMAddress & 0x0FFF));
                                ppu8Step_temp = ReadPPU(ppuAddressBus);

                                ppu8Step_NextCharacter = ppu8Step_temp;

                                ppuAddressBus = (uint16_t)(0x23C0 | (VRAMAddress & 0x0C00) | ((VRAMAddress >> 4) & 0x38) | ((VRAMAddress >> 2) & 0x07));
                                ppu8Step_temp = ReadPPU(ppuAddressBus);

                                ppu8Step_attribute = ppu8Step_temp;
                                if ((VRAMAddress & 3) >= 2) {
                                    ppu8Step_attribute = (uint8_t)(ppu8Step_attribute >> 2);
                                }
                                if ((((VRAMAddress & 0b0000001111100000) >> 5) & 3) >= 2) {
                                    ppu8Step_attribute = (uint8_t)(ppu8Step_attribute >> 4);
                                }
                                ppu8Step_attribute = (uint8_t)(ppu8Step_attribute & 3);
                                break;
                            case 1:
                                ppuAddressBus = (uint16_t)(((VRAMAddress & 0b0111000000000000) >> 12) | ppu8Step_NextCharacter * 16 | (PPUCTRL.BackgroundTable ? 0x1000 : 0));
                                ppu8Step_temp = ReadPPU(ppuAddressBus);

                                ppu8Step_patternLowBitPlane = ppu8Step_temp;
                                ppuAddressBus += 8;

                                ppu8Step_temp = ReadPPU(ppuAddressBus);
                                // if (ppu8Step_temp > 0) printf("%x %x ", ppu8Step_patternLowBitPlane, ppu8Step_patternHighBitPlane);
                                // printf("%x ", ppuAddressBus);

                                ppu8Step_patternHighBitPlane = ppu8Step_temp;
                                // if (ppu8Step_temp > 0) printf("%x %x ", ppu8Step_patternLowBitPlane, ppu8Step_patternHighBitPlane);
                                // printf("7");
                                if ((VRAMAddress & 0x001F) == 31) {
                                    VRAMAddress &= 0xFFE0;
                                    VRAMAddress ^= 0x0400;
                                } else {
                                    VRAMAddress++;
                                }
                                break;
                        }
                    }
                }
                if (ppuFastDot == 63) {
                    PPU_IncrementScrollY();
                }
                else if (ppuFastDot == 64) {
                    PPU_ResetXScroll();
                }
                if (ppuFastDot >= 70 && ppuFastDot <= 76 && ppuFastScanline == 261) {
                    PPU_ResetYScroll();
                }
            }
        }

        if (ppuFastScanline < 241 && ppuFastDot < 64) {
            uint8_t PalHi = 0;
            uint8_t PalLow = 0;
            if (PPUMASK.EnableBackground && (ppuFastDot >= 2 || PPUMASK.ShowBackgroundColumn)) {
                uint8_t col0 = (uint8_t)((ppuShiftRegister_patternL >> (15 - ppuScrollFineX)) & 1);
                uint8_t col1 = (uint8_t)((ppuShiftRegister_patternH >> (15 - ppuScrollFineX)) & 1);
                PalLow = (uint8_t)((col1 << 1) | col0);

                // if (ppuShiftRegister_patternH > 0) printf("%x %x ", ppuShiftRegister_patternL, ppuShiftRegister_patternH);

                uint8_t pal0 = (uint8_t)((ppuShiftRegister_attributeL >> (15 - ppuScrollFineX)) & 1);
                uint8_t pal1 = (uint8_t)((ppuShiftRegister_attributeH >> (15 - ppuScrollFineX)) & 1);
                PalHi = (uint8_t)((pal1 << 1) | pal0);

                if (PalLow == 0 && PalHi != 0) {
                    PalHi = 0;
                }
            }

            uint8_t SpritePalHi = 0;
            uint8_t SpritePalLow = 0;
            bool SpritePriority = false;
            if (PPUMASK.EnableSprites && (ppuFastDot >= 2 || PPUMASK.ShowSpriteColumn)) {
                for (uint8_t i = 0; i < 8; i++) {
                    if ((ppu_SpriteXposition[i] <= 0) && (i < (ppuSecondaryOAMSize / 4))) {
                        bool SpixelL = (ppu_SpriteShiftRegisterL[i] & 0x80) != 0;
                        bool SpixelH = (ppu_SpriteShiftRegisterH[i] & 0x80) != 0;
                        SpritePalLow = 0;
                        if (SpixelL) SpritePalLow = 1;
                        if (SpixelH) SpritePalLow |= 2;

                        SpritePalHi = (uint8_t)((ppu_SpriteAttribute[i] & 0x03) | 0x04);
                        SpritePriority = ((ppu_SpriteAttribute[i] >> 5) & 1) == 0;
                    } else {
                        continue;
                    }
                    if (SpritePalLow != 0) {
                        if (i == 0 && ppuCurrentScanlineContainsSpriteZero && PalLow != 0 && PPUMASK.EnableBackground) {
                            PPUSTATUS.Sprite0Hit = true;
//                            EventBuffer[ppuFastScanline][ppuFastDot][0] = 0x00;
//                            EventBuffer[ppuFastScanline][ppuFastDot][1] = 0xFF;
//                            EventBuffer[ppuFastScanline][ppuFastDot][2] = 0xFF;
                            // printf(" Sprite 0 Hit line: %d dot: %d\n", ppuFastScanline, ppuFastDot);
                        }
                        break;
                    }
                }
            }

            if ((SpritePriority && SpritePalLow != 0) || PalLow == 0) {
                PalLow = SpritePalLow;
                PalHi = SpritePalHi;
                if (PalLow == 0) PalHi = 0;
            }

            uint16_t *PixelOut = NESBufferFast[ppuFastScanline >> 3][ppuFastDot];
            const uint8_t *Color = Palette[PaletteRAM[PalLow + PalHi * 4]];
            PixelOut[0] += Color[0];
            PixelOut[1] += Color[1];
            PixelOut[2] += Color[2];
        }

        ppuFastDot++;
        if (ppuFastDot > 85) { // 85 x 4 = 340
            ppuFastDot = 0;
            ppuFastScanline++;
            if (ppuFastScanline > 261) { // NTSC
            // if (ppuFastScanline > 33) { // 33 x 8 = 264 ~ NTSC
                ppuFastScanline = 0;
            }
        }
    }
    CyclesLeft = Cycles;
}

#ifndef NES_FAST
void NES_Run() {
	// tempTime = HAL_GetTick();
    Emulate_CPU();
    // tempTime2 = HAL_GetTick();
    // CPU_Time += tempTime2 - tempTime;
    // tempTime2 = HAL_GetTick();
    // Cycles = Cycles + AddOAMCycles + AddDMCCycles;
    Cycles = Cycles + AddOAMCycles;
    AddOAMCycles = 0;
    // AddDMCCycles = 0;
    while (Cycles > 0) {
        Cycles--;

        AddAlignmentCycle = !AddAlignmentCycle;

        Emulate_APU();

        Emulate_PPU();
        Emulate_PPU();
        Emulate_PPU();
    }
    // tempTime = HAL_GetTick();
    // PPU_Time += tempTime - tempTime2;
}
#endif

void NES_Run_Fast() {
    // tempTime = HAL_GetTick();
    Emulate_CPU();
    // tempTime2 = HAL_GetTick();
    // CPU_Time += tempTime2 - tempTime;
    // tempTime2 = HAL_GetTick();
    // Cycles = Cycles + AddOAMCycles + AddDMCCycles;
    Cycles = Cycles + AddOAMCycles + CyclesLeft;
    AddOAMCycles = 0;
    // AddDMCCycles = 0;
    // Emulate_APU_Fast();
    Emulate_PPU_Fast();
    // tempTime = HAL_GetTick();
    // PPU_Time += tempTime - tempTime2;
}

#ifndef PC_DEBUG
void NES_Init(void) {
	NOKIA_StartDataPrepare();
	NOKIA_Clear();

	HUB75_Clear();

	DrawEmblem();

	NOKIA_StopDataPrepare();
	NOKIA_SendData();

	NES_Reset();
}

void NES_Logic(void) {
//	while (true) {
//		tempTime = HAL_GetTick();
//		Logic_Time += tempTime - tempTime2;
		while (DrawNewFrame == false) {
//			NES_Run();
			NES_Run_Fast();
		}
		// while (DrawNewFrame == false) {
  //           NES_Run_Fast();
  //       }
//		tempTime = HAL_GetTick();
//		if (DrawNewFrame == true) {
			DrawNewFrame = false;
//			tempTime = HAL_GetTick();
			if (HUB75_StartDrawing()) {
//				NES_PrepareDisplay();
				NES_PrepareDisplayFast();
//				DrawEmblem();
//				HUB75_StopDrawing();
			}
//			tempTime2 = HAL_GetTick();
//			Draw_Time = tempTime2 - tempTime;
			tempTime = HAL_GetTick();
			Frame_Time = tempTime - tempTime3;
			tempTime3 = HAL_GetTick();
//			CPU_Time = 0;
//			PPU_Time = 0;
//			Draw_Time = 0;
//			Logic_Time = 0;
//		}
//		tempTime2 = HAL_GetTick();
}
#else
int main() {
    NES_Reset();
    // printf("$%x\t%x\t%s\t%x\tA: %x\tX: %x\tY: %x\tSP: %x", ProgramCounter, opcode, opnames[opcode], addressBus, A, X, Y, StackPointer);
    // printf("\n");
    // printf("%x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x", RAM[0], RAM[1], RAM[2], RAM[3], RAM[4], RAM[5], RAM[6], RAM[7], RAM[8], RAM[9], RAM[10], RAM[11], RAM[12], RAM[13], RAM[14], RAM[15]);
    // printf("\n");
    // printf("%x\n", CHRData[0]);
    while(1) {
        // NES_Run();
        NES_Run_Fast();
        // if (PPUSTATUS.VBlank == true && ppuScanline == 241 && ppuDot < 50) {
        // if (opcode == 0x02) {
        // printf("$%x\t%x\t%s\t%x\tA: %x\tX: %x\tY: %x\tFlags: %x\tSP: %x\tPPUSTATUS: %x\tframe: %d", ProgramCounter, opcode, opnames[opcode], addressBus, NES_A, NES_X, NES_Y, Flags.Raw, StackPointer, PPUSTATUS.Raw, frameCounter);
        // fflush(stdin);
        // getc(stdin);
        if (DrawNewFrame == true) {
            DrawNewFrame = false;
            // SaveNESBufferBMPNumbered("output/nesbuffer", frameCounter);
            // SaveMergedBufferBMPNumbered("output/nesbuffer", frameCounter);
            frameCounter++;
                if (frameCounter == 150) Controller1Buttons = 0b00001000; // START
                if (frameCounter == 160) Controller1Buttons = 0b00000000;

                // if (frameCounter == 50) Controller1Buttons = 0b01000000; // LEFT
                // if (frameCounter == 60) Controller1Buttons = 0b00000000;
                // if (frameCounter == 70) Controller1Buttons = 0b01000000;
                // if (frameCounter == 80) Controller1Buttons = 0b00000000;
                // if (frameCounter == 90) Controller1Buttons = 0b01000000;
                // if (frameCounter == 100) Controller1Buttons = 0b00000000;
                // if (frameCounter == 110) Controller1Buttons = 0b01000000;
                // if (frameCounter == 120) Controller1Buttons = 0b00000000;
                // if (frameCounter == 130) Controller1Buttons = 0b01000000;
                // if (frameCounter == 140) Controller1Buttons = 0b00000000;
                // if (frameCounter == 150) Controller1Buttons = 0b01000000;
                // if (frameCounter == 160) Controller1Buttons = 0b00000000;
                // if (frameCounter == 170) Controller1Buttons = 0b01000000;
                // if (frameCounter == 180) Controller1Buttons = 0b00000000;
                // if (frameCounter == 190) Controller1Buttons = 0b01000000;
                // if (frameCounter == 200) Controller1Buttons = 0b00000000;
                // if (frameCounter == 210) Controller1Buttons = 0b01000000;
                // if (frameCounter == 220) Controller1Buttons = 0b00000000;
                // if (frameCounter == 250) Controller1Buttons = 0b00100000;
                // if (frameCounter == 260) Controller1Buttons = 0b00000000;
                // if (frameCounter == 270) Controller1Buttons = 0b00100000;
                // if (frameCounter == 280) Controller1Buttons = 0b00000000;
                // if (frameCounter == 290) Controller1Buttons = 0b00100000;
                // if (frameCounter == 300) Controller1Buttons = 0b00000000;

                // if (frameCounter == 130) Controller1Buttons = 0b00100000;
                // if (frameCounter == 140) Controller1Buttons = 0b00000000;
                // if (frameCounter == 150) Controller1Buttons = 0b00100000;
                // if (frameCounter == 160) Controller1Buttons = 0b00000000;
                // if (frameCounter == 170) Controller1Buttons = 0b00100000;
                // if (frameCounter == 180) Controller1Buttons = 0b00000000;
                // if (frameCounter == 190) Controller1Buttons = 0b00100000;
                // if (frameCounter == 200) Controller1Buttons = 0b00000000;
                // if (frameCounter == 210) Controller1Buttons = 0b00100000;
                // if (frameCounter == 220) Controller1Buttons = 0b00000000;
                // if (frameCounter == 230) Controller1Buttons = 0b00100000;
                // if (frameCounter == 240) Controller1Buttons = 0b00000000;
                // if (frameCounter == 250) Controller1Buttons = 0b00100000;
                // if (frameCounter == 260) Controller1Buttons = 0b00000000;
                // if (frameCounter == 270) Controller1Buttons = 0b00100000;
                // if (frameCounter == 280) Controller1Buttons = 0b00000000;
                // if (frameCounter == 290) Controller1Buttons = 0b00100000;
                // if (frameCounter == 300) Controller1Buttons = 0b00000000;

                // if (frameCounter == 50) Controller1Buttons = 0b01000000; // LEFT
                // if (frameCounter == 60) Controller1Buttons = 0b00000000;
                // if (frameCounter == 70) Controller1Buttons = 0b01000000; // LEFT
                // if (frameCounter == 80) Controller1Buttons = 0b00000000;
                // if (frameCounter == 90) Controller1Buttons = 0b00100000; // DOWN
                // if (frameCounter == 100) Controller1Buttons = 0b00000000;
                // if (frameCounter == 110) Controller1Buttons = 0b00100000; // DOWN
                // if (frameCounter == 120) Controller1Buttons = 0b00000000;
                // if (frameCounter == 130) Controller1Buttons = 0b00100000; // DOWN
                // if (frameCounter == 140) Controller1Buttons = 0b00000000;
                // if (frameCounter == 150) Controller1Buttons = 0b00000010; // B (SKIP)
                // if (frameCounter == 160) Controller1Buttons = 0b00000000;
                // if (frameCounter == 170) Controller1Buttons = 0b00010000; // UP
                // if (frameCounter == 180) Controller1Buttons = 0b00000000;
                // if (frameCounter == 190) Controller1Buttons = 0b00010000; // UP
                // if (frameCounter == 200) Controller1Buttons = 0b00000000;
                // if (frameCounter == 210) Controller1Buttons = 0b00010000; // UP
                // if (frameCounter == 220) Controller1Buttons = 0b00000000;
                // if (frameCounter == 230) Controller1Buttons = 0b00001000; // START
                // if (frameCounter == 240) Controller1Buttons = 0b00000000;

                // if (frameCounter == 310) Controller1Buttons = 0b00000001; // A (RUN)
                // if (frameCounter == 320) Controller1Buttons = 0b00000000;
                // if (frameCounter == 310) Controller1Buttons = 0b00000010; // B (SKIP)
                // if (frameCounter == 320) Controller1Buttons = 0b00000000;
            if (true) {
            // if (frameCounter % 10 == 0) {
            // if (frameCounter % 100 == 0) {
            // if (frameCounter % 1000 == 0) {
            // if (frameCounter % 6000 == 0) {
            // if (frameCounter % 10000 == 0) {
            // if (frameCounter % 100000 == 0) {
            // if (frameCounter >= 4301) {
            // if (frameCounter >= 5000) {
            // if (true) {
                // NES_DebugVRAM();
                // SaveNESBufferBMP("nesbuffer.bmp");
                // NES_PrepareDisplay();
                NES_PrepareDisplayFast();
                SaveDisplayBufferBMP("displaybuffer.bmp");
                SaveEventBufferBMP("eventbuffer.bmp");
                SaveMergedBufferBMP("eventbuffermerged.bmp");
                NES_DebugVRAM();
                SaveNESBufferBMP("vrambuffer.bmp");
                // memset(EventBuffer, 0x00, sizeof(EventBuffer));
//                printf("BytesRemaining: %x\tAPUSTATUS: %x\tSampleLength: %x\tCycleCount: %x\tDMCFLAGS: %x\n", APUDMC_BytesRemaining, APUSTATUS, DMCSampleLength, DMCCycleCount, DMCFLAGS);
//                printf("$%x\t%x\t%s\t%x\tA: %x\tX: %x\tY: %x\tFlags: %x\tSP: %x\tPPUSTATUS: %x\tframe: %d", ProgramCounter, opcode, opnames[opcode], addressBus, NES_A, NES_X, NES_Y, Flags.Raw, StackPointer, PPUSTATUS.Raw, frameCounter);
//                fflush(stdin);
//                getc(stdin);
            }
            memset(EventBuffer, 0x00, sizeof(EventBuffer));
            // if (frameCounter >= 2897) {
            //     SaveNESBufferBMP("nesbuffer.bmp");
            //     NES_PrepareDisplay();
            //     SaveDisplayBufferBMP("displaybuffer.bmp");
            //     NES_DebugVRAM();
            //     SaveNESBufferBMP("vrambuffer.bmp");
            // }
        }
        // printf("$%x\t%x\t%s\t%x\tA: %x\tX: %x\tY: %x\tFlags: %x\tSP: %x\tPPUSTATUS: %x\tframe: %d\n", ProgramCounter, opcode, opnames[opcode], addressBus, NES_A, NES_X, NES_Y, Flags.Raw, StackPointer, PPUSTATUS.Raw, frameCounter);
        // if (frameCounter == 2897) {
        //     printf("$%x\t%x\t%s\t%x\tA: %x\tX: %x\tY: %x\tFlags: %x\tSP: %x\tPPUSTATUS: %x\tframe: %d\n", ProgramCounter, opcode, opnames[opcode], addressBus, NES_A, NES_X, NES_Y, Flags.Raw, StackPointer, PPUSTATUS.Raw, frameCounter);
        //     // fflush(stdin);
        //     // getc(stdin);
        // }
        // if (frameCounter > 2897) {
        //     printf("$%x\t%x\t%s\t%x\tA: %x\tX: %x\tY: %x\tFlags: %x\tSP: %x\tPPUSTATUS: %x\tframe: %d", ProgramCounter, opcode, opnames[opcode], addressBus, NES_A, NES_X, NES_Y, Flags.Raw, StackPointer, PPUSTATUS.Raw, frameCounter);
        //     fflush(stdin);
        //     getc(stdin);
        // }
    }
    return 0;
}
#endif
