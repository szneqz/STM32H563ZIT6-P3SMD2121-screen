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
#define ASTEROIDS_NOKIA_X0 1
#define ASTEROIDS_NOKIA_X1 50
#define ASTEROIDS_NOKIA_Y0 1
#define ASTEROIDS_NOKIA_Y1 32

#define ASTEROIDS_MAX_BULLETS 4

static void ASTEROIDS_Shoot(void);
static void ASTEROIDS_DrawBullets(void);
static void ASTEROIDS_DrawShip(void);
static void ASTEROIDS_DrawShipLines(struct Vertex v0, struct Vertex v1, struct Vertex v2, struct Vertex v3);
static void ASTEROIDS_AsteroidLogic(void);
static void ASTEROIDS_DrawAsteroid(bool copyPixels);
static void ASTEROIDS_DrawBorder(void);

extern volatile uint32_t randomNumber;

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

const ColorBitfield ASTEROIDS_asteroidColor = { .bits.r = 20, .bits.g = 0, .bits.b = 31 };
const uint8_t asteroidPointsNr = 10;
const float asteroidPointRotations[10] = {0.0f, 0.6283f, 1.2566f, 1.8849f, 2.5132f, 3.1416f, 3.7698f, 4.3981f, 5.0264f, 5.6547f};
const float asteroidPointDistances[10] = {4.0f, 3.5f, 4.0f, 4.0f, 4.0f, 3.0f, 4.0f, 4.0f, 3.8f, 4.0f};
const float asteroidMinSpeed = 0.2f;
const float asteroidMaxSpeed = 0.35f;
const float asteroidMinRotationSpeed = 0.01f;
const float asteroidMaxRotationSpeed = 0.1f;
struct Vertex asteroidPosition = {-1, -1};
float asteroidRotation = 0.0f;
float asteroidRotationSpeed = 0.0f;
float asteroidMoveRotation = 0.0f;
float asteroidMoveSpeed = 0.0f;


