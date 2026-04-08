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
static uint8_t s_framebuf[2][HUB75_PANEL_HEIGHT][HUB75_PANEL_WIDTH];
static uint8_t framebuf_row[HUB75_PANEL_WIDTH];

static uint8_t current_draw_frame = 0;
static uint8_t current_display_frame = 1;
static bool isDrawing = false;
static bool alreadyDisplayed = false;

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
static inline void prv_LatchRow(void)
{
    /* Blank the display while we latch to avoid row-ghosting                */
    HAL_GPIO_WritePin(HUB75_OE_PORT,  HUB75_OE_PIN,  GPIO_PIN_SET);   /* OE off  */

    HAL_GPIO_WritePin(HUB75_LAT_PORT, HUB75_LAT_PIN, GPIO_PIN_SET);   /* LAT hi  */
    /* Minimum LAT pulse width is typically ≥20 ns; at 250 MHz one __NOP()   *
     * ≈ 4 ns — eight NOPs ≈ 32 ns.  Add more if your panel requires it.      */
    for (int i = 0; i < 8; i++)
    {
    	__NOP();
    }

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
    prv_LatchRow();
    prv_SetABCD(abcd);

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

// Compact bits at positions {4,2,0} down to {2,1,0}
// Input s guaranteed to have only bits 0, 2, 4 set (from & 0x15 mask)
#define COMPACT3(s)  ((uint8_t)((s & 1u) | ((s >> 1u) & 2u) | ((s >> 2u) & 4u)))

void HUB75_PrepareRowToDraw(uint8_t abcd)
{
    static uint8_t current_frame_nr = 0;

    // Cache row pointers once — avoids re-computing multi-dim array offsets
    // 'restrict' tells the compiler row0, row1, out don't alias → better codegen
    const uint8_t * const restrict row0 = s_framebuf[current_display_frame][abcd];
    const uint8_t * const restrict row1 = s_framebuf[current_display_frame][abcd + HUB75_ROW_PAIRS];
    uint8_t       * const restrict out  = framebuf_row;

    const uint8_t fn = current_frame_nr;   // hoist invariant to register

    // Pixel byte format: bits [1:0]=R, [3:2]=G, [5:4]=B (2-bit channels)
    // Branch on fn is hoisted OUT of the loop — one branch, three tight loops
    if (fn < 1u) {
        // channel > 0: nonzero ↔ OR of both bits in each 2-bit field
        for (uint32_t i = 0; i < HUB75_PANEL_WIDTH; i++) {
            uint8_t p0 = row0[i];
            uint8_t p1 = row1[i];
            uint8_t s0 = (uint8_t)((p0 | (p0 >> 1u)) & 0x15u);
            uint8_t s1 = (uint8_t)((p1 | (p1 >> 1u)) & 0x15u);
            out[i] = (uint8_t)(COMPACT3(s0) | (COMPACT3(s1) << 3u));
        }
    } else if (fn < 3u) {
        // channel > 1: MSB of field set ↔ just the upper bit of each 2-bit field
        for (uint32_t i = 0; i < HUB75_PANEL_WIDTH; i++) {
            uint8_t p0 = row0[i];
            uint8_t p1 = row1[i];
            uint8_t s0 = (uint8_t)((p0 >> 1u) & 0x15u);
            uint8_t s1 = (uint8_t)((p1 >> 1u) & 0x15u);
            out[i] = (uint8_t)(COMPACT3(s0) | (COMPACT3(s1) << 3u));
        }
    } else {
        // channel > 2: must equal 3 ↔ AND of both bits in each 2-bit field
        for (uint32_t i = 0; i < HUB75_PANEL_WIDTH; i++) {
            uint8_t p0 = row0[i];
            uint8_t p1 = row1[i];
            uint8_t s0 = (uint8_t)((p0 & (p0 >> 1u)) & 0x15u);
            uint8_t s1 = (uint8_t)((p1 & (p1 >> 1u)) & 0x15u);
            out[i] = (uint8_t)(COMPACT3(s0) | (COMPACT3(s1) << 3u));
        }
    }

    #undef COMPACT3

    // Branchless modulo-3 increment
    current_frame_nr = (fn >= 4u) ? 0u : fn + 1u;
}

bool HUB75_StartDrawing(void) {
	if (current_display_frame == (current_draw_frame ^ 1)) {
		isDrawing = false;
		return false;
	}
	current_draw_frame ^= 1;
	isDrawing = true;
	return true;
}

void HUB75_StopDrawing(void) {
	isDrawing = false;
	alreadyDisplayed = false;
}

void HUB75_SwapDisplayFrame(void) {
	if (!alreadyDisplayed && (!isDrawing || current_draw_frame != (current_display_frame ^ 1))) {
		current_display_frame ^= 1;
		alreadyDisplayed = true;
	}
}

void HUB75_CopyFrame(uint8_t *frame, uint16_t size) {
	memcpy(s_framebuf[current_draw_frame], frame, size);
}

void HUB75_SetPixel(uint16_t row, uint16_t col,
                    uint8_t r, uint8_t g, uint8_t b)
{
	if (!isDrawing) return;
    if (row >= HUB75_PANEL_HEIGHT || col >= HUB75_PANEL_WIDTH) return;

    s_framebuf[current_draw_frame][row][col] =
    		(uint8_t)(((b & 3u) << 4u) | ((g & 3u) << 2u) | (r & 3u));
}

void HUB75_FillColor(uint8_t r, uint8_t g, uint8_t b)
{
    for (uint16_t row = 0; row < HUB75_PANEL_HEIGHT; row++)
        for (uint16_t col = 0; col < HUB75_PANEL_WIDTH; col++)
            HUB75_SetPixel(row, col, r, g, b);
}

void HUB75_Clear(void)
{
    /*
     * Zero all color bits
     */
    memset(s_framebuf, 0, sizeof(s_framebuf));
    memset(framebuf_row, 0, sizeof(framebuf_row));
}
