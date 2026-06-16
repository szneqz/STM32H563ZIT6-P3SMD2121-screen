/**
 ******************************************************************************
 * @file    hub75_ospi.c
 * @brief   HUB75 LED matrix driver — OctoSPI implementation (STM32H563ZI)
 *
 * CubeMX / ioc configuration checklist
 * ──────────────────────────────────────
 * OCTOSPI1:
 *   Functional Mode        : Indirect-write mode
 *   Data Interface         : Octo Lines (8 data lines)
 *   Clock Prescaler        : e.g. 4  → ~62.5 MHz with 250 MHz AHB
 *   Sample Shifting        : None
 *   FIFO Threshold         : 1
 *   Chip-Select High Time  : 1 (we never actually talk to a flash)
 *   Clock Mode             : Mode 0 (CPOL=0 CPHA=0) — adjust if panel differs
 *   NCS pin                : not connected (or tied high externally)
 *
 * Typical OCTOSPI1 IO pin assignments on STM32H563 (verify with your board):
 *   OCTOSPI1_IO0  PF8  (AF10)   → R1
 *   OCTOSPI1_IO1  PF9  (AF10)   → G1
 *   OCTOSPI1_IO2  PF7  (AF10)   → B1
 *   OCTOSPI1_IO3  PA6  (AF10)   → R2
 *   OCTOSPI1_IO4  PD4  (AF10)   → G2
 *   OCTOSPI1_IO5  PC2  (AF10)   → B2
 *   OCTOSPI1_IO6  PC3  (AF10)   → NC
 *   OCTOSPI1_IO7  PC0  (AF10)   → NC
 *   OCTOSPI1_CLK  PB2  (AF10)   → CLK
 *
 * External GPIO (Output Push-Pull, no pull, high speed):
 *   PF2 - A   (HUB75_A_PIN)
 *   PD0 - B   (HUB75_B_PIN)
 *   PF0 → C   (HUB75_C_PIN)
 *   PF1 → D   (HUB75_D_PIN)
 *   PD1 → LAT (HUB75_LAT_PIN)
 *   PG0 → OE  (HUB75_OE_PIN, active-low)
 ******************************************************************************
 */

#include "hub75_ospi.h"
#include <string.h>

/* ── Private state ──────────────────────────────────────────────────────── */

/** Saved handle pointer passed to HUB75_Init(). */
static XSPI_HandleTypeDef *s_hospi = NULL;

/**
 * Software framebuffer — one byte per pixel per row-pair.
 *
 * Byte layout (colour bits set by HUB75_SetPixel):
 *   [7] not assigned
 *   [6] not assigned
 *   [5] B2 — bottom-half blue
 *   [4] G2 — bottom-half green
 *   [3] R2 — bottom-half red
 *   [2] B1 — top-half blue
 *   [1] G1 — top-half green
 *   [0] R1 — top-half red
 *
 * Addressing: s_framebuf[row_pair][col]
 *   row_pair = 0 … HUB75_ROW_PAIRS-1
 *   col      = 0 … HUB75_PANEL_WIDTH-1
 */
static ColorBitfield s_framebuf[2][HUB75_PANEL_HEIGHT][HUB75_PANEL_WIDTH];
static uint8_t framebuf_row[HUB75_PANEL_WIDTH];

static uint8_t current_draw_frame = 1;
static uint8_t current_display_frame = 1;
static bool isDrawing = false;
static bool alreadyDisplayed = false;

