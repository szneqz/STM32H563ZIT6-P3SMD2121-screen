/*
 * TETRIS.c
 *
 *  Created on: Jun 1, 2026
 *      Author: szneqz
 */

#include "tetris.h"
#include <string.h>

#define TETRIS_WIDTH 10
#define TETRIS_HEIGHT 16
#define TETRIS_PIXEL_SIZE 160
#define TETRIS_X00 43
#define TETRIS_X01 62
#define TETRIS_X10 65
#define TETRIS_X11 84
#define TETRIS_Y0 0
#define TETRIS_Y1 31
#define TETRIS_NOKIA_X0 1
#define TETRIS_NOKIA_X1 20
#define TETRIS_NOKIA_Y0 0
#define TETRIS_NOKIA_Y1 31

#define TETRIS_NR_FIGURES 7

typedef struct
{
    int8_t x;
    int8_t y;
} BlockPosition;

extern volatile uint32_t randomNumber;

static ColorBitfield black = { 0x0000 };

static const unsigned long maxTetrisGameDelay = 30;
static const unsigned long maxMoveTetrisLeftRightDelay = 200;

static BlockPosition GetFigureBlockPos(uint8_t i, int8_t myFigPosX, int8_t myFigPosY, int8_t myFigRot, int8_t thisFigure);
static void DrawFigure(int8_t lastPosX, int8_t lastPosY, int8_t lastRot);
static void DrawAnyFigure(int8_t myFigPosX, int8_t myFigPosY, int8_t myFigRot, int8_t thisFigure);
static bool CheckFigurePossibility(int8_t myFigPosX, int8_t myFigPosY, int8_t myFigRot);
static void RotateTetrisFigure(int8_t dir);
static void MoveTetrisLeftRight(int8_t dir, uint32_t actualMillis);
static void CheckWholeLines(int8_t minHeight, int8_t maxHeight);
static void TETRIS_DrawBorder(void);
static void TETRIS_DrawPixel(uint8_t x, uint8_t y, ColorBitfield hubColor, bool nokiaColor);

enum TETRIS_MODE {
	TETRIS_DEAD, TETRIS_STATIC, TETRIS_PLAYING
};

static ColorBitfield tetrisPlayfield[TETRIS_PIXEL_SIZE];

