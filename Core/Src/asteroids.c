/*
 * asteroids.c
 *
 *  Created on: Jun 23, 2026
 *      Author: szneqz
 */

#include "asteroids.h"
#include "main_logic.h"
#include "math.h"

#define ASTEROIDS_WIDTH 50
#define ASTEROIDS_HEIGHT 32
#define ASTEROIDS_X00 14
#define ASTEROIDS_X01 63
#define ASTEROIDS_X10 64
#define ASTEROIDS_X11 113
#define ASTEROIDS_Y0 0
#define ASTEROIDS_Y1 31

static void ASTEROIDS_DrawShip(void);
static void ASTEROIDS_DrawShipLines(struct Vertex v0, struct Vertex v1, struct Vertex v2, struct Vertex v3);
static void ASTEROIDS_DrawBorder(void);

ColorBitfield ASTEROIDS_ship = { .bits.r = 0, .bits.g = 31, .bits.b = 15 };
uint32_t ASTEROIDS_maxPlayingDelay = 30;
float ASTEROIDS_shipRotation = 0;
struct Vertex ASTEROIDS_shipPosition = {ASTEROIDS_WIDTH / 2, ASTEROIDS_HEIGHT / 2};
float shipPointRotations[4] = {0.0f, 2.0943f, 3.1416f, 4.1888f};
float shipPointDistances[4] = {4.0f, 2.0f, 0.0f, 2.0f};
float maxSpeed = 1.0f;
float acceleration = 0.05f;
float deceleration = 0.005f;
struct Vertex speedVector = {0, 0};

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

		if (GAMEPAD_GetHoldButton(RIGHT)) ASTEROIDS_shipRotation += 0.1f;
		if (GAMEPAD_GetHoldButton(LEFT)) ASTEROIDS_shipRotation -= 0.1f;
		if (GAMEPAD_GetHoldButton(UP)) {
			struct Vertex forwardAcceleration = CalculatePoint(0.0f, 0.0f, ASTEROIDS_shipRotation, acceleration, 0.0f);
			speedVector.x += forwardAcceleration.x;
			speedVector.y += forwardAcceleration.y;

			float actualSpeed = vectorMagnitude(speedVector);
			if (actualSpeed > maxSpeed) {
				speedVector.x /= actualSpeed;
				speedVector.y /= actualSpeed;
				speedVector.x *= maxSpeed;
				speedVector.y *= maxSpeed;
			}
		} else {
			float actualSpeed = vectorMagnitude(speedVector);
			if (actualSpeed > 0) {
				speedVector.x /= actualSpeed;
				speedVector.y /= actualSpeed;
				speedVector.x *= actualSpeed - deceleration;
				speedVector.y *= actualSpeed - deceleration;
			}
		}

		ASTEROIDS_shipPosition.x += speedVector.x;
		ASTEROIDS_shipPosition.y += speedVector.y;

		if (ASTEROIDS_shipPosition.x > ASTEROIDS_WIDTH) ASTEROIDS_shipPosition.x = 0;
		if (ASTEROIDS_shipPosition.x < 0) ASTEROIDS_shipPosition.x = ASTEROIDS_WIDTH;
		if (ASTEROIDS_shipPosition.y > ASTEROIDS_HEIGHT) ASTEROIDS_shipPosition.y = 0;
		if (ASTEROIDS_shipPosition.y < 0) ASTEROIDS_shipPosition.y = ASTEROIDS_HEIGHT;

		while (ASTEROIDS_shipRotation > (M_PI * 2)) {
			ASTEROIDS_shipRotation -= (M_PI * 2);
		}
		while (ASTEROIDS_shipRotation < 0) {
			ASTEROIDS_shipRotation += (M_PI * 2);
		}

		if (HUB75_StartDrawing()) {
		HUB75_ClearActive();

		ASTEROIDS_DrawShip();
		ASTEROIDS_DrawBorder();
		}
	}
}