static const ColorBitfield black = { .bits.r = 0, .bits.g = 0, .bits.b = 0 };
static const unsigned char ASCII[][5] = {
  // First 32 characters (0x00-0x19) are ignored. These are
  // non-displayable, control characters.
   {0x00, 0x00, 0x00, 0x00, 0x00} // 0x20
  ,{0x00, 0x00, 0x5f, 0x00, 0x00} // 0x21 !
  ,{0x00, 0x07, 0x00, 0x07, 0x00} // 0x22 "
  ,{0x14, 0x7f, 0x14, 0x7f, 0x14} // 0x23 #
  ,{0x24, 0x2a, 0x7f, 0x2a, 0x12} // 0x24 $
  ,{0x23, 0x13, 0x08, 0x64, 0x62} // 0x25 %
  ,{0x36, 0x49, 0x55, 0x22, 0x50} // 0x26 &
  ,{0x00, 0x05, 0x03, 0x00, 0x00} // 0x27 '
  ,{0x00, 0x1c, 0x22, 0x41, 0x00} // 0x28 (
  ,{0x00, 0x41, 0x22, 0x1c, 0x00} // 0x29 )
  ,{0x14, 0x08, 0x3e, 0x08, 0x14} // 0x2a *
  ,{0x08, 0x08, 0x3e, 0x08, 0x08} // 0x2b +
  ,{0x00, 0x50, 0x30, 0x00, 0x00} // 0x2c ,
  ,{0x08, 0x08, 0x08, 0x08, 0x08} // 0x2d -
  ,{0x00, 0x60, 0x60, 0x00, 0x00} // 0x2e .
  ,{0x20, 0x10, 0x08, 0x04, 0x02} // 0x2f /
  ,{0x3e, 0x51, 0x49, 0x45, 0x3e} // 0x30 0
  ,{0x00, 0x42, 0x7f, 0x40, 0x00} // 0x31 1
  ,{0x42, 0x61, 0x51, 0x49, 0x46} // 0x32 2
  ,{0x21, 0x41, 0x45, 0x4b, 0x31} // 0x33 3
  ,{0x18, 0x14, 0x12, 0x7f, 0x10} // 0x34 4
  ,{0x27, 0x45, 0x45, 0x45, 0x39} // 0x35 5
  ,{0x3c, 0x4a, 0x49, 0x49, 0x30} // 0x36 6
  ,{0x01, 0x71, 0x09, 0x05, 0x03} // 0x37 7
  ,{0x36, 0x49, 0x49, 0x49, 0x36} // 0x38 8
  ,{0x06, 0x49, 0x49, 0x29, 0x1e} // 0x39 9
  ,{0x00, 0x36, 0x36, 0x00, 0x00} // 0x3a :
  ,{0x00, 0x56, 0x36, 0x00, 0x00} // 0x3b ;
  ,{0x08, 0x14, 0x22, 0x41, 0x00} // 0x3c <
  ,{0x14, 0x14, 0x14, 0x14, 0x14} // 0x3d =
  ,{0x00, 0x41, 0x22, 0x14, 0x08} // 0x3e >
  ,{0x02, 0x01, 0x51, 0x09, 0x06} // 0x3f ?
  ,{0x32, 0x49, 0x79, 0x41, 0x3e} // 0x40 @
  ,{0x7e, 0x11, 0x11, 0x11, 0x7e} // 0x41 A
  ,{0x7f, 0x49, 0x49, 0x49, 0x36} // 0x42 B
  ,{0x3e, 0x41, 0x41, 0x41, 0x22} // 0x43 C
  ,{0x7f, 0x41, 0x41, 0x22, 0x1c} // 0x44 D
  ,{0x7f, 0x49, 0x49, 0x49, 0x41} // 0x45 E
  ,{0x7f, 0x09, 0x09, 0x09, 0x01} // 0x46 F
  ,{0x3e, 0x41, 0x49, 0x49, 0x7a} // 0x47 G
  ,{0x7f, 0x08, 0x08, 0x08, 0x7f} // 0x48 H
  ,{0x00, 0x41, 0x7f, 0x41, 0x00} // 0x49 I
  ,{0x20, 0x40, 0x41, 0x3f, 0x01} // 0x4a J
  ,{0x7f, 0x08, 0x14, 0x22, 0x41} // 0x4b K
  ,{0x7f, 0x40, 0x40, 0x40, 0x40} // 0x4c L
  ,{0x7f, 0x02, 0x0c, 0x02, 0x7f} // 0x4d M
  ,{0x7f, 0x04, 0x08, 0x10, 0x7f} // 0x4e N
  ,{0x3e, 0x41, 0x41, 0x41, 0x3e} // 0x4f O
  ,{0x7f, 0x09, 0x09, 0x09, 0x06} // 0x50 P
  ,{0x3e, 0x41, 0x51, 0x21, 0x5e} // 0x51 Q
  ,{0x7f, 0x09, 0x19, 0x29, 0x46} // 0x52 R
  ,{0x46, 0x49, 0x49, 0x49, 0x31} // 0x53 S
  ,{0x01, 0x01, 0x7f, 0x01, 0x01} // 0x54 T
  ,{0x3f, 0x40, 0x40, 0x40, 0x3f} // 0x55 U
  ,{0x1f, 0x20, 0x40, 0x20, 0x1f} // 0x56 V
  ,{0x3f, 0x40, 0x38, 0x40, 0x3f} // 0x57 W
  ,{0x63, 0x14, 0x08, 0x14, 0x63} // 0x58 X
  ,{0x07, 0x08, 0x70, 0x08, 0x07} // 0x59 Y
  ,{0x61, 0x51, 0x49, 0x45, 0x43} // 0x5a Z
  ,{0x00, 0x7f, 0x41, 0x41, 0x00} // 0x5b [
  ,{0x02, 0x04, 0x08, 0x10, 0x20} // 0x5c \ (keep this to escape the backslash)
  ,{0x00, 0x41, 0x41, 0x7f, 0x00} // 0x5d ]
  ,{0x04, 0x02, 0x01, 0x02, 0x04} // 0x5e ^
  ,{0x40, 0x40, 0x40, 0x40, 0x40} // 0x5f _
  ,{0x00, 0x01, 0x02, 0x04, 0x00} // 0x60 `
  ,{0x20, 0x54, 0x54, 0x54, 0x78} // 0x61 a
  ,{0x7f, 0x48, 0x44, 0x44, 0x38} // 0x62 b
  ,{0x38, 0x44, 0x44, 0x44, 0x20} // 0x63 c
  ,{0x38, 0x44, 0x44, 0x48, 0x7f} // 0x64 d
  ,{0x38, 0x54, 0x54, 0x54, 0x18} // 0x65 e
  ,{0x08, 0x7e, 0x09, 0x01, 0x02} // 0x66 f
  ,{0x0c, 0x52, 0x52, 0x52, 0x3e} // 0x67 g
  ,{0x7f, 0x08, 0x04, 0x04, 0x78} // 0x68 h
  ,{0x00, 0x44, 0x7d, 0x40, 0x00} // 0x69 i
  ,{0x20, 0x40, 0x44, 0x3d, 0x00} // 0x6a j
  ,{0x7f, 0x10, 0x28, 0x44, 0x00} // 0x6b k
  ,{0x00, 0x41, 0x7f, 0x40, 0x00} // 0x6c l
  ,{0x7c, 0x04, 0x18, 0x04, 0x78} // 0x6d m
  ,{0x7c, 0x08, 0x04, 0x04, 0x78} // 0x6e n
  ,{0x38, 0x44, 0x44, 0x44, 0x38} // 0x6f o
  ,{0x7c, 0x14, 0x14, 0x14, 0x08} // 0x70 p
  ,{0x08, 0x14, 0x14, 0x18, 0x7c} // 0x71 q
  ,{0x7c, 0x08, 0x04, 0x04, 0x08} // 0x72 r
  ,{0x48, 0x54, 0x54, 0x54, 0x20} // 0x73 s
  ,{0x04, 0x3f, 0x44, 0x40, 0x20} // 0x74 t
  ,{0x3c, 0x40, 0x40, 0x20, 0x7c} // 0x75 u
  ,{0x1c, 0x20, 0x40, 0x20, 0x1c} // 0x76 v
  ,{0x3c, 0x40, 0x30, 0x40, 0x3c} // 0x77 w
  ,{0x44, 0x28, 0x10, 0x28, 0x44} // 0x78 x
  ,{0x0c, 0x50, 0x50, 0x50, 0x3c} // 0x79 y
  ,{0x44, 0x64, 0x54, 0x4c, 0x44} // 0x7a z
  ,{0x00, 0x08, 0x36, 0x41, 0x00} // 0x7b {
  ,{0x00, 0x00, 0x7f, 0x00, 0x00} // 0x7c |
  ,{0x00, 0x41, 0x36, 0x08, 0x00} // 0x7d }
  ,{0x10, 0x08, 0x08, 0x10, 0x08} // 0x7e ~
  ,{0x78, 0x46, 0x41, 0x46, 0x78} // 0x7f DEL
};

