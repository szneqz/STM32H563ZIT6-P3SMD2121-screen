/*
 * snake.c
 *
 *  Created on: Jun 1, 2026
 *      Author: szneqz
 */

#include "snake.h"

#define SNAKE_WIDTH 24
#define SNAKE_HEIGHT 15
#define MAX_SNAKE_SEGMENTS 360
#define SNAKE_X00 15
#define SNAKE_X01 62
#define SNAKE_X10 65
#define SNAKE_X11 112
#define SNAKE_Y0 1
#define SNAKE_Y1 30
#define SNAKE_NOKIA_X0 1
#define SNAKE_NOKIA_X1 48
#define SNAKE_NOKIA_Y0 1
#define SNAKE_NOKIA_Y1 30

static ColorBitfield black = { 0x0000 };
static ColorBitfield white = { 0x7FFF };

static void SNAKE_DrawBorder(void);
static void SNAKE_DrawPixel(uint8_t x, uint8_t y, ColorBitfield hubColor, bool nokiaColor);
static void SNAKE_SpawnFruit(void);
static bool SNAKE_CalculateDirection(void);
static float Fract(float x);
static float Mix(float a, float b, float t);
static float Clampf(float value, float min, float max);
static ColorBitfield HSVtoRGB(float h, float s, float v);

enum SNAKE_DIR {
	SNAKE_RIGHT, SNAKE_DOWN, SNAKE_LEFT, SNAKE_UP
};

enum SNAKE_MODE {
	SNAKE_DEAD, SNAKE_STATIC, SNAKE_PLAYING
};

static uint8_t snakePos[MAX_SNAKE_SEGMENTS][2];
static uint16_t head = 0;
static uint16_t tailLen = 3;
static uint8_t snakeDir = SNAKE_RIGHT;
static uint8_t lastSnakeDir = SNAKE_RIGHT;
static uint8_t fruitPos[2] = {0, 0};
static uint8_t snakeMode = SNAKE_STATIC;
static const uint16_t maxSnakePlayingDelay = 500;
static const uint16_t maxSnakeDeadDelay = 1000;
static uint8_t snakeDeadBlinks = 6;
static const uint8_t maxSnakeDeadBlinks = 6;

void SNAKE_Init(void) {
	NOKIA_StartDataPrepare();
	NOKIA_Clear();

	SNAKE_DrawBorder();

	snakePos[0][0] = (uint8_t)(SNAKE_WIDTH / 2);
	snakePos[0][1] = (uint8_t)(SNAKE_HEIGHT / 2);
	snakePos[1][0] = snakePos[0][0] - 1;
	snakePos[1][1] = snakePos[0][1];
	snakePos[2][0] = snakePos[1][0] - 1;
	snakePos[2][1] = snakePos[0][1];

	head = 0;
	tailLen = 3;
	snakeDir = SNAKE_RIGHT;
	lastSnakeDir = SNAKE_RIGHT;
	SNAKE_DrawPixel(fruitPos[0], fruitPos[1], black, false);	//paint over the previous fruit
	SNAKE_SpawnFruit();
	SNAKE_DrawPixel(fruitPos[0], fruitPos[1], white, true);
	snakeMode = SNAKE_STATIC;

	for (uint16_t i = 0; i < tailLen; i++) {
		ColorBitfield tmpColor = HSVtoRGB((float)i / tailLen, 1, 1);
		SNAKE_DrawPixel(snakePos[i][0], snakePos[i][1], tmpColor, true);
	}

	NOKIA_StopDataPrepare();
	NOKIA_SendData();
}

