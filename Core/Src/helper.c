/*
 * helper.c
 *
 *  Created on: Jun 6, 2026
 *      Author: szneqz
 */

#include "helper.h"

float Fract(float x) {
	return x - ((int) x);
}

float Mix(float a, float b, float t) {
	return a + (b - a) * t;
}

float Clampf(float value, float min, float max)
{
    if (value < min)
        return min;
    if (value > max)
        return max;
    return value;
}

ColorBitfield HSVtoRGB(float h, float s, float v) {
	ColorBitfield result = { color: 0x0000 };
	result.bits.r = v * Mix(1.0, Clampf(fabsf(Fract(h + 1.0) * 6.0 - 3.0) - 1.0, 0.0, 1.0), s) * 31;
	result.bits.g = v * Mix(1.0, Clampf(fabsf(Fract(h + 0.6666666) * 6.0 - 3.0) - 1.0, 0.0, 1.0), s) * 31;
	result.bits.b = v * Mix(1.0, Clampf(fabsf(Fract(h + 0.3333333) * 6.0 - 3.0) - 1.0, 0.0, 1.0), s) * 31;

	return result;
}