static int8_t figurePosX = 0;
static int8_t figurePosY = 0;
static const int8_t figurePosXStart = 3;
static int8_t figureRot = 0;  //4 rotations
//cyan, 	red, 	  green, 	blue, 	  orange, 	magenta,  yellow
static ColorBitfield tetrisColors[TETRIS_NR_FIGURES] = { {0x03ff}, {0x7c00}, {0x03e0}, {0x001f}, {0x7e00}, {0x7c1f}, {0x7fe0} };
static int8_t figures[TETRIS_NR_FIGURES][4][4] = {
		{ { 4, 5, 6, 7 }, { 2, 6, 10, 14 }, { 8, 9, 10, 11 }, { 1, 5, 9, 13 } },  // line
		{ { 0, 1, 5, 6 }, { 2, 5, 6, 9 }, { 4, 5, 9, 10 }, { 1, 4, 5, 8 } },      // Z
		{ { 1, 2, 4, 5 }, { 1, 5, 6, 10 }, { 5, 6, 8, 9 }, { 0, 4, 5, 9 } },      // Z reversed
		{ { 0, 4, 5, 6 }, { 1, 2, 5, 9 }, { 4, 5, 6, 10 }, { 1, 5, 8, 9 } },      // J
		{ { 2, 4, 5, 6 }, { 1, 5, 9, 10 }, { 4, 5, 6, 8 }, { 0, 1, 5, 9 } },      // L
		{ { 1, 4, 5, 6 }, { 1, 5, 6, 9 }, { 4, 5, 6, 9 }, { 1, 4, 5, 9 } },       // |-
		{ { 1, 2, 5, 6 }, { 1, 2, 5, 6 }, { 1, 2, 5, 6 }, { 1, 2, 5, 6 } }        // square
};
static const int8_t wallKicksAmount = 5;
static int8_t regularWallKicksClockwise[4][5][2] = {
		{ { 0, 0 }, { -1, 0 }, { -1, -1 }, { 0, 2 }, { -1, 2 } },  //0>>1
		{ { 0, 0 }, { 1, 0 }, { 1, 1 }, { 0, -2 }, { 1, -2 } },    //1>>2
		{ { 0, 0 }, { 1, 0 }, { 1, -1 }, { 0, 2 }, { 1, 2 } },     //2>>3
		{ { 0, 0 }, { -1, 0 }, { -1, 1 }, { 0, -2 }, { -1, -2 } }  //3>>0
};
static int8_t regularWallKicksCounterClockwise[4][5][2] = {
		{ { 0, 0 }, { 1, 0 }, { 1, -1 }, { 0, 2 }, { 1, 2 } },     //0>>3
		{ { 0, 0 }, { 1, 0 }, { 1, 1 }, { 0, -2 }, { 1, -2 } },    //1>>0
		{ { 0, 0 }, { -1, 0 }, { -1, -1 }, { 0, 2 }, { -1, 2 } },  //2>>1
		{ { 0, 0 }, { -1, 0 }, { -1, 1 }, { 0, -2 }, { -1, -2 } }  //3>>2
};
static int8_t iWallKicksClockwise[4][5][2] = {
		{ { 0, 0 }, { -2, 0 }, { 1, 0 }, { -2, 1 }, { 1, -2 } },  //0>>1
		{ { 0, 0 }, { -1, 0 }, { 2, 0 }, { -1, -2 }, { 2, 1 } },  //1>>2
		{ { 0, 0 }, { 2, 0 }, { -1, 0 }, { 2, -1 }, { -1, 2 } },  //2>>3
		{ { 0, 0 }, { 1, 0 }, { -2, 0 }, { 1, 2 }, { -2, -1 } }   //3>>0
};
static int8_t iWallKicksCounterClockwise[4][5][2] = {
		{ { 0, 0 }, { -1, 0 }, { 2, 0 }, { -1, -2 }, { 2, 1 } },  //0>>3
		{ { 0, 0 }, { 2, 0 }, { -1, 0 }, { 2, -1 }, { -1, 2 } },  //1>>0
		{ { 0, 0 }, { 1, 0 }, { -2, 0 }, { 1, 2 }, { -2, -1 } },  //2>>1
		{ { 0, 0 }, { -2, 0 }, { 1, 0 }, { -2, 1 }, { 1, -2 } }   //3>>2
};
static int8_t nextFigure = 0;
static int8_t actualFigure = 0;
static ColorBitfield actualFigureColor = {0x03ff};
static bool randomizeFigure = false;      //if create new random figure
static int8_t tetrisMode = TETRIS_STATIC;
static const int8_t fastMovementBlockDelay = 1;
static const int8_t movementBlockDelay = 20;
static const int16_t deadDelay = 3000;
static int8_t actualMovementBlockDelay = movementBlockDelay;
static int8_t blockDelay = movementBlockDelay;
static int8_t startBlockDelay = movementBlockDelay;
static uint16_t tetrisScore = 0;

static bool previousNokiaFrameCopied = false;
static bool previousHUB75FrameCopied = false;

void TETRIS_Init(void) {
	NOKIA_StartDataPrepare();
	NOKIA_Clear();

	HUB75_Clear();
	TETRIS_DrawBorder();

	memset(tetrisPlayfield, 0, sizeof(tetrisPlayfield));

	randomizeFigure = true;
	nextFigure = randomNumber % TETRIS_NR_FIGURES;
	tetrisMode = TETRIS_STATIC;
	tetrisScore = 0;

	NOKIA_StopDataPrepare();
	NOKIA_SendData();
}