/* ── Private helpers ────────────────────────────────────────────────────── */

/**
 * @brief  Drive GPIO address lines A, B, C and D.
 * @param  cd  4-bit value: bit0 → A, bit1 → B, bit2 → C, bit3 → D
 */
static inline void prv_SetABCD(uint8_t abcd)
{
    HAL_GPIO_WritePin(HUB75_A_PORT, HUB75_A_PIN,
                      (abcd & 0x01u) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(HUB75_B_PORT, HUB75_B_PIN,
                      (abcd & 0x02u) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(HUB75_C_PORT, HUB75_C_PIN,
                      (abcd & 0x04u) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(HUB75_D_PORT, HUB75_D_PIN,
                      (abcd & 0x08u) ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

/**
 * @brief  Latch a completed row: disable OE → assert LAT → release LAT → enable OE.
 *         The OE blanking prevents display glitches (ghosting) during latching.
 */
static inline void prv_LatchRowStart(void)
{
    HAL_GPIO_WritePin(HUB75_OE_PORT,  HUB75_OE_PIN,  GPIO_PIN_SET);   /* OE off  */

    HAL_GPIO_WritePin(HUB75_LAT_PORT, HUB75_LAT_PIN, GPIO_PIN_SET);   /* LAT hi  */
}

static inline void prv_LatchRowEnd(void) {
    HAL_GPIO_WritePin(HUB75_LAT_PORT, HUB75_LAT_PIN, GPIO_PIN_RESET);  /* LAT lo  */

    HAL_GPIO_WritePin(HUB75_OE_PORT,  HUB75_OE_PIN,  GPIO_PIN_RESET);  /* OE on   */
}

/**
 * @brief  Send one row's pixels over OctoSPI (indirect-write, 8 simultaneous lines).
 *
 *         The OSPI peripheral clocks one byte per CLK edge.  All 8 IO lines are
 *         driven in parallel so every byte maps directly to the 8 HUB75 signals:
 *         {0, 0, B2, G2, R2, B1, G1, R1}.
 *
 * @param  data  Pointer to HUB75_PANEL_WIDTH bytes.
 * @retval HAL_OK / HAL_ERROR
 */
static HAL_StatusTypeDef prv_OSPIStartSend(const uint8_t *data)
{
    XSPI_RegularCmdTypeDef cmd = {0};

    /*
     * HAL_XSPI_Command() rejects transfers with both Instruction and Address
     * set to NONE.  We satisfy the check with a 1-byte dummy instruction
     * (0x00) on all 8 lines — this costs one extra CLK edge at the start,
     * which is harmless because the HUB75 shift register is exactly
     * PANEL_WIDTH wide and the leading zero gets pushed off the end.
     */
    cmd.OperationType      = HAL_XSPI_OPTYPE_COMMON_CFG;

    /* ── Instruction: 1 dummy byte, 0x00, on all 8 IO lines ───────────── */
    cmd.InstructionMode    = HAL_XSPI_INSTRUCTION_8_LINES;
    cmd.InstructionWidth   = HAL_XSPI_INSTRUCTION_8_BITS;
    cmd.InstructionDTRMode = HAL_XSPI_INSTRUCTION_DTR_DISABLE;
    cmd.Instruction        = 0x00U;

    /* ── Address phase: disabled ───────────────────────────────────────── */
    cmd.AddressMode        = HAL_XSPI_ADDRESS_NONE;

    /* ── Alternate-bytes: disabled ─────────────────────────────────────── */
    cmd.AlternateBytesMode = HAL_XSPI_ALT_BYTES_NONE;

    /* ── Data: PANEL_WIDTH bytes on all 8 lines ────────────────────────── */
    cmd.DataMode           = HAL_XSPI_DATA_8_LINES;
    cmd.DataLength         = HUB75_PANEL_WIDTH;   // field renamed from NbData
    cmd.DataDTRMode        = HAL_XSPI_DATA_DTR_DISABLE;

    cmd.DummyCycles        = 0;
    cmd.DQSMode            = HAL_XSPI_DQS_DISABLE;
    cmd.SIOOMode           = HAL_XSPI_SIOO_INST_EVERY_CMD;

    if (HAL_XSPI_Command(s_hospi, &cmd, HAL_MAX_DELAY) != HAL_OK)
        return HAL_ERROR;

    return HAL_XSPI_Transmit_DMA(s_hospi, (uint8_t *)(uintptr_t)data);
}

/* ── Public API ─────────────────────────────────────────────────────────── */

void HUB75_Init(XSPI_HandleTypeDef *hospi)
{
    s_hospi = hospi;

    /* Ensure display blanked and latch de-asserted at startup               */
    prv_SetABCD(0u);

    HUB75_Clear();

    HUB75_PrepareRowToDraw(0);
    prv_OSPIStartSend((uint8_t *)(uintptr_t)framebuf_row);
}

// Called when DMA transfer completes
void HAL_XSPI_TxCpltCallback(XSPI_HandleTypeDef *hxspi) {
	static uint8_t abcd = 0;

    // Latch row and set address lines
	prv_LatchRowStart();
    prv_SetABCD(abcd);
    prv_LatchRowEnd();

    // Increment row counter
    abcd++;
    if (abcd >= HUB75_ROW_PAIRS) {
        abcd = 0;
        HUB75_SwapDisplayFrame();
    }

    HUB75_PrepareRowToDraw(abcd);

    // Try starting next DMA immediately
    if (hxspi->State == HAL_XSPI_STATE_READY) {
    	HAL_StatusTypeDef status = prv_OSPIStartSend((uint8_t *)(uintptr_t)framebuf_row);

    	if (status != HAL_OK)
    	{
    		HAL_GPIO_WritePin(LED1_GPIO_PORT, LED1_PIN, GPIO_PIN_RESET);
    		HAL_GPIO_WritePin(LED3_GPIO_PORT, LED3_PIN, GPIO_PIN_SET);
    	}
    	else
    	{
    		HAL_GPIO_WritePin(LED1_GPIO_PORT, LED1_PIN, GPIO_PIN_SET);
    		HAL_GPIO_WritePin(LED3_GPIO_PORT, LED3_PIN, GPIO_PIN_RESET);
    	}
    }
}

void HUB75_PrepareRowToDraw(uint8_t abcd)
{
    static uint8_t pwm_step = 0;

    const ColorBitfield * const restrict row0 =
        s_framebuf[current_display_frame][abcd];

    const ColorBitfield * const restrict row1 =
        s_framebuf[current_display_frame][abcd + HUB75_ROW_PAIRS];

    uint8_t * const restrict out = framebuf_row;

    for (uint32_t i = 0; i < HUB75_PANEL_WIDTH; i++) {
        // temporal PWM compare
        uint8_t R0 = (row0[i].bits.r > pwm_step);
        uint8_t G0 = (row0[i].bits.g > pwm_step);
        uint8_t B0 = (row0[i].bits.b > pwm_step);

        uint8_t R1 = (row1[i].bits.r > pwm_step);
        uint8_t G1 = (row1[i].bits.g  > pwm_step);
        uint8_t B1 = (row1[i].bits.b  > pwm_step);

        out[i] =
              (R0 << 0)
            | (G0 << 1)
            | (B0 << 2)
            | (R1 << 3)
            | (G1 << 4)
            | (B1 << 5);
    }

    pwm_step += 5;
    if (pwm_step > 31)
        pwm_step = 0;
}

bool HUB75_StartDrawing(void) {
	if (!isDrawing) {
		if (current_display_frame == (current_draw_frame ^ 1)) {
			isDrawing = false;
			return false;
		}
		current_draw_frame ^= 1;
		isDrawing = true;
		return true;
	}
	return true;
}

void HUB75_StopDrawing(void) {
	if (isDrawing) {
		isDrawing = false;
		alreadyDisplayed = false;
	}
}

void HUB75_SwapDisplayFrame(void) {
	if (!alreadyDisplayed && (!isDrawing || current_draw_frame != (current_display_frame ^ 1))) {
		current_display_frame ^= 1;
		alreadyDisplayed = true;
	}
}

void HUB75_CopyFrame(ColorBitfield *frame, uint16_t size) {
	memcpy(s_framebuf[current_draw_frame], frame, (size * 2));
}

void HUB75_CopyPreviousFrame(void) {
	memcpy(s_framebuf[current_draw_frame], s_framebuf[current_display_frame], (HUB75_PANEL_HEIGHT * HUB75_PANEL_WIDTH * 2));
}

void HUB75_SetPixelRGB(uint16_t x, uint16_t y,
                    uint8_t r, uint8_t g, uint8_t b)
{
	if (!isDrawing) return;
    if (y >= HUB75_PANEL_HEIGHT || x >= HUB75_PANEL_WIDTH) return;

    s_framebuf[current_draw_frame][y][x].bits.r = r;
    s_framebuf[current_draw_frame][y][x].bits.g = g;
    s_framebuf[current_draw_frame][y][x].bits.b = b;
}

void HUB75_SetPixelColor(uint16_t x, uint16_t y,
						ColorBitfield color)
{
	if (!isDrawing) return;
    if (y >= HUB75_PANEL_HEIGHT || x >= HUB75_PANEL_WIDTH) return;

    s_framebuf[current_draw_frame][y][x] = color;
}

void HUB75_ChangeDrawFrameColor(ColorBitfield color) {
	for (int y = 0; y < HUB75_PANEL_HEIGHT; y++) {
		for (int x = 0; x < HUB75_PANEL_WIDTH; x++) {
			if (s_framebuf[current_draw_frame][y][x].color != 0 && s_framebuf[current_draw_frame][y][x].bits.mask == false) {
				float strength = (float)(
						s_framebuf[current_draw_frame][y][x].bits.r * 2 +
						s_framebuf[current_draw_frame][y][x].bits.g * 5 +
						s_framebuf[current_draw_frame][y][x].bits.b)
						/ 248;
				// 8 color strength divided by 31 max = 248
				s_framebuf[current_draw_frame][y][x].bits.r = color.bits.r * strength;
				s_framebuf[current_draw_frame][y][x].bits.g = color.bits.g * strength;
				s_framebuf[current_draw_frame][y][x].bits.b = color.bits.b * strength;
			}
		}
	}
}

void HUB75_FillColor(uint8_t r, uint8_t g, uint8_t b)
{
    for (uint16_t row = 0; row < HUB75_PANEL_HEIGHT; row++)
        for (uint16_t col = 0; col < HUB75_PANEL_WIDTH; col++)
            HUB75_SetPixelRGB(row, col, r, g, b);
}

void HUB75_SetChar(char character, int x, int y, ColorBitfield color, bool transparent)
{
	int column; // temp byte to store character's column bitmap
	for (int i=0; i<5; i++) // 5 columns (x) per character
	{
		column = ASCII[character - 0x20][i];
		for (int j=0; j<8; j++) // 8 rows (y) per character
		{
			if (column & (0x01 << j)) {// test bits to set pixels
				if ((transparent && color.color != 0x0000) || !transparent) {
					HUB75_SetPixelColor(x+i, y+j, color);
				}
			}
			else {
				if ((transparent && color.color > 0x0000) || !transparent) {
					HUB75_SetPixelColor(x+i, y+j, black);
				}
			}
		}
	}
}

void HUB75_SetStr(char * dString, int x, int y, ColorBitfield color, bool transparent, bool wrapAround)
{
	bool stopWriting = false;

	while (*dString != 0x00) // loop until null terminator
	{
		HUB75_SetChar(*dString++, x, y, color, transparent);
		x+=5;
		if ((transparent && color.color > 0x0000) || !transparent) {
			for (int i=y; i<y+8; i++)
			{
				HUB75_SetPixelColor(x, i, black);
			}
		}
		x++;

		//when not wrapping around and writing the last character stop trying to write
		if (stopWriting) {
			break;
		}

		if (x > (HUB75_PANEL_WIDTH - 5)) // Enables wrap around
		{
			if (wrapAround) {
				x = 0;
				y += 8;
			} else {
				stopWriting = true;
			}
		}
	}
}


void HUB75_Clear(void)
{
    /*
     * Zero all color bits
     */
    memset(s_framebuf, 0, sizeof(s_framebuf));
    memset(framebuf_row, 0, sizeof(framebuf_row));
}
