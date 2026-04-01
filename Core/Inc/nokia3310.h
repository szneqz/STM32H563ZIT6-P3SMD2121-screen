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

void NOKIA_Select();
void NOKIA_Unselect();
void NOKIA_Cmd(uint8_t cmd);
void NOKIA_Data(uint8_t *data);
void NOKIA_Data_Single(uint8_t data);
void NOKIA_Reset();
void NOKIA_Init(SPI_HandleTypeDef *hspi);
void NOKIA_Clear();

#endif /* SRC_NOKIA3310_H_ */