void SNAKE_Logic(void) {
	static uint32_t lastMillis = 0;
	uint32_t actualMillis = HAL_GetTick();

	//don't try to catch up if long time passed
	if (actualMillis > (lastMillis + 2000)) lastMillis = actualMillis;

	if (snakeMode == SNAKE_STATIC) {
		if (SNAKE_CalculateDirection()) {
			snakeMode = SNAKE_PLAYING;
		}
	} else if (snakeMode == SNAKE_PLAYING) {
		SNAKE_CalculateDirection();

		while (actualMillis > lastMillis) {
			lastMillis += maxSnakePlayingDelay;

			//save previous head position
			uint8_t prevHead[2];
			prevHead[0] = snakePos[head][0];
			prevHead[1] = snakePos[head][1];

			//head position should be always moved to tail position
			head--;
			if (head < 0)
				head = tailLen - 1;

			//save previous tail position
			uint8_t prevTail[2];
			prevTail[0] = snakePos[head][0];
			prevTail[1] = snakePos[head][1];

			int8_t movX = 0;
			int8_t movY = 0;
			lastSnakeDir = snakeDir;
			switch (snakeDir) {
			case SNAKE_RIGHT: movX = 1; break;
			case SNAKE_DOWN: movY = 1; break;
			case SNAKE_LEFT: movX = -1; break;
			case SNAKE_UP: movY = -1; break;
			default: break;
			}

			snakePos[head][0] = prevHead[0] + movX;
			snakePos[head][1] = prevHead[1] + movY;

			if (snakePos[head][0] < 0)
				snakePos[head][0] = SNAKE_WIDTH - 1;
			if (snakePos[head][0] >= SNAKE_WIDTH)
				snakePos[head][0] = 0;
			if (snakePos[head][1] < 0)
				snakePos[head][1] = SNAKE_HEIGHT - 1;
			if (snakePos[head][1] >= SNAKE_HEIGHT)
				snakePos[head][1] = 0;

			NOKIA_StartDataPrepare();
			NOKIA_CopyPreviousFrame();

			if (HUB75_StartDrawing()) {
				HUB75_CopyPreviousFrame();

				ColorBitfield tmpColor = HSVtoRGB((float)head / tailLen, 1, 1);
				SNAKE_DrawPixel(snakePos[head][0], snakePos[head][1], tmpColor, true);

				SNAKE_DrawPixel(fruitPos[0], fruitPos[1], white, true);
			}

			//scoring point
			if (snakePos[head][0] == fruitPos[0] && snakePos[head][1] == fruitPos[1]) {
				tailLen++;
				SNAKE_SpawnFruit();

				for (uint8_t i = tailLen; i > head; i--) {
					snakePos[i][0] = snakePos[i - 1][0];
					snakePos[i][1] = snakePos[i - 1][1];
				}

				snakePos[head][0] = prevTail[0];
				snakePos[head][1] = prevTail[1];

				head++;
			}

			//death when crashing into itself
			for (uint8_t i = 0; i < tailLen; i++) {
				if (i == head)
					continue;
				if (snakePos[head][0] == snakePos[i][0] && snakePos[head][1] == snakePos[i][1]) {
					snakeMode = SNAKE_DEAD;
					snakeDeadBlinks = maxSnakeDeadBlinks;
					break;
				}
			}

			SNAKE_DrawPixel(prevTail[0], prevTail[1], black, false); //remove segments on previous position of tail
			NOKIA_StopDataPrepare();
			NOKIA_SendData();
		}
	} else if (snakeMode == SNAKE_DEAD) {
		while (actualMillis > lastMillis) {
			lastMillis += maxSnakeDeadDelay;

			NOKIA_StartDataPrepare();
			NOKIA_CopyPreviousFrame();

			if (HUB75_StartDrawing()) {
				HUB75_CopyPreviousFrame();
			}

			if (snakeDeadBlinks % 2 == 0) {
				for (uint16_t i = 0; i < tailLen; i++) {
					ColorBitfield tmpColor = HSVtoRGB((float)i / tailLen, 1, 1);
					SNAKE_DrawPixel(snakePos[i][0], snakePos[i][1], tmpColor, true);
				}
			} else {
				for (uint16_t i = 0; i < tailLen; i++) {
					SNAKE_DrawPixel(snakePos[i][0], snakePos[i][1], black, true);
				}
			}

			NOKIA_StopDataPrepare();
			NOKIA_SendData();

	        snakeDeadBlinks--;
	        if (snakeDeadBlinks <= 0)
	          SNAKE_Init();
		}
	}
}

static void SNAKE_SpawnFruit(void) {
	  bool isGood = true;

	  do {
	    isGood = true;

	    fruitPos[0] = randomNumber % SNAKE_WIDTH;
	    fruitPos[1] = (randomNumber >> 16) % SNAKE_HEIGHT;

	    for (uint16_t i = 0; i < tailLen; i++) {
	      if (snakePos[i][0] == fruitPos[0] && snakePos[i][1] == fruitPos[1]) {
	        isGood = false;
	        break;
	      }
	    }
	  } while (isGood == false);
}

