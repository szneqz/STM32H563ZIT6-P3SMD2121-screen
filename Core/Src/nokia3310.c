#include "nokia3310.h"
#include <string.h>

static SPI_HandleTypeDef *s_hspi = NULL;
volatile bool spi_busy = 0;

static uint8_t s_framebuf[2][NOKIA_PANEL_DATA_SIZE];
static uint8_t s_datatype[2];
static uint16_t s_datasize[2];

static uint8_t current_draw_frame = 1;
static uint8_t current_display_frame = 1;
static bool isDrawing = false;
static bool alreadyDisplayed = false;

void NOKIA_Select(void) {
    HAL_GPIO_WritePin(NOKIA_SCE_PORT, NOKIA_SCE_PIN, GPIO_PIN_RESET);
}

void NOKIA_Unselect(void) {
    HAL_GPIO_WritePin(NOKIA_SCE_PORT, NOKIA_SCE_PIN, GPIO_PIN_SET);
}

void NOKIA_SendCmd(uint8_t *cmd, uint16_t size)
{
	if (NOKIA_StartDataPrepare()) {
	memcpy(s_framebuf[current_draw_frame], cmd, size);
	s_datatype[current_draw_frame] = NOKIA_CMD_TYPE;
	s_datasize[current_draw_frame] = size;
	NOKIA_StopDataPrepare();
	}

	if (!spi_busy) {
		spi_busy = true;
		if (NOKIA_SwapDisplayFrame()) {
			HAL_GPIO_WritePin(NOKIA_DC_PORT, NOKIA_DC_PIN, GPIO_PIN_RESET); // command
			NOKIA_Select();
			HAL_SPI_Transmit_DMA(s_hspi, s_framebuf[current_display_frame], s_datasize[current_display_frame]);
		}
	}
}

void NOKIA_SendData(void) {
	if (!spi_busy) {
		spi_busy = true;
		if (NOKIA_SwapDisplayFrame()) {
			HAL_GPIO_WritePin(NOKIA_DC_PORT, NOKIA_DC_PIN, GPIO_PIN_SET); // data
			NOKIA_Select();
			HAL_SPI_Transmit_DMA(s_hspi, s_framebuf[current_display_frame], s_datasize[current_display_frame]);
		}
	}
}

void NOKIA_SetPixel(uint8_t x, uint8_t y, bool black) {
    int index = x + (y / 8) * NOKIA_PANEL_WIDTH;

    if (black)
    	s_framebuf[current_draw_frame][index] |= (1 << (y % 8));
    else
    	s_framebuf[current_draw_frame][index] &= ~(1 << (y % 8));
}

bool NOKIA_StartDataPrepare(void) {
	if (current_display_frame == (current_draw_frame ^ 1)) {
		isDrawing = false;
		return false;
	}
	current_draw_frame ^= 1;
	isDrawing = true;
	s_datatype[current_draw_frame] = NOKIA_DATA_TYPE;
	s_datasize[current_draw_frame] = NOKIA_PANEL_DATA_SIZE;
	return true;
}

void NOKIA_StopDataPrepare(void) {
	isDrawing = false;
	alreadyDisplayed = false;
}

bool NOKIA_SwapDisplayFrame(void) {
	if (!alreadyDisplayed && (!isDrawing || current_draw_frame != (current_display_frame ^ 1))) {
		current_display_frame ^= 1;
		alreadyDisplayed = true;
		return true;
	}
	return false;
}

void NOKIA_Reset(void)
{
    HAL_GPIO_WritePin(NOKIA_RST_PORT, NOKIA_RST_PIN, GPIO_PIN_RESET);
    HAL_Delay(10);
    HAL_GPIO_WritePin(NOKIA_RST_PORT, NOKIA_RST_PIN, GPIO_PIN_SET);
}

void NOKIA_Init(SPI_HandleTypeDef *hspi)
{
	s_hspi = hspi;
	
    NOKIA_Reset();
    NOKIA_Clear();

    uint8_t cmd[] = {
    		0x21,	// Extended instruction set
			0xBF,	// Contrast
			0x04,	// Temp coefficient
			0x14,	// Bias mode
			0x20,	// Basic instruction set
			0x0C	// Normal display (not inverted)
    };

    NOKIA_SendCmd(cmd, 6);
}

void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi)
{
    if (hspi->Instance == SPI1) {
    	if (NOKIA_SwapDisplayFrame() && hspi->State == HAL_SPI_STATE_READY) {
    		HAL_GPIO_WritePin(NOKIA_DC_PORT, NOKIA_DC_PIN, s_datatype[current_display_frame] ? GPIO_PIN_SET : GPIO_PIN_RESET);
    		NOKIA_Select();

    		HAL_SPI_Transmit_DMA(s_hspi, s_framebuf[current_display_frame], s_datasize[current_display_frame]);
    	} else {
			NOKIA_Unselect();
			spi_busy = false;
    	}
    }
}

void NOKIA_Clear(void)
{
	memset(s_framebuf, 0, sizeof(s_framebuf));
}
