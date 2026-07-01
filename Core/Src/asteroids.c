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

#define ASTEROIDS_MAX_BULLETS 4

static void ASTEROIDS_Shoot(void);
static void ASTEROIDS_DrawBullets(void);
static void ASTEROIDS_DrawShip(void);
static void ASTEROIDS_DrawShipLines(struct Vertex v0, struct Vertex v1, struct Vertex v2, struct Vertex v3);
static void ASTEROIDS_DrawBorder(void);

const ColorBitfield ASTEROIDS_shipColor = { .bits.r = 0, .bits.g = 31, .bits.b = 15 };
const uint32_t ASTEROIDS_maxPlayingDelay = 30;
float ASTEROIDS_shipRotation = 0;
struct Vertex ASTEROIDS_shipPosition = {ASTEROIDS_WIDTH / 2, ASTEROIDS_HEIGHT / 2};
const float shipPointRotations[4] = {0.0f, 2.0943f, 3.1416f, 4.1888f};
const float shipPointDistances[4] = {4.0f, 2.0f, 0.0f, 2.0f};
const float maxSpeed = 1.0f;
const float acceleration = 0.05f;
const float deceleration = 0.005f;
struct Vertex speedVector = {0, 0};

const ColorBitfield ASTEROIDS_bulletColor = { .bits.r = 31, .bits.g = 25, .bits.b = 0 };
struct Vertex bullets[4] = {{-1, -1}, {-1, -1}, {-1, -1}, {-1, -1}};
float bulletsRotation[4] = {0, 0, 0, 0};
const float tipPointRotation = 0.0f;
const float tipPointDistance = 4.0f;
const float maxBulletSpeed = 2.0f;
const uint32_t maxShootDelay = 200;

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
	static uint32_t lastShootMillis = 0;
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

		if (GAMEPAD_GetClickButton(A)) {
			GAMEPAD_SetClickReadFlag(A);

			if (actualMillis > lastShootMillis) {
				lastShootMillis = actualMillis + maxShootDelay;
				ASTEROIDS_Shoot();
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
		ASTEROIDS_DrawBullets();
		ASTEROIDS_DrawBorder();
		}
	}
}

static void ASTEROIDS_Shoot(void) {
	struct Vertex tipPosition = CalculatePoint(ASTEROIDS_shipPosition.x + ASTEROIDS_X00, ASTEROIDS_shipPosition.y, ASTEROIDS_shipRotation, tipPointDistance, tipPointRotation);

	if (tipPosition.x < ASTEROIDS_X00) tipPosition.x += ASTEROIDS_WIDTH;
	else if (tipPosition.x > ASTEROIDS_X01) tipPosition.x -= ASTEROIDS_WIDTH;
	if (tipPosition.y < ASTEROIDS_Y0) tipPosition.y += ASTEROIDS_HEIGHT;
	else if (tipPosition.y > ASTEROIDS_Y1) tipPosition.y -= ASTEROIDS_HEIGHT;

	for (int i = 0; i < ASTEROIDS_MAX_BULLETS; i++) {
		if (bullets[i].x == -1 && bullets[i].y == -1) {
			bullets[i].x = tipPosition.x;
			bullets[i].y = tipPosition.y;
			bulletsRotation[i] = ASTEROIDS_shipRotation;
			break;
		}
	}
}

static void ASTEROIDS_DrawBullets(void) {
	for (int i = 0; i < ASTEROIDS_MAX_BULLETS; i++) {
		if (bullets[i].x != -1 && bullets[i].y != -1) {
			struct Vertex forwardVector = CalculatePoint(0.0f, 0.0f, bulletsRotation[i], maxBulletSpeed, 0.0f);

			bullets[i].x += forwardVector.x;
			bullets[i].y += forwardVector.y;

			if (bullets[i].x < ASTEROIDS_X00 || bullets[i].x > ASTEROIDS_X01 ||
					bullets[i].y < ASTEROIDS_Y0 || bullets[i].y > ASTEROIDS_Y1) {
				bullets[i].x = -1;
				bullets[i].y = -1;
			}

			HUB75_DrawLineAAInBorders(bullets[i].x, bullets[i].y, bullets[i].x, bullets[i].y, ASTEROIDS_X00, ASTEROIDS_Y0, ASTEROIDS_X01, ASTEROIDS_Y1, ASTEROIDS_bulletColor, false);
			HUB75_DrawLineAAInBorders(bullets[i].x + ASTEROIDS_WIDTH, bullets[i].y, bullets[i].x + ASTEROIDS_WIDTH, bullets[i].y, ASTEROIDS_X10, ASTEROIDS_Y0, ASTEROIDS_X11, ASTEROIDS_Y1, ASTEROIDS_bulletColor, false);
		}
	}
}