void TETRIS_Logic(void) {
	static uint32_t lastMillis = 0;
	uint32_t actualMillis = HAL_GetTick();

	previousNokiaFrameCopied = false;
	previousHUB75FrameCopied = false;

	//don't try to catch up if long time passed
	if (actualMillis > (lastMillis + 2000)) lastMillis = actualMillis;

	if (tetrisMode == TETRIS_STATIC) {
		if (GAMEPAD_GetHoldButton(LEFT) || GAMEPAD_GetHoldButton(RIGHT) || GAMEPAD_GetHoldButton(DOWN)) {
			tetrisMode = TETRIS_PLAYING;
		}

		if (GAMEPAD_GetClickButton(A) || GAMEPAD_GetClickButton(B)) {
			GAMEPAD_SetClickReadFlag(A);
			GAMEPAD_SetClickReadFlag(B);
		}

	} else if (tetrisMode == TETRIS_PLAYING) {
		if (GAMEPAD_GetHoldButton(DOWN)) {
			if (blockDelay > fastMovementBlockDelay && startBlockDelay <= 0)
				blockDelay = fastMovementBlockDelay;

			actualMovementBlockDelay = fastMovementBlockDelay;
		} else {
			actualMovementBlockDelay = movementBlockDelay;
		}

		if (GAMEPAD_GetHoldButton(LEFT)) {
			MoveTetrisLeftRight(-1, actualMillis);
		} else if (GAMEPAD_GetHoldButton(RIGHT)) {
			MoveTetrisLeftRight(1, actualMillis);
		}

		if (GAMEPAD_GetClickButton(A)) {
			RotateTetrisFigure(-1);
			GAMEPAD_SetClickReadFlag(A);
		}

		if (GAMEPAD_GetClickButton(B)) {
			RotateTetrisFigure(1);
			GAMEPAD_SetClickReadFlag(B);
		}

		while (actualMillis > lastMillis) {
			lastMillis += maxTetrisGameDelay;

			if (!previousNokiaFrameCopied) {
				NOKIA_StartDataPrepare();
				NOKIA_CopyPreviousFrame();
				previousNokiaFrameCopied = true;
			}

			if (!previousHUB75FrameCopied) {
				if (HUB75_StartDrawing()) {
					HUB75_CopyPreviousFrame();
					previousHUB75FrameCopied = true;
				}
			}

			if (randomizeFigure) {
				actualFigure = nextFigure;
				actualFigureColor = tetrisColors[actualFigure];
				nextFigure = randomNumber % 7;
				//TODO: Show next figure
				//DrawAnyFigure(TETRIS_WIDTH + 3, 2, 0, nextFigure);  //draw another figure
				figurePosX = figurePosXStart;
				figurePosY = 0;
				figureRot = 0;
				randomizeFigure = false;
				blockDelay = movementBlockDelay;
				startBlockDelay = movementBlockDelay;
				DrawFigure(figurePosX, figurePosY, -1);

				if (!CheckFigurePossibility(figurePosX, figurePosY, figureRot)) {  //if spawning figure in another existing figure then it means it is end of the game
					tetrisMode = TETRIS_DEAD;
					lastMillis = actualMillis + deadDelay;
					break;
				}
			}

			if (blockDelay > 0)
				blockDelay--;

			if (startBlockDelay > 0)
				startBlockDelay--;

			if (blockDelay <= 0 && tetrisMode == TETRIS_PLAYING) {
				for (int8_t i = 0; i < 4; i++) {
					BlockPosition figureBlockPos = GetFigureBlockPos(i, -10, -10, -1, -1);
					if (figureBlockPos.y + 1 >= TETRIS_HEIGHT ||
							tetrisPlayfield[(figureBlockPos.y + 1) * TETRIS_WIDTH + figureBlockPos.x].color != 0) {  //check if figure is outside game area or figure is inside another figure when getting down
						randomizeFigure = true;
						break;
					}
				}

				if (!randomizeFigure) {
					figurePosY++;  //down movemnt of figure
					DrawFigure(figurePosX, figurePosY - 1, -1);
					if (startBlockDelay > 0)
						blockDelay = movementBlockDelay;
					else
						blockDelay = actualMovementBlockDelay;
				} else {  //if figure can't move then save info about position in array and check full lines for scoring
					uint8_t minHeight = 0;
					uint8_t maxHeight = 0;

					for (uint8_t i = 0; i < 4; i++) {
						uint8_t figureBlockPos = (figurePosY + (figures[actualFigure][figureRot][i] / 4)) * TETRIS_WIDTH + figurePosX + (figures[actualFigure][figureRot][i] % 4);
						tetrisPlayfield[figureBlockPos] = actualFigureColor;

						if (i == 0)
							maxHeight = figureBlockPos / TETRIS_WIDTH;
						if (i == 3)
							minHeight = figureBlockPos / TETRIS_WIDTH;
					}
					CheckWholeLines(minHeight, maxHeight);
				}
			}

			NOKIA_StopDataPrepare();
			NOKIA_SendData();
		}
	} else if (tetrisMode == TETRIS_DEAD) {
		if (actualMillis > lastMillis) {
			lastMillis = actualMillis - 1;
			if (GAMEPAD_GetHoldButton(LEFT) || GAMEPAD_GetHoldButton(RIGHT) || GAMEPAD_GetHoldButton(DOWN)) {
				TETRIS_Init();
			}
		}
	}
}