static void SNAKE_DrawBorder(void) {
	//HUB75 SCREEN
	if (HUB75_StartDrawing()) {
		ColorBitfield white = { .bits.r = 31, .bits.g = 31, .bits.b = 31 };
		uint8_t x0 = SNAKE_X00 - 1;
		uint8_t x1 = SNAKE_X01 + 1;
		uint8_t y0 = SNAKE_Y0 - 1;
		uint8_t y1 = SNAKE_Y1 + 1;

		for (uint8_t x = x0; x <= x1; x++) {
			HUB75_SetPixelColor(x, y0, white);
			HUB75_SetPixelColor(x, y1, white);
		}

		for (uint8_t y = y0; y <= y1; y++) {
			HUB75_SetPixelColor(x0, y, white);
			HUB75_SetPixelColor(x1, y, white);
		}

		x0 = SNAKE_X10 - 1;
		x1 = SNAKE_X11 + 1;

		for (uint8_t x = x0; x <= x1; x++) {
			HUB75_SetPixelColor(x, y0, white);
			HUB75_SetPixelColor(x, y1, white);
		}

		for (uint8_t y = y0; y <= y1; y++) {
			HUB75_SetPixelColor(x0, y, white);
			HUB75_SetPixelColor(x1, y, white);
		}
	}

	//NOKIA SCREEN
	NOKIA_SetLine(SNAKE_NOKIA_X0 - 1, SNAKE_NOKIA_Y0 - 1, SNAKE_NOKIA_X1 + 1, SNAKE_NOKIA_Y0 - 1, true);
	NOKIA_SetLine(SNAKE_NOKIA_X0 - 1, SNAKE_NOKIA_Y1 + 1, SNAKE_NOKIA_X1 + 1, SNAKE_NOKIA_Y1 + 1, true);
	NOKIA_SetLine(SNAKE_NOKIA_X0 - 1, SNAKE_NOKIA_Y0 - 1, SNAKE_NOKIA_X0 - 1, SNAKE_NOKIA_Y1 + 1, true);
	NOKIA_SetLine(SNAKE_NOKIA_X1 + 1, SNAKE_NOKIA_Y0 - 1, SNAKE_NOKIA_X1 + 1, SNAKE_NOKIA_Y1 + 1, true);
}

static void SNAKE_DrawPixel(uint8_t x, uint8_t y, ColorBitfield hubColor, bool nokiaColor) {
	if (x >= SNAKE_WIDTH) return;
	if (y >= SNAKE_HEIGHT) return;

	if (HUB75_StartDrawing()) {
		HUB75_SetPixelColor(SNAKE_X00 + x * 2, SNAKE_Y0 + y * 2, hubColor);
		HUB75_SetPixelColor(SNAKE_X00 + x * 2 + 1, SNAKE_Y0 + y * 2, hubColor);
		HUB75_SetPixelColor(SNAKE_X00 + x * 2, SNAKE_Y0 + y * 2 + 1, hubColor);
		HUB75_SetPixelColor(SNAKE_X00 + x * 2 + 1, SNAKE_Y0 + y * 2 + 1, hubColor);
		HUB75_SetPixelColor(SNAKE_X10 + x * 2, SNAKE_Y0 + y * 2, hubColor);
		HUB75_SetPixelColor(SNAKE_X10 + x * 2 + 1, SNAKE_Y0 + y * 2, hubColor);
		HUB75_SetPixelColor(SNAKE_X10 + x * 2, SNAKE_Y0 + y * 2 + 1, hubColor);
		HUB75_SetPixelColor(SNAKE_X10 + x * 2 + 1, SNAKE_Y0 + y * 2 + 1, hubColor);
	}

	NOKIA_SetRect(SNAKE_NOKIA_X0 + x * 2, SNAKE_NOKIA_Y0 + y * 2, SNAKE_NOKIA_X0 + x * 2 + 1, SNAKE_NOKIA_Y0 + y * 2 + 1, true, nokiaColor);
}

static bool SNAKE_CalculateDirection(void) {
	if (GAMEPAD_GetHoldButton(UP)) {
		if (snakeDir != SNAKE_DOWN && lastSnakeDir != SNAKE_DOWN) snakeDir = SNAKE_UP;
		return true;
	} else if (GAMEPAD_GetHoldButton(LEFT)) {
		if (snakeDir != SNAKE_RIGHT && lastSnakeDir != SNAKE_RIGHT) snakeDir = SNAKE_LEFT;
		return true;
	} else if (GAMEPAD_GetHoldButton(RIGHT)) {
		if (snakeDir != SNAKE_LEFT && lastSnakeDir != SNAKE_LEFT) snakeDir = SNAKE_RIGHT;
		return true;
	} else if (GAMEPAD_GetHoldButton(DOWN)) {
		if (snakeDir != SNAKE_UP && lastSnakeDir != SNAKE_UP) snakeDir = SNAKE_DOWN;
		return true;
	}
	return false;
}

static float Fract(float x) {
	return x - ((int) x);
}

static float Mix(float a, float b, float t) {
	return a + (b - a) * t;
}

static float Clampf(float value, float min, float max)
{
    if (value < min)
        return min;
    if (value > max)
        return max;
    return value;
}

static ColorBitfield HSVtoRGB(float h, float s, float v) {
	ColorBitfield result = { color: 0x0000 };
	result.bits.r = v * Mix(1.0, Clampf(fabsf(Fract(h + 1.0) * 6.0 - 3.0) - 1.0, 0.0, 1.0), s) * 31;
	result.bits.g = v * Mix(1.0, Clampf(fabsf(Fract(h + 0.6666666) * 6.0 - 3.0) - 1.0, 0.0, 1.0), s) * 31;
	result.bits.b = v * Mix(1.0, Clampf(fabsf(Fract(h + 0.3333333) * 6.0 - 3.0) - 1.0, 0.0, 1.0), s) * 31;

	return result;
}