static void ASTEROIDS_DrawShip(void) {
	//main ship
	struct Vertex result0 = CalculatePoint(ASTEROIDS_shipPosition.x + ASTEROIDS_X00, ASTEROIDS_shipPosition.y, ASTEROIDS_shipRotation, shipPointDistances[0], shipPointRotations[0]);
	struct Vertex result1 = CalculatePoint(ASTEROIDS_shipPosition.x + ASTEROIDS_X00, ASTEROIDS_shipPosition.y, ASTEROIDS_shipRotation, shipPointDistances[1], shipPointRotations[1]);
	struct Vertex result2 = CalculatePoint(ASTEROIDS_shipPosition.x + ASTEROIDS_X00, ASTEROIDS_shipPosition.y, ASTEROIDS_shipRotation, shipPointDistances[2], shipPointRotations[2]);
	struct Vertex result3 = CalculatePoint(ASTEROIDS_shipPosition.x + ASTEROIDS_X00, ASTEROIDS_shipPosition.y, ASTEROIDS_shipRotation, shipPointDistances[3], shipPointRotations[3]);

	ASTEROIDS_DrawShipLines(result0, result1, result2, result3);
}

static void ASTEROIDS_DrawShipLines(struct Vertex v0, struct Vertex v1, struct Vertex v2, struct Vertex v3) {
	//first screen
	HUB75_DrawLineAAInBorders(v0.x, v0.y, v1.x, v1.y, ASTEROIDS_X00, ASTEROIDS_Y0, ASTEROIDS_X01, ASTEROIDS_Y1, ASTEROIDS_shipColor, true);
	HUB75_DrawLineAAInBorders(v1.x, v1.y, v2.x, v2.y, ASTEROIDS_X00, ASTEROIDS_Y0, ASTEROIDS_X01, ASTEROIDS_Y1, ASTEROIDS_shipColor, true);
	HUB75_DrawLineAAInBorders(v2.x, v2.y, v3.x, v3.y, ASTEROIDS_X00, ASTEROIDS_Y0, ASTEROIDS_X01, ASTEROIDS_Y1, ASTEROIDS_shipColor, true);
	HUB75_DrawLineAAInBorders(v3.x, v3.y, v0.x, v0.y, ASTEROIDS_X00, ASTEROIDS_Y0, ASTEROIDS_X01, ASTEROIDS_Y1, ASTEROIDS_shipColor, true);

	//second screen
	HUB75_DrawLineAAInBorders(v0.x + ASTEROIDS_WIDTH, v0.y, v1.x + ASTEROIDS_WIDTH, v1.y, ASTEROIDS_X10, ASTEROIDS_Y0, ASTEROIDS_X11, ASTEROIDS_Y1, ASTEROIDS_shipColor, true);
	HUB75_DrawLineAAInBorders(v1.x + ASTEROIDS_WIDTH, v1.y, v2.x + ASTEROIDS_WIDTH, v2.y, ASTEROIDS_X10, ASTEROIDS_Y0, ASTEROIDS_X11, ASTEROIDS_Y1, ASTEROIDS_shipColor, true);
	HUB75_DrawLineAAInBorders(v2.x + ASTEROIDS_WIDTH, v2.y, v3.x + ASTEROIDS_WIDTH, v3.y, ASTEROIDS_X10, ASTEROIDS_Y0, ASTEROIDS_X11, ASTEROIDS_Y1, ASTEROIDS_shipColor, true);
	HUB75_DrawLineAAInBorders(v3.x + ASTEROIDS_WIDTH, v3.y, v0.x + ASTEROIDS_WIDTH, v0.y, ASTEROIDS_X10, ASTEROIDS_Y0, ASTEROIDS_X11, ASTEROIDS_Y1, ASTEROIDS_shipColor, true);
}

static void ASTEROIDS_DrawBorder(void) {
	const ColorBitfield white = {.bits.r = 8, .bits.g = 8, .bits.b = 8};

	HUB75_DrawLine(ASTEROIDS_X00 - 1, 0, ASTEROIDS_X00 - 1, 32, white);
	HUB75_DrawLine(ASTEROIDS_X11 + 1, 0, ASTEROIDS_X11 + 1, 32, white);
}