static BlockPosition GetFigureBlockPos(uint8_t i, int8_t myFigPosX, int8_t myFigPosY, int8_t myFigRot, int8_t thisFigure) {
	if (myFigPosX == -10) myFigPosX = figurePosX;
	if (myFigPosY == -10) myFigPosY = figurePosY;
	if (myFigRot == -1) myFigRot = figureRot;
	if (thisFigure == -1) thisFigure = actualFigure;
	BlockPosition blockPosition = {
			.x = (myFigPosX + (figures[thisFigure][myFigRot][i] % 4)),
			.y = (myFigPosY + (figures[thisFigure][myFigRot][i] / 4))
	};
	return blockPosition;
}

static void DrawFigure(int8_t lastPosX, int8_t lastPosY, int8_t lastRot) {
	if (lastRot == -1) lastRot = figureRot;

	for (int8_t i = 0; i < 4; i++) {
		BlockPosition figureBlockPos = GetFigureBlockPos(i, lastPosX, lastPosY, lastRot, -1);
		TETRIS_DrawPixel(figureBlockPos.x, figureBlockPos.y, black, false);
	}

	for (int8_t i = 0; i < 4; i++) {
		BlockPosition figureBlockPos = GetFigureBlockPos(i, figurePosX, figurePosY, -1, -1);
		TETRIS_DrawPixel(figureBlockPos.x, figureBlockPos.y, actualFigureColor, true);
	}
}

static void DrawAnyFigure(int8_t myFigPosX, int8_t myFigPosY, int8_t myFigRot, int8_t thisFigure) {
	if (myFigPosX == -10) myFigPosX = figurePosX;
	if (myFigPosY == -10) myFigPosY = figurePosY;
	if (myFigRot == -1) myFigRot = figureRot;
	if (thisFigure == -1) thisFigure = actualFigure;

	for (int8_t i = 0; i < 4; i++) {  //paint black 4 x 4 square
		for (int8_t j = 0; j < 4; j++) {
			TETRIS_DrawPixel(myFigPosX + j, myFigPosY + i, black, false);
		}
	}

	for (int8_t i = 0; i < 4; i++) {
		BlockPosition figureBlockPos = GetFigureBlockPos(i, myFigPosX, myFigPosY, myFigRot, thisFigure);
		TETRIS_DrawPixel(figureBlockPos.x, figureBlockPos.y, tetrisColors[thisFigure], true);
	}
}

static bool CheckFigurePossibility(int8_t myFigPosX, int8_t myFigPosY, int8_t myFigRot) {
	for (int8_t i = 0; i < 4; i++) {
		BlockPosition figureBlockPos = GetFigureBlockPos(i, myFigPosX, myFigPosY, myFigRot, -1);
		if (figureBlockPos.x < 0 || figureBlockPos.y < 0 || figureBlockPos.x >= TETRIS_WIDTH || figureBlockPos.y >= TETRIS_HEIGHT ||
				tetrisPlayfield[figureBlockPos.y * TETRIS_WIDTH + figureBlockPos.x].color != 0) {  //check if figure is outside game area or figure is inside another figure
			return false;
		}
	}
	return true;
}

