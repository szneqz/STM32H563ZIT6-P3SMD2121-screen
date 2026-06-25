/*
 * helper.c
 *
 *  Created on: Jun 6, 2026
 *      Author: szneqz
 */

#ifndef INC_HELPER_C_
#define INC_HELPER_C_

#include "hub75_ospi.h"

struct Vertex {
	float x;
	float y;
};

float Fract(float x);
float Mix(float a, float b, float t);
float Clampf(float value, float min, float max);
ColorBitfield HSVtoRGB(float h, float s, float v);
struct Vertex CalculatePoint(float cx, float cy, float angle, float distance, float localAngle);
float fpart(float x);
float rfpart(float x);
float vectorMagnitude(struct Vertex vector);

#endif /* INC_HELPER_C_ */