static void ASTEROIDS_DrawShip(void) {
	//main ship top
	struct Vertex result0 = CalculatePoint(ASTEROIDS_shipPosition.x + ASTEROIDS_X00, ASTEROIDS_shipPosition.y + ASTEROIDS_HEIGHT, ASTEROIDS_shipRotation, shipPointDistances[0], shipPointRotations[0]);
	struct Vertex result1 = CalculatePoint(ASTEROIDS_shipPosition.x + ASTEROIDS_X00, ASTEROIDS_shipPosition.y + ASTEROIDS_HEIGHT, ASTEROIDS_shipRotation, shipPointDistances[1], shipPointRotations[1]);
	struct Vertex result2 = CalculatePoint(ASTEROIDS_shipPosition.x + ASTEROIDS_X00, ASTEROIDS_shipPosition.y + ASTEROIDS_HEIGHT, ASTEROIDS_shipRotation, shipPointDistances[2], shipPointRotations[2]);
	struct Vertex result3 = CalculatePoint(ASTEROIDS_shipPosition.x + ASTEROIDS_X00, ASTEROIDS_shipPosition.y + ASTEROIDS_HEIGHT, ASTEROIDS_shipRotation, shipPointDistances[3], shipPointRotations[3]);

	ASTEROIDS_DrawShipLines(result0, result1, result2, result3);

	//main ship
	result0.y -= ASTEROIDS_HEIGHT;
	result1.y -= ASTEROIDS_HEIGHT;
	result2.y -= ASTEROIDS_HEIGHT;
	result3.y -= ASTEROIDS_HEIGHT;

	ASTEROIDS_DrawShipLines(result0, result1, result2, result3);

	//main ship bottom
	result0.y -= ASTEROIDS_HEIGHT;
	result1.y -= ASTEROIDS_HEIGHT;
	result2.y -= ASTEROIDS_HEIGHT;
	result3.y -= ASTEROIDS_HEIGHT;

	ASTEROIDS_DrawShipLines(result0, result1, result2, result3);

	//main ship bottom right
	result0.x -= ASTEROIDS_WIDTH;
	result1.x -= ASTEROIDS_WIDTH;
	result2.x -= ASTEROIDS_WIDTH;
	result3.x -= ASTEROIDS_WIDTH;

	ASTEROIDS_DrawShipLines(result0, result1, result2, result3);

	//main ship right
	result0.y += ASTEROIDS_HEIGHT;
	result1.y += ASTEROIDS_HEIGHT;
	result2.y += ASTEROIDS_HEIGHT;
	result3.y += ASTEROIDS_HEIGHT;

	ASTEROIDS_DrawShipLines(result0, result1, result2, result3);

	//main ship top right
	result0.y += ASTEROIDS_HEIGHT;
	result1.y += ASTEROIDS_HEIGHT;
	result2.y += ASTEROIDS_HEIGHT;
	result3.y += ASTEROIDS_HEIGHT;

	ASTEROIDS_DrawShipLines(result0, result1, result2, result3);

	//second screen ship top
	result0.x += 2 * ASTEROIDS_WIDTH;
	result1.x += 2 * ASTEROIDS_WIDTH;
	result2.x += 2 * ASTEROIDS_WIDTH;
	result3.x += 2 * ASTEROIDS_WIDTH;

	ASTEROIDS_DrawShipLines(result0, result1, result2, result3);

	//second screen ship
	result0.y -= ASTEROIDS_HEIGHT;
	result1.y -= ASTEROIDS_HEIGHT;
	result2.y -= ASTEROIDS_HEIGHT;
	result3.y -= ASTEROIDS_HEIGHT;

	ASTEROIDS_DrawShipLines(result0, result1, result2, result3);

	//second screen ship bottom
	result0.y -= ASTEROIDS_HEIGHT;
	result1.y -= ASTEROIDS_HEIGHT;
	result2.y -= ASTEROIDS_HEIGHT;
	result3.y -= ASTEROIDS_HEIGHT;

	ASTEROIDS_DrawShipLines(result0, result1, result2, result3);

	//second screen ship bottom left
	result0.x += ASTEROIDS_WIDTH;
	result1.x += ASTEROIDS_WIDTH;
	result2.x += ASTEROIDS_WIDTH;
	result3.x += ASTEROIDS_WIDTH;

	ASTEROIDS_DrawShipLines(result0, result1, result2, result3);

	//second screen ship left
	result0.y += ASTEROIDS_HEIGHT;
	result1.y += ASTEROIDS_HEIGHT;
	result2.y += ASTEROIDS_HEIGHT;
	result3.y += ASTEROIDS_HEIGHT;

	ASTEROIDS_DrawShipLines(result0, result1, result2, result3);

	//second screen ship top left
	result0.y += ASTEROIDS_HEIGHT;
	result1.y += ASTEROIDS_HEIGHT;
	result2.y += ASTEROIDS_HEIGHT;
	result3.y += ASTEROIDS_HEIGHT;

	ASTEROIDS_DrawShipLines(result0, result1, result2, result3);
}

static void ASTEROIDS_DrawShipLines(struct Vertex v0, struct Vertex v1, struct Vertex v2, struct Vertex v3) {
	HUB75_DrawLineAA(v0.x, v0.y, v1.x, v1.y, ASTEROIDS_ship);
	HUB75_DrawLineAA(v1.x, v1.y, v2.x, v2.y, ASTEROIDS_ship);
	HUB75_DrawLineAA(v2.x, v2.y, v3.x, v3.y, ASTEROIDS_ship);
	HUB75_DrawLineAA(v3.x, v3.y, v0.x, v0.y, ASTEROIDS_ship);
}

static void ASTEROIDS_DrawBorder(void) {
	ColorBitfield white = {.bits.r = 8, .bits.g = 8, .bits.b = 8};
	ColorBitfield black = {.bits.r = 0, .bits.g = 0, .bits.b = 0};

	HUB75_DrawLine(ASTEROIDS_X00 - 1, 0, ASTEROIDS_X00 - 1, 32, white);
	HUB75_DrawLine(ASTEROIDS_X11 + 1, 0, ASTEROIDS_X11 + 1, 32, white);
	HUB75_DrawRect(0, 0, ASTEROIDS_X00 - 2, 32, black);
	HUB75_DrawRect(ASTEROIDS_X11 + 2, 0, 127, 32, black);
}
