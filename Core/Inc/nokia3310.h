/*
 * NOKIA3310.h
 *
 *  Created on: Mar 31, 2026
 *      Author: szneqz
 */

#ifndef SRC_NOKIA3310_H_
#define SRC_NOKIA3310_H_

#include "main.h"
#include <stdbool.h>

#define NOKIA_SCE_PORT      NOKIA_SCE_GPIO_Port
#define NOKIA_SCE_PIN       NOKIA_SCE_Pin

#define NOKIA_DC_PORT		NOKIA_DC_GPIO_Port
#define NOKIA_DC_PIN		NOKIA_DC_Pin

#define NOKIA_RST_PORT		NOKIA_RST_GPIO_Port
#define NOKIA_RST_PIN		NOKIA_RST_Pin

#define NOKIA_PANEL_WIDTH   84u
#define NOKIA_PANEL_HEIGHT  48u
#define NOKIA_PANEL_LINES   (NOKIA_PANEL_HEIGHT / 8u)
#define NOKIA_PANEL_DATA_SIZE (NOKIA_PANEL_WIDTH * NOKIA_PANEL_LINES)

#define NOKIA_CMD_TYPE 0u
#define NOKIA_DATA_TYPE 1u

void NOKIA_Select(void);
void NOKIA_Unselect(void);
void NOKIA_SendCmd(uint8_t *cmd, uint16_t size);
void NOKIA_SendData(void);
void NOKIA_SetPixel(uint8_t x, uint8_t y, bool black);
bool NOKIA_StartDataPrepare(void);
void NOKIA_StopDataPrepare(void);
bool NOKIA_SwapDisplayFrame(void);
void NOKIA_Reset(void);
void NOKIA_Init(SPI_HandleTypeDef *hspi);
void NOKIA_Clear(void);

#endif /* SRC_NOKIA3310_H_ */