void ASTEROIDS_Init(void) {
	NOKIA_StartDataPrepare();
	NOKIA_Clear();

	HUB75_Clear();

	DrawEmblem();

	NOKIA_StopDataPrepare();
	NOKIA_SendData();

	asteroidPosition.x = -1;
	asteroidPosition.y = -1;
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

		ASTEROIDS_AsteroidLogic();

		if (HUB75_StartDrawing()) {
		NOKIA_StartDataPrepare();
		NOKIA_ClearActive();

		HUB75_ClearActive();

		ASTEROIDS_DrawAsteroid(true);
		ASTEROIDS_DrawShip();
		ASTEROIDS_DrawBullets();
		ASTEROIDS_DrawBorder();
		DrawEmblem();

		NOKIA_StopDataPrepare();
		NOKIA_SendData();
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

			NOKIA_SetPixel(bullets[i].x + ASTEROIDS_NOKIA_X0 - ASTEROIDS_X00, bullets[i].y + ASTEROIDS_NOKIA_Y0, true);
			NOKIA_SetPixel(bullets[i].x + 1 + ASTEROIDS_NOKIA_X0 - ASTEROIDS_X00, bullets[i].y + ASTEROIDS_NOKIA_Y0, true);
			NOKIA_SetPixel(bullets[i].x + ASTEROIDS_NOKIA_X0 - ASTEROIDS_X00, bullets[i].y + 1 + ASTEROIDS_NOKIA_Y0, true);
			NOKIA_SetPixel(bullets[i].x - 1 + ASTEROIDS_NOKIA_X0 - ASTEROIDS_X00, bullets[i].y + ASTEROIDS_NOKIA_Y0, true);
			NOKIA_SetPixel(bullets[i].x + ASTEROIDS_NOKIA_X0 - ASTEROIDS_X00, bullets[i].y - 1 + ASTEROIDS_NOKIA_Y0, true);
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

	//Nokia screen
	NOKIA_SetLineInBorders(v0.x + ASTEROIDS_NOKIA_X0 - ASTEROIDS_X00, v0.y + ASTEROIDS_NOKIA_Y0, v1.x + ASTEROIDS_NOKIA_X0 - ASTEROIDS_X00, v1.y + ASTEROIDS_NOKIA_Y0, ASTEROIDS_NOKIA_X0, ASTEROIDS_NOKIA_Y0, ASTEROIDS_NOKIA_X1, ASTEROIDS_NOKIA_Y1, true, true);
	NOKIA_SetLineInBorders(v1.x + ASTEROIDS_NOKIA_X0 - ASTEROIDS_X00, v1.y + ASTEROIDS_NOKIA_Y0, v2.x + ASTEROIDS_NOKIA_X0 - ASTEROIDS_X00, v2.y + ASTEROIDS_NOKIA_Y0, ASTEROIDS_NOKIA_X0, ASTEROIDS_NOKIA_Y0, ASTEROIDS_NOKIA_X1, ASTEROIDS_NOKIA_Y1, true, true);
	NOKIA_SetLineInBorders(v2.x + ASTEROIDS_NOKIA_X0 - ASTEROIDS_X00, v2.y + ASTEROIDS_NOKIA_Y0, v3.x + ASTEROIDS_NOKIA_X0 - ASTEROIDS_X00, v3.y + ASTEROIDS_NOKIA_Y0, ASTEROIDS_NOKIA_X0, ASTEROIDS_NOKIA_Y0, ASTEROIDS_NOKIA_X1, ASTEROIDS_NOKIA_Y1, true, true);
	NOKIA_SetLineInBorders(v3.x + ASTEROIDS_NOKIA_X0 - ASTEROIDS_X00, v3.y + ASTEROIDS_NOKIA_Y0, v0.x + ASTEROIDS_NOKIA_X0 - ASTEROIDS_X00, v0.y + ASTEROIDS_NOKIA_Y0, ASTEROIDS_NOKIA_X0, ASTEROIDS_NOKIA_Y0, ASTEROIDS_NOKIA_X1, ASTEROIDS_NOKIA_Y1, true, true);
}

static void ASTEROIDS_AsteroidLogic(void) {
	if (asteroidPosition.x == -1 && asteroidPosition.y == -1) {
		uint32_t wallNr = randomNumber % 4;

		if (wallNr == 0 || wallNr == 2) {
			asteroidPosition.x = randomNumber % (ASTEROIDS_WIDTH - 1) + 1;
			if (wallNr == 0) asteroidPosition.y = 1;
			else asteroidPosition.y = ASTEROIDS_HEIGHT - 1;
		} else {
			asteroidPosition.y = randomNumber % (ASTEROIDS_HEIGHT - 1) + 1;
			if (wallNr == 1) asteroidPosition.x = 1;
			else asteroidPosition.x = ASTEROIDS_WIDTH - 1;
		}

		struct Vertex towardsCenter = {-(ASTEROIDS_WIDTH / 2 - asteroidPosition.x), -(ASTEROIDS_HEIGHT / 2 -  asteroidPosition.y)};

		asteroidMoveRotation = CalculateAngle(towardsCenter);

		asteroidRotationSpeed = ((float)(randomNumber & 0xffff) / (float)UINT16_MAX) * (asteroidMaxRotationSpeed - asteroidMinRotationSpeed) + asteroidMinRotationSpeed;
		asteroidMoveSpeed = ((float)(randomNumber >> 16) / (float)UINT16_MAX) * (asteroidMaxSpeed - asteroidMinSpeed) + asteroidMinSpeed;
	} else {
		struct Vertex forwardVector = CalculatePoint(0.0f, 0.0f, asteroidMoveRotation, asteroidMoveSpeed, 0.0f);

		asteroidPosition.x += forwardVector.x;
		asteroidPosition.y += forwardVector.y;

		if (asteroidPosition.x > ASTEROIDS_WIDTH) asteroidPosition.x = 0;
		if (asteroidPosition.x < 0) asteroidPosition.x = ASTEROIDS_WIDTH;
		if (asteroidPosition.y > ASTEROIDS_HEIGHT) asteroidPosition.y = 0;
		if (asteroidPosition.y < 0) asteroidPosition.y = ASTEROIDS_HEIGHT;

		asteroidRotation += asteroidRotationSpeed;

		while (asteroidRotation > (M_PI * 2)) {
			asteroidRotation -= (M_PI * 2);
		}
		while (asteroidRotation < 0) {
			asteroidRotation += (M_PI * 2);
		}
	}
}

static void ASTEROIDS_DrawAsteroid(bool copyPixels) {
	if (asteroidPosition.x != -1 && asteroidPosition.y != -1) {
		struct Vertex prevVertex = CalculatePoint(asteroidPosition.x + ASTEROIDS_X00, asteroidPosition.y, asteroidRotation, asteroidPointDistances[0], asteroidPointRotations[0]);

		for (uint8_t i = 0; i < asteroidPointsNr; i++) {
			uint8_t nexti = (i + 1) % asteroidPointsNr;

			struct Vertex thisVertex = CalculatePoint(asteroidPosition.x + ASTEROIDS_X00, asteroidPosition.y, asteroidRotation, asteroidPointDistances[nexti], asteroidPointRotations[nexti]);

			//first screen
			HUB75_DrawLineAAInBorders(prevVertex.x, prevVertex.y, thisVertex.x, thisVertex.y, ASTEROIDS_X00, ASTEROIDS_Y0, ASTEROIDS_X01, ASTEROIDS_Y1, ASTEROIDS_asteroidColor, copyPixels);

			//second screen
			HUB75_DrawLineAAInBorders(prevVertex.x + ASTEROIDS_WIDTH, prevVertex.y, thisVertex.x + ASTEROIDS_WIDTH, thisVertex.y, ASTEROIDS_X10, ASTEROIDS_Y0, ASTEROIDS_X11, ASTEROIDS_Y1, ASTEROIDS_asteroidColor, copyPixels);

			//Nokia screen
			NOKIA_SetLineInBorders(prevVertex.x + ASTEROIDS_NOKIA_X0 - ASTEROIDS_X00, prevVertex.y + ASTEROIDS_NOKIA_Y0, thisVertex.x + ASTEROIDS_NOKIA_X0 - ASTEROIDS_X00, thisVertex.y + ASTEROIDS_NOKIA_Y0, ASTEROIDS_NOKIA_X0, ASTEROIDS_NOKIA_Y0, ASTEROIDS_NOKIA_X1, ASTEROIDS_NOKIA_Y1, true, copyPixels);

			prevVertex = thisVertex;
		}
	}
}

static void ASTEROIDS_DrawBorder(void) {
	const ColorBitfield white = {.bits.r = 8, .bits.g = 8, .bits.b = 8};

	HUB75_DrawLine(ASTEROIDS_X00 - 1, 0, ASTEROIDS_X00 - 1, 32, white);
	HUB75_DrawLine(ASTEROIDS_X11 + 1, 0, ASTEROIDS_X11 + 1, 32, white);

	NOKIA_SetRect(ASTEROIDS_NOKIA_X0 - 1, ASTEROIDS_NOKIA_Y0 - 1, ASTEROIDS_NOKIA_X1 + 1, ASTEROIDS_NOKIA_Y1 + 1, false, true);
}
