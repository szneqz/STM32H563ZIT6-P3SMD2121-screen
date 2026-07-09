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

float Clampf(float value, float min, float max) {
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

struct Vertex CalculatePoint(float cx, float cy, float angle, float distance, float localAngle) {
	float x = cx + distance * cosf(angle + localAngle);
	float y = cy + distance * sinf(angle + localAngle);
	return (struct Vertex){x, y};
}

float CalculateAngle(struct Vertex v) {
	float angle = atan2(v.y, v.x);

	if (angle < 0)
	    angle += 2.0 * M_PI;

	return angle;
}

float fpart(float x) {
    return x - floorf(x);
}

float rfpart(float x) {
    return 1.0f - fpart(x);
}

float vectorMagnitude(struct Vertex vector) {
	return sqrtf(vector.x * vector.x + vector.y * vector.y);
}
