/*
 * asteroids.c
 *
 *  Created on: Jun 23, 2026
 *      Author: szneqz
 */

#include "asteroids.h"
#include "main_logic.h"
#include "math.h"

ColorBitfield ASTEROIDS_white = { .bits.r = 31, .bits.g = 31, .bits.b = 31 };
uint32_t ASTEROIDS_maxPlayingDelay = 30;
float ASTEROIDS_rotation = 0;
float pointRotations[4] = {0.0f, 2.0943f, 3.1416f, 4.1888f};
float pointDistances[4] = {3.0f, 2.0f, 0.5f, 2.0f};

void ASTEROIDS_Init(void) {
	NOKIA_StartDataPrepare();
	NOKIA_Clear();

	HUB75_Clear();

	DrawEmblem();

	NOKIA_StopDataPrepare();
	NOKIA_SendData();
}

void ASTEROIDS_Logic(void) {
	static uint32_t lastMillis = 0;
	uint32_t actualMillis = HAL_GetTick();

	while (actualMillis > lastMillis) {
		lastMillis += ASTEROIDS_maxPlayingDelay;

		ASTEROIDS_rotation += 0.1f;

		while (ASTEROIDS_rotation > (M_PI * 2)) {
			ASTEROIDS_rotation -= (M_PI * 2);
		}

		if (HUB75_StartDrawing()) {
		HUB75_ClearActive();

		struct Vertex result0 = CalculatePoint(32, 16, ASTEROIDS_rotation, pointDistances[0], pointRotations[0]);
		struct Vertex result1 = CalculatePoint(32, 16, ASTEROIDS_rotation, pointDistances[1], pointRotations[1]);
		struct Vertex result2 = CalculatePoint(32, 16, ASTEROIDS_rotation, pointDistances[2], pointRotations[2]);
		struct Vertex result3 = CalculatePoint(32, 16, ASTEROIDS_rotation, pointDistances[3], pointRotations[3]);

		HUB75_DrawLine(result0.x, result0.y, result1.x, result1.y, ASTEROIDS_white);
		HUB75_DrawLine(result1.x, result1.y, result2.x, result2.y, ASTEROIDS_white);
		HUB75_DrawLine(result2.x, result2.y, result3.x, result3.y, ASTEROIDS_white);
		HUB75_DrawLine(result3.x, result3.y, result0.x, result0.y, ASTEROIDS_white);
		}
	}
}
