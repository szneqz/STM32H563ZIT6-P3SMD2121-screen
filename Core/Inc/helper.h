/*
 * helper.c
 *
 *  Created on: Jun 6, 2026
 *      Author: szneqz
 */

#ifndef INC_HELPER_C_
#define INC_HELPER_C_

#include "hub75_ospi.h"

float Fract(float x);
float Mix(float a, float b, float t);
float Clampf(float value, float min, float max);
ColorBitfield HSVtoRGB(float h, float s, float v);

#endif /* INC_HELPER_C_ */