static void RotateTetrisFigure(int8_t dir)  //1 - clockwise  -1 - counter clockwise
{
	int8_t newRot = figureRot + dir;
	int8_t newPosX = figurePosX;
	int8_t newPosY = figurePosY;
	int8_t lastRot = figureRot;
	int8_t lastPosX = figurePosX;
	int8_t lastPosY = figurePosY;

	if (newRot > 3)
		newRot = 0;
	if (newRot < 0)
		newRot = 3;

	if (!previousNokiaFrameCopied) {
		NOKIA_StartDataPrepare();
		NOKIA_CopyPreviousFrame();
		previousNokiaFrameCopied = true;
	}

	if (!previousHUB75FrameCopied) {
		if (HUB75_StartDrawing()) {
			HUB75_CopyPreviousFrame();
			previousHUB75FrameCopied = true;
		}
	}

	if (dir == 1) {
		if (actualFigure == 0)  //if blocks of figure are in whole line (scoring point)
		{
			for (int8_t i = 0; i < wallKicksAmount; i++) {
				newPosX = figurePosX + iWallKicksClockwise[figureRot][i][0];
				newPosY = figurePosY + iWallKicksClockwise[figureRot][i][1];
				if (CheckFigurePossibility(newPosX, newPosY, newRot)) {
					figureRot = newRot;
					figurePosX = newPosX;
					figurePosY = newPosY;
					DrawFigure(lastPosX, lastPosY, lastRot);
					break;
				}
			}
		} else {
			for (int8_t i = 0; i < wallKicksAmount; i++) {
				newPosX = figurePosX + regularWallKicksClockwise[figureRot][i][0];
				newPosY = figurePosY + regularWallKicksClockwise[figureRot][i][1];
				if (CheckFigurePossibility(newPosX, newPosY, newRot)) {
					figureRot = newRot;
					figurePosX = newPosX;
					figurePosY = newPosY;
					DrawFigure(lastPosX, lastPosY, lastRot);
					break;
				}
			}
		}
	} else {
		if (actualFigure == 0)  //if blocks of figure are in whole line (scoring point)
		{
			for (int8_t i = 0; i < wallKicksAmount; i++) {
				newPosX = figurePosX + iWallKicksCounterClockwise[figureRot][i][0];
				newPosY = figurePosY + iWallKicksCounterClockwise[figureRot][i][1];
				if (CheckFigurePossibility(newPosX, newPosY, newRot)) {
					figureRot = newRot;
					figurePosX = newPosX;
					figurePosY = newPosY;
					DrawFigure(lastPosX, lastPosY, lastRot);
					break;
				}
			}
		} else {
			for (int8_t i = 0; i < wallKicksAmount; i++) {
				newPosX = figurePosX + regularWallKicksCounterClockwise[figureRot][i][0];
				newPosY = figurePosY + regularWallKicksCounterClockwise[figureRot][i][1];
				if (CheckFigurePossibility(newPosX, newPosY, newRot)) {
					figureRot = newRot;
					figurePosX = newPosX;
					figurePosY = newPosY;
					DrawFigure(lastPosX, lastPosY, lastRot);
					break;
				}
			}
		}
	}
}

static void MoveTetrisLeftRight(int8_t dir, uint32_t actualMillis) {
	static uint32_t lastMillis = 0;

	//don't try to catch up if long time passed
	if (actualMillis > (lastMillis + 2000)) lastMillis = actualMillis - 1;

	while (actualMillis > lastMillis) {
		lastMillis += maxMoveTetrisLeftRightDelay;

		if (tetrisMode == TETRIS_PLAYING) {
			bool canMove = true;

			for (int8_t i = 0; i < 4; i++) {
				BlockPosition figureBlockPos = GetFigureBlockPos(i, -10, -10, -1, -1);
				if ((figureBlockPos.x + dir) < 0 || (figureBlockPos.x + dir) >= TETRIS_WIDTH ||
						tetrisPlayfield[figureBlockPos.y * TETRIS_WIDTH + figureBlockPos.x + dir].color != 0) {  //check if figure is outside game area or figure is inside another figure
					canMove = false;
					break;
				}
			}

			if (canMove) {
				figurePosX += dir;
				if (!previousNokiaFrameCopied) {
					NOKIA_StartDataPrepare();
					NOKIA_CopyPreviousFrame();
					previousNokiaFrameCopied = true;
				}

				if (!previousHUB75FrameCopied) {
					if (HUB75_StartDrawing()) {
						HUB75_CopyPreviousFrame();
						previousHUB75FrameCopied = true;
					}
				}

				DrawFigure(figurePosX - dir, figurePosY, -1);
				}
			}
		}
	}
}

