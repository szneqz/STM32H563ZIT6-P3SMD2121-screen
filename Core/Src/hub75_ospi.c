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
 *   OCTOSPI1_IO0  PB1  (AF10)   → R1
 *   OCTOSPI1_IO1  PB2  (AF10)   → G1
 *   OCTOSPI1_IO2  PA7  (AF10)   → B1
 *   OCTOSPI1_IO3  PA6  (AF10)   → R2
 *   OCTOSPI1_IO4  PB0  (AF10)   → G2
 *   OCTOSPI1_IO5  PB9  (AF10)   → B2
 *   OCTOSPI1_IO6  PD7  (AF10)   → A  (row address bit 0)
 *   OCTOSPI1_IO7  PD6  (AF10)   → B  (row address bit 1)
 *   OCTOSPI1_CLK  PB2  (AF10)   → CLK
 *
 * External GPIO (Output Push-Pull, no pull, high speed):
 *   PF0 → C   (HUB75_C_PIN)
 *   PF1 → D   (HUB75_D_PIN)
 *   PF2 → LAT (HUB75_LAT_PIN)
 *   PF3 → OE  (HUB75_OE_PIN, active-low)
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
 * Byte layout (constant A,B baked in; colour bits set by HUB75_SetPixel):
 *   [7] B  — row address bit 1
 *   [6] A  — row address bit 0
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
static uint8_t s_framebuf[HUB75_ROW_PAIRS][HUB75_PANEL_WIDTH];

/* ── Private helpers ────────────────────────────────────────────────────── */

/**
 * @brief  Drive GPIO address lines C and D.
 * @param  cd  2-bit value: bit0 → C, bit1 → D
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
     * ≈ 4 ns — four NOPs ≈ 16 ns.  Add more if your panel requires it.      */
    for (int i = 0; i < 8; i++)
    {
    	__NOP();
    }
    //HAL_Delay(1);
    HAL_GPIO_WritePin(HUB75_LAT_PORT, HUB75_LAT_PIN, GPIO_PIN_RESET);   /* LAT lo  */

    HAL_GPIO_WritePin(HUB75_OE_PORT,  HUB75_OE_PIN,  GPIO_PIN_RESET); /* OE on   */
}

/**
 * @brief  Send one row's pixels over OctoSPI (indirect-write, 8 simultaneous lines).
 *
 *         The OSPI peripheral clocks one byte per CLK edge.  All 8 IO lines are
 *         driven in parallel so every byte maps directly to the 8 HUB75 signals:
 *         {B, A, B2, G2, R2, B1, G1, R1}.
 *
 * @param  data  Pointer to HUB75_PANEL_WIDTH bytes.
 * @retval HAL_OK / HAL_ERROR
 */
static HAL_StatusTypeDef prv_OSPISendRow(const uint8_t *data)
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

    return HAL_XSPI_Transmit(s_hospi, (uint8_t *)(uintptr_t)data, HAL_MAX_DELAY);
}

/**
 * @brief  Rebuild the A,B address nibble stored in bits [7:6] of a framebuffer
 *         row's bytes after the row_pair index is known.
 *         Called whenever colour data is written, keeping the byte self-contained.
 *
 * @param  row_pair  0 … HUB75_ROW_PAIRS-1
 */
static void prv_BakeAB(uint8_t row_pair)
{
    uint8_t ab   = row_pair & 0x03u;        /* low 2 bits  → A,B (OSPI)      */
    uint8_t mask = (uint8_t)(ab << 6u);     /* bits [7:6]                     */

    for (uint16_t col = 0; col < HUB75_PANEL_WIDTH; col++)
    {
        s_framebuf[row_pair][col] =
            (s_framebuf[row_pair][col] & 0x3Fu) | mask;
    }
}

/* ── Public API ─────────────────────────────────────────────────────────── */

void HUB75_Init(XSPI_HandleTypeDef *hospi)
{
    s_hospi = hospi;

    /* Ensure display blanked and latch de-asserted at startup               */
    prv_SetABCD(0u);

    HUB75_Clear();
}

