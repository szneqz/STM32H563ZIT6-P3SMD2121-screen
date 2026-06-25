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
 *   OSPI IO6  ──►  NC
 *   OSPI IO7  ──►  NC
 *   OSPI CLK  ──►  CLK
 *
 * External GPIO (outputs):
 *   A_PIN  ──►  A	 (row address bit 0)
 *   B_PIN  ──►  B   (row address bit 2)
 *   C_PIN  ──►  C   (row address bit 2)
 *   D_PIN  ──►  D   (row address bit 3)
 *   LAT_PIN ──► LAT
 *   OE_PIN  ──► OE  (active-low)
 *
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
#define HUB75_PANEL_WIDTH   128u   /* pixels per row  (64 × 2 because there are two panels */
#define HUB75_PANEL_HEIGHT  32u    /* total rows  (must be 2 × ROW_PAIRS)   */
#define HUB75_ROW_PAIRS     (HUB75_PANEL_HEIGHT / 2u)   /* = 16             */

/* ── GPIO: row address bits A, B, C and D ───────────────────────────────────────
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

typedef union {
    uint16_t color;     // Access the entire byte at once
    struct {
        uint16_t b : 5; // 5 bits
        uint16_t g : 5; // 5 bits
        uint16_t r : 5; // 5 bits
        bool mask : 1; 	// 1 bit
    } bits;             // Total: 5 + 5 + 5 + 1 = 16 bits
} ColorBitfield;

void HUB75_Init(XSPI_HandleTypeDef *hospi);

void HUB75_PrepareRowToDraw(uint8_t abcd);

bool HUB75_StartDrawing(void);

void HUB75_StopDrawing(void);

void HUB75_SwapDisplayFrame(void);

void HUB75_CopyFrame(ColorBitfield *frame, uint16_t size);

void HUB75_CopyPreviousFrame(void);

/**
 * @brief  Write a 1-bit-per-channel pixel to the software framebuffer.
 *         Call this to build the image; changes are only displayed after
 *         the next HUB75_Refresh().
 *
 * @param  x   Column        (0 … HUB75_PANEL_WIDTH-1)
 * @param  y   Absolute row  (0 … HUB75_PANEL_HEIGHT-1)
 * @param  r     Red   (0 or 1)
 * @param  g     Green (0 or 1)
 * @param  b     Blue  (0 or 1)
 */
void HUB75_SetPixelRGB(uint16_t x, uint16_t y,
                    uint8_t r, uint8_t g, uint8_t b);

void HUB75_SetPixelColor(uint16_t x, uint16_t y,
					ColorBitfield color);

void HUB75_SetPixelColorAlpha(int x, int y,
    ColorBitfield color, float alpha);

void HUB75_ChangeDrawFrameColor(ColorBitfield color);
void HUB75_DrawLine(int x0, int y0, int x1, int y1, ColorBitfield color);
void HUB75_DrawLineAA(float x0, float y0, float x1, float y1, ColorBitfield color);
void HUB75_DrawRect(int x0, int y0, int x1, int y1, ColorBitfield color);

/**
 * @brief  Fill every pixel with the same colour.
 */
void HUB75_FillColor(uint8_t r, uint8_t g, uint8_t b);

void HUB75_SetChar(char character, int x, int y, ColorBitfield color, bool transparent);
void HUB75_SetStr(char * dString, int x, int y, ColorBitfield color, bool transparent, bool wrapAround);

/**
 * @brief  Clear (turn off) all pixels.
 */
void HUB75_Clear(void);
void HUB75_ClearActive(void);

/**
 * @brief  Push the framebuffer to the panel.
 *         Performs 16 OctoSPI row transfers (one per row-pair)
 *         grouped into 8 blocks by the A,B,C,D GPIO value.
 *         Call this repeatedly (e.g. from a 1 kHz timer callback).
 * @retval HAL_OK on success, HAL_ERROR if any OSPI transfer fails.
 */
HAL_StatusTypeDef HUB75_Refresh(void);

#ifdef __cplusplus
}
#endif

#endif /* HUB75_OSPI_H */
