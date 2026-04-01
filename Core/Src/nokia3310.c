#include "nokia3310.h"

static SPI_HandleTypeDef *s_hspi = NULL;
volatile bool spi_busy = 0;

void NOKIA_Select() {
    HAL_GPIO_WritePin(NOKIA_SCE_PORT, NOKIA_SCE_PIN, GPIO_PIN_RESET);
}

void NOKIA_Unselect() {
    HAL_GPIO_WritePin(NOKIA_SCE_PORT, NOKIA_SCE_PIN, GPIO_PIN_SET);
}

void NOKIA_Cmd(uint8_t cmd)
{
	if (!spi_busy) {
		HAL_GPIO_WritePin(NOKIA_DC_PORT, NOKIA_DC_PIN, GPIO_PIN_RESET); // command
		NOKIA_Select();
		HAL_SPI_Transmit(s_hspi, &cmd, 1, HAL_MAX_DELAY);
		NOKIA_Unselect();
	}
}

void NOKIA_Data(uint8_t *data)
{
	if (!spi_busy) {
		spi_busy = true;
		HAL_GPIO_WritePin(NOKIA_DC_PORT, NOKIA_DC_PIN, GPIO_PIN_SET); // data
		NOKIA_Select();
		HAL_SPI_Transmit_DMA(s_hspi, data, 1);
	}
}

void NOKIA_Data_Single(uint8_t data)
{
	if (!spi_busy) {
		spi_busy = true;
		HAL_GPIO_WritePin(NOKIA_DC_PORT, NOKIA_DC_PIN, GPIO_PIN_SET); // data
		NOKIA_Select();
		HAL_StatusTypeDef status = HAL_SPI_Transmit_DMA(s_hspi, &data, 1);

		if (data > 0x00)
		{
			int a = 0;
			int b = a;
		}
	}
}

void NOKIA_Reset()
{
    HAL_GPIO_WritePin(NOKIA_RST_PORT, NOKIA_RST_PIN, GPIO_PIN_RESET);
    HAL_Delay(10);
    HAL_GPIO_WritePin(NOKIA_RST_PORT, NOKIA_RST_PIN, GPIO_PIN_SET);
}

void NOKIA_Init(SPI_HandleTypeDef *hspi)
{
	s_hspi = hspi;
	
    NOKIA_Reset();

    NOKIA_Cmd(0x21); // Extended instruction set
    NOKIA_Cmd(0xB7); // Contrast
    NOKIA_Cmd(0x04); // Temp coefficient
    NOKIA_Cmd(0x14); // Bias mode

    NOKIA_Cmd(0x20); // Basic instruction set
    NOKIA_Cmd(0x0C); // Normal display (not inverted)
}

void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi)
{
    if (hspi->Instance == SPI1) {
    	NOKIA_Unselect();
        spi_busy = false;
    }
}

void NOKIA_Clear()
{
    for (int i = 0; i < 504; i++) {
    	NOKIA_Data_Single(0x00);
    }
}
