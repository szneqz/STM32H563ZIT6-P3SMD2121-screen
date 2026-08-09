/*
 * nes.h
 *
 *  Created on: Aug 2, 2026
 *      Author: mishcat
 */

#ifndef INC_NES_H_
#define INC_NES_H_

// For PC only!
//#include <stdio.h>
//#include <stdlib.h>
//#include <string.h>

#include <stdbool.h>
#include <stdint.h>

#define NES_HEADER_SIZE (16)

void NES_Run();
void NES_Reset();
void NES_PrepareDisplay();
void NES_DebugVRAM();
void NES_Init();
void NES_Logic();

#endif /* INC_NES_H_ */
