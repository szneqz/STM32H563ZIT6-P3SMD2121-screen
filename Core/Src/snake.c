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

static void SNAKE_DrawBorder(void);
static void SNAKE_DrawPixel(uint8_t x, uint8_t y, ColorBitfield hubColor, bool nokiaColor);

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
static uint8_t fruitPos[2];
static uint8_t snakeMode = SNAKE_STATIC;
static uint8_t snakeDelay = 30;

void SNAKE_Init(void) {
	NOKIA_StartDataPrepare();
	NOKIA_Clear();

	SNAKE_DrawBorder();

	ColorBitfield black = { 0x0000 };
	ColorBitfield white = { 0x7FFF };

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
	//TODO: SpawnFruit();
	snakeMode = SNAKE_STATIC;
	snakeDelay = 30;

	for (uint8_t i = 0; i < tailLen; i++) {
		SNAKE_DrawPixel(snakePos[i][0], snakePos[i][1], white, true);
	}

	//snakeGameDelay = maxSnakeGameDelay; //reset snake timer

	NOKIA_StopDataPrepare();
	NOKIA_SendData();
}

void SNAKE_Logic(void) {

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

	HUB75_SetPixelColor(SNAKE_X00 + x * 2, SNAKE_Y0 + y * 2, hubColor);
	HUB75_SetPixelColor(SNAKE_X00 + x * 2 + 1, SNAKE_Y0 + y * 2, hubColor);
	HUB75_SetPixelColor(SNAKE_X00 + x * 2, SNAKE_Y0 + y * 2 + 1, hubColor);
	HUB75_SetPixelColor(SNAKE_X00 + x * 2 + 1, SNAKE_Y0 + y * 2 + 1, hubColor);
	HUB75_SetPixelColor(SNAKE_X10 + x * 2, SNAKE_Y0 + y * 2, hubColor);
	HUB75_SetPixelColor(SNAKE_X10 + x * 2 + 1, SNAKE_Y0 + y * 2, hubColor);
	HUB75_SetPixelColor(SNAKE_X10 + x * 2, SNAKE_Y0 + y * 2 + 1, hubColor);
	HUB75_SetPixelColor(SNAKE_X10 + x * 2 + 1, SNAKE_Y0 + y * 2 + 1, hubColor);

	NOKIA_SetRect(SNAKE_NOKIA_X0 + x * 2, SNAKE_NOKIA_Y0 + y * 2, SNAKE_NOKIA_X0 + x * 2 + 1, SNAKE_NOKIA_Y0 + y * 2 + 1, true, nokiaColor);
}
