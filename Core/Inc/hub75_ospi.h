/**
 ******************************************************************************
 * @file    hub75_ospi.h
 * @brief   HUB75 LED matrix driver using OctoSPI on STM32H563ZI
 *
 * Architecture overview
 * ─────────────────────
 * OctoSPI drives 8 signals per clock edge simultaneously:
 *
 *   OSPI IO0  ──►  R1   (top-half red)
 *   OSPI IO1  ──►  G1   (top-half green)
 *   OSPI IO2  ──►  B1   (top-half blue)
 *   OSPI IO3  ──►  R2   (bottom-half red)
 *   OSPI IO4  ──►  G2   (bottom-half green)
 *   OSPI IO5  ──►  B2   (bottom-half blue)
 *   OSPI IO6  ──►  A    (row address bit 0)  ← baked into every framebuf byte
 *   OSPI IO7  ──►  B    (row address bit 1)  ← baked into every framebuf byte
 *   OSPI CLK  ──►  CLK
 *
 * External GPIO (outputs):
 *   C_PIN  ──►  C   (row address bit 2)  ┐ change only 4× per frame
 *   D_PIN  ──►  D   (row address bit 3)  ┘
 *   LAT_PIN ──► LAT
 *   OE_PIN  ──► OE  (active-low)
 *
 * Frame refresh strategy (for a 64×32 panel, 16 row-pairs):
 *
 *   for cd in {00, 01, 10, 11}:            ← 4 OSPI transfer groups
 *       GPIO: set C, D = cd
 *       for ab in {00, 01, 10, 11}:        ← 4 OSPI Transmit() calls
 *           OSPI: clock out PANEL_WIDTH bytes  (A,B embedded in bits[7:6])
 *           GPIO: pulse LAT, manage OE
 *
 * Each byte in the transfer:
 *   bit 7 (IO7) = B  ─ row address bit 1 (constant within one row)
 *   bit 6 (IO6) = A  ─ row address bit 0 (constant within one row)
 *   bit 5 (IO5) = B2
 *   bit 4 (IO4) = G2
 *   bit 3 (IO3) = R2
 *   bit 2 (IO2) = B1
 *   bit 1 (IO1) = G1
 *   bit 0 (IO0) = R1
 ******************************************************************************
 */

#ifndef HUB75_OSPI_H
#define HUB75_OSPI_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"   /* pulls in stm32h5xx_hal.h and your pin defines */
#include <stdint.h>
#include <stdbool.h>

/* ── Panel geometry ────────────────────────────────────────────────────────
 * Change these to match your physical panel.                               */
#define HUB75_PANEL_WIDTH   128u   /* pixels per row                        */
#define HUB75_PANEL_HEIGHT  32u   /* total rows  (must be 2 × ROW_PAIRS)   */
#define HUB75_ROW_PAIRS     (HUB75_PANEL_HEIGHT / 2u)   /* = 16            */

/* ── GPIO: row address bits A, B, C and D ───────────────────────────────────────
 * These are the two address bits NOT carried by OctoSPI.
 * Adjust PORT / PIN to your actual wiring.                                 */
#define HUB75_A_PORT        DISPLAY_A_GPIO_Port
#define HUB75_A_PIN         DISPLAY_A_Pin

#define HUB75_B_PORT        DISPLAY_B_GPIO_Port
#define HUB75_B_PIN         DISPLAY_B_Pin

#define HUB75_C_PORT        DISPLAY_C_GPIO_Port
#define HUB75_C_PIN         DISPLAY_C_Pin

#define HUB75_D_PORT        DISPLAY_D_GPIO_Port
#define HUB75_D_PIN         DISPLAY_D_Pin

/* ── GPIO: control signals ────────────────────────────────────────────────*/
#define HUB75_LAT_PORT      DISPLAY_LATCH_GPIO_Port
#define HUB75_LAT_PIN       DISPLAY_LATCH_Pin

#define HUB75_OE_PORT       DISPLAY_OE_GPIO_Port
#define HUB75_OE_PIN        DISPLAY_OE_Pin   /* active-low: SET = disabled     */

/* ── Public API ───────────────────────────────────────────────────────────*/

/**
 * @brief  Initialise the driver (OctoSPI + GPIO must already be clocked and
 *         configured by CubeMX / MX_OCTOSPI1_Init / MX_GPIO_Init).
 *         Call once before HUB75_Refresh().
 * @param  hospi  Pointer to the HAL OctoSPI handle (usually &hospi1).
 */
void HUB75_Init(XSPI_HandleTypeDef *hospi);

bool HUB75_StartDrawing(void);

void HUB75_StopDrawing(void);

void HUB75_SwapDisplayFrame(void);

/**
 * @brief  Write a 1-bit-per-channel pixel to the software framebuffer.
 *         Call this to build the image; changes are only displayed after
 *         the next HUB75_Refresh().
 *
 * @param  row   Absolute row  (0 … HUB75_PANEL_HEIGHT-1)
 * @param  col   Column        (0 … HUB75_PANEL_WIDTH-1)
 * @param  r     Red   (0 or 1)
 * @param  g     Green (0 or 1)
 * @param  b     Blue  (0 or 1)
 */
void HUB75_SetPixel(uint16_t row, uint16_t col,
                    uint8_t r, uint8_t g, uint8_t b);

/**
 * @brief  Fill every pixel with the same colour.
 */
void HUB75_FillColor(uint8_t r, uint8_t g, uint8_t b);

/**
 * @brief  Clear (turn off) all pixels.
 */
void HUB75_Clear(void);

/**
 * @brief  Push the framebuffer to the panel.
 *         Performs 4 × 4 = 16 OctoSPI row transfers (one per row-pair)
 *         grouped into 4 blocks by the C,D GPIO value.
 *         Call this repeatedly (e.g. from a 1 kHz timer callback).
 * @retval HAL_OK on success, HAL_ERROR if any OSPI transfer fails.
 */
HAL_StatusTypeDef HUB75_Refresh(void);

#ifdef __cplusplus
}
#endif

#endif /* HUB75_OSPI_H */