void HUB75_SetPixel(uint16_t row, uint16_t col,
                    uint8_t r, uint8_t g, uint8_t b)
{
    if (row >= HUB75_PANEL_HEIGHT || col >= HUB75_PANEL_WIDTH) return;

    /*
     * Map absolute row → row_pair and top/bottom half.
     *
     * Rows 0 … ROW_PAIRS-1       → top half    (R1,G1,B1)
     * Rows ROW_PAIRS … HEIGHT-1  → bottom half  (R2,G2,B2)
     */
    uint8_t row_pair  = (uint8_t)(row % HUB75_ROW_PAIRS);
    uint8_t is_bottom = (row >= HUB75_ROW_PAIRS) ? 1u : 0u;

    uint8_t byte = s_framebuf[row_pair][col];

    if (is_bottom)
    {
        /* Update bits [5:3] (B2,G2,R2), preserve the rest                  */
        byte &= ~0x38u;
        byte |= (uint8_t)(((b & 1u) << 5u) | ((g & 1u) << 4u) | ((r & 1u) << 3u));
    }
    else
    {
        /* Update bits [2:0] (B1,G1,R1), preserve the rest                  */
        byte &= ~0x07u;
        byte |= (uint8_t)(((b & 1u) << 2u) | ((g & 1u) << 1u) | (r & 1u));
    }

    /* Re-bake A,B address into bits [7:6]                                   */
    //uint8_t LATCH_OE = 0b10;
    //if (col > (62))
    //{
    //	LATCH_OE = 0b01;
    //}
    byte = (byte & 0x3Fu); //| (uint8_t)(LATCH_OE << 6u);

    s_framebuf[row_pair][col] = byte;
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
     * Zero all colour bits but preserve the baked-in A,B address bits.
     * We rebuild every row-pair from scratch so A,B are always correct.
     */
    memset(s_framebuf, 0, sizeof(s_framebuf));
    for (uint8_t rp = 0; rp < HUB75_ROW_PAIRS; rp++)
        prv_BakeAB(rp);
}

/**
 * HUB75_Refresh — full-frame scan
 * ─────────────────────────────────────────────────────────────────────────
 *
 * Row-pair addressing:
 *
 *   row_pair = (cd << 2) | ab       full 4-bit address (D C B A)
 *
 * Transfer map (4 outer groups × 4 inner rows = 16 OSPI transfers total):
 *
 *   cd=0 (C=0,D=0):  ab=0 → row_pair 0   ab=1 → row_pair 1  …  ab=3 → row_pair 3
 *   cd=1 (C=1,D=0):  ab=0 → row_pair 4   …                       ab=3 → row_pair 7
 *   cd=2 (C=0,D=1):  ab=0 → row_pair 8   …                       ab=3 → row_pair 11
 *   cd=3 (C=1,D=1):  ab=0 → row_pair 12  …                       ab=3 → row_pair 15
 *
 * GPIO C,D change once at the start of each outer group (4 GPIO transitions
 * per frame refresh — this is the "GPIO change between transfers" requirement).
 */
HAL_StatusTypeDef HUB75_Refresh(void)
{
	HAL_StatusTypeDef status = HAL_OK;

	/*
	 * ── Outer loop: 4 C,D values  (address bits [3:2] via GPIO) ──────────
	 * Each iteration represents one "OSPI transfer group".
	 */
	for (uint8_t abcd = 0; abcd < 16u; abcd++)
	{
			/* ── OSPI: clock out all pixels for this row-pair ─────────── */
			status = prv_OSPISendRow(s_framebuf[abcd]);
			if (status != HAL_OK)
			{
				HAL_GPIO_WritePin(LED1_GPIO_PORT, LED1_PIN, GPIO_PIN_RESET);
				HAL_GPIO_WritePin(LED3_GPIO_PORT, LED3_PIN, GPIO_PIN_SET);
				return status;
			}
			HAL_GPIO_WritePin(LED1_GPIO_PORT, LED1_PIN, GPIO_PIN_SET);
			HAL_GPIO_WritePin(LED3_GPIO_PORT, LED3_PIN, GPIO_PIN_RESET);

			prv_LatchRow();

			/* ── GPIO: set the two address lines that OctoSPI cannot drive ──── */
			prv_SetABCD(abcd);

			/*
			 * Optional: insert a small OE-enable window here for BCM/PWM
			 * brightness control.  For binary (1-bit) colour the LatchRow()
			 * already re-enables OE unconditionally.
			 */
		/* C,D will be updated at the top of the next cd iteration          */
			//HAL_Delay(200);
	}
	//prv_SetABCD(0b0000);
	//HAL_GPIO_WritePin(HUB75_OE_PORT,  HUB75_OE_PIN,  GPIO_PIN_SET);   /* OE off  */
	return HAL_OK;
}