static void CheckWholeLines(int8_t minHeight, int8_t maxHeight) {
	uint8_t iterations = minHeight - maxHeight + 1;
	uint8_t actHeight = minHeight;

	for (uint8_t i = 0; i < iterations; i++) {
		bool scorePoints = true;
		for (uint8_t j = 0; j < TETRIS_WIDTH; j++) {
			if (tetrisPlayfield[actHeight * TETRIS_WIDTH + j].color == 0) {
				scorePoints = false;
				actHeight--;
				break;
			}
		}

		if (scorePoints) {
			for (int8_t k = actHeight - 1; k >= 0; k--) {
				for (uint8_t l = 0; l < TETRIS_WIDTH; l++) {
					tetrisPlayfield[(k + 1) * TETRIS_WIDTH + l] = tetrisPlayfield[k * TETRIS_WIDTH + l];
					TETRIS_DrawPixel(l, (k + 1), tetrisPlayfield[k * TETRIS_WIDTH + l], tetrisPlayfield[k * TETRIS_WIDTH + l].color > 0);
				}
			}
			//TODO: Draw Tetris score here

			tetrisScore++;
		}
	}
}

static void TETRIS_DrawBorder(void) {
	//HUB75 SCREEN
	if (HUB75_StartDrawing()) {
		ColorBitfield white = { .bits.r = 31, .bits.g = 31, .bits.b = 31 };
		uint8_t x0 = TETRIS_X00 - 1;
		uint8_t x1 = TETRIS_X01 + 1;
		uint8_t y0 = TETRIS_Y0;
		uint8_t y1 = TETRIS_Y1;

		for (uint8_t y = y0; y <= y1; y++) {
			HUB75_SetPixelColor(x0, y, white);
			HUB75_SetPixelColor(x1, y, white);
		}

		x0 = TETRIS_X10 - 1;
		x1 = TETRIS_X11 + 1;

		for (uint8_t y = y0; y <= y1; y++) {
			HUB75_SetPixelColor(x0, y, white);
			HUB75_SetPixelColor(x1, y, white);
		}
	}

	//NOKIA SCREEN
	NOKIA_SetLine(TETRIS_NOKIA_X0 - 1, TETRIS_NOKIA_Y1 + 1, TETRIS_NOKIA_X1 + 1, TETRIS_NOKIA_Y1 + 1, true);
	NOKIA_SetLine(TETRIS_NOKIA_X0 - 1, TETRIS_NOKIA_Y0, TETRIS_NOKIA_X0 - 1, TETRIS_NOKIA_Y1 + 1, true);
	NOKIA_SetLine(TETRIS_NOKIA_X1 + 1, TETRIS_NOKIA_Y0, TETRIS_NOKIA_X1 + 1, TETRIS_NOKIA_Y1 + 1, true);
}

static void TETRIS_DrawPixel(uint8_t x, uint8_t y, ColorBitfield hubColor, bool nokiaColor) {
	if (x >= TETRIS_WIDTH) return;
	if (y >= TETRIS_HEIGHT) return;

	if (HUB75_StartDrawing()) {
		HUB75_SetPixelColor(TETRIS_X00 + x * 2, TETRIS_Y0 + y * 2, hubColor);
		HUB75_SetPixelColor(TETRIS_X00 + x * 2 + 1, TETRIS_Y0 + y * 2, hubColor);
		HUB75_SetPixelColor(TETRIS_X00 + x * 2, TETRIS_Y0 + y * 2 + 1, hubColor);
		HUB75_SetPixelColor(TETRIS_X00 + x * 2 + 1, TETRIS_Y0 + y * 2 + 1, hubColor);
		HUB75_SetPixelColor(TETRIS_X10 + x * 2, TETRIS_Y0 + y * 2, hubColor);
		HUB75_SetPixelColor(TETRIS_X10 + x * 2 + 1, TETRIS_Y0 + y * 2, hubColor);
		HUB75_SetPixelColor(TETRIS_X10 + x * 2, TETRIS_Y0 + y * 2 + 1, hubColor);
		HUB75_SetPixelColor(TETRIS_X10 + x * 2 + 1, TETRIS_Y0 + y * 2 + 1, hubColor);
	}

	NOKIA_SetPixel(TETRIS_NOKIA_X0 + x * 2, TETRIS_NOKIA_Y0 + y * 2, nokiaColor);
	NOKIA_SetPixel(TETRIS_NOKIA_X0 + x * 2, TETRIS_NOKIA_Y0 + y * 2 + 1, nokiaColor);
	NOKIA_SetPixel(TETRIS_NOKIA_X0 + x * 2 + 1, TETRIS_NOKIA_Y0 + y * 2, nokiaColor);
	NOKIA_SetPixel(TETRIS_NOKIA_X0 + x * 2 + 1, TETRIS_NOKIA_Y0 + y * 2 + 1, nokiaColor);
}
