/*
 * main_logic.c
 *
 *  Created on: Apr 8, 2026
 *      Author: szneqz
 */
#include "main_logic.h"
#include "screen_images.h"

#define NO_COLOR 7
#define RAINBOW 8

static void DrawMainMenu(void);
static void LogicMainMenu(void);
static void DrawEmotesMenu(void);
static void LogicEmotesMenu(void);
static void ApplyEmoteColor(void);
static void AssignRealEmoteColor(void);
static void DrawGamesMenu(void);
static void LogicGamesMenu(void);
static ColorBitfield RainbowColorChange(void);
static void DrawEmblemMenu(void);
static void LogicEmblemMenu(void);
static void ApplyEmblemColor(void);
static void AssignRealEmblemColor(void);
static void EmoteFrameChange(void);
static void DrawEmblem(ColorBitfield color);
static void BacklightLogic(void);
static void GlitchLogic(void);

static bool isNokiaUpdated = false;
											  //red,     green,    blue,     cyan,     magenta,  yellow,   white,    nocolor
static const ColorBitfield possibleColors[] = {{0x7c00}, {0x03e0}, {0x001f}, {0x03ff}, {0x7c1f}, {0x7fe0}, {0x7fff}, {0x0000}};
static const char colorNames[] = {'R', 'G', 'B', 'C', 'M', 'Y', 'W', 'N', '$'};	// N - no color, $ - rainbow
static const uint32_t maxMillisRainbowStep = 25;

// Main Menu
enum MENU_TYPE {
	MAIN_MENU, EMOTES_MENU, GAMES_MENU, EMBLEM_MENU, MENU_TYPE_COUNT
};
enum MAIN_MENU_SELECTIONS {
	EMOTES, GAMES, EMBLEM, BACKLIGHT, GLITCH, MAIN_MENU_SELECTIONS_COUNT
};
static uint8_t menuType = MAIN_MENU;
static uint8_t mainMenuSelected = EMOTES;

// Emotes submenu
static char *emotesNames[] = {" Pro_STD   ", " Pro_Happy ", " Pro_Sad   ", " Pro_^^    ", " Pro_Dizzy ", " Pro_Arouse", " Pro_Love  ", " Pro_Flat  ",
		" Pro_Shock ", "Pro_Dead   ",
		" Yes       ", " No        ", " Warning   ", " No Signal ", " Low Batter", " Charging  ", " Full Batt ", " Test      "};
static bool emotesDefaultNoColor[] = {false, false, false, false, false, false, false, false,
									false, false,
									true, true, true, true, true, true, true, true};
static uint8_t emotesNamesSize = 18;
static uint8_t markedEmote = 0;
static uint8_t selectedEmote = 0;
static uint8_t emotesScrollOffset = 0;
enum EMOTES_SUB_MENU {
	EMOTES_EMOTE, EMOTES_COLOR, EMOTES_R, EMOTES_G, EMOTES_B
};
static uint8_t emotesSubMenu = EMOTES_EMOTE;
static uint8_t selectedEmoteColor = 2;	//default blue
static bool isEmoteRainbowMode = false;
static uint8_t pickedEmoteRed = 0b00000;
static uint8_t pickedEmoteGreen = 0b00000;
static uint8_t pickedEmoteBlue = 0b11111;

// Games submenu
enum GAMES_MENU_SELECTIONS {
	SNAKE, TETRIS, GAMES_MENU_SELECTIONS_COUNT
};
static bool isInGame = false;

// Emblem submenu
enum EMBLEM_SUB_MENU {
	EMBLEM_COLOR, EMBLEM_R, EMBLEM_G, EMBLEM_B
};
static uint8_t emblemSubMenu = EMBLEM_COLOR;
static uint8_t selectedEmblemColor = 2;	//default blue
static bool isEmblemRainbowMode = false;
static uint8_t pickedEmblemRed = 0b00000;
static uint8_t pickedEmblemGreen = 0b00000;
static uint8_t pickedEmblemBlue = 0b11111;

// Backlight
static bool isBacklight = true;

// Glitch
static bool isGlitch = false;

static ColorBitfield (*protogen_emotes[18])[32][128] = {
		protogen_neutral, protogen_happy, protogen_sad, protogen_dashdash, protogen_dizzy, protogen_aroused, protogen_love, protogen_flat,
		protogen_shocked, protogen_dead,
		protogen_yes, protogen_no, protogen_warning, protogen_nosignal, protogen_low_battery, protogen_charging, protogen_full_battery, protogen_test
};

static uint8_t nrFrames[18] = {4, 1, 2, 1, 4, 2, 2, 1,
							  1, 1,
							  1, 1, 1, 1, 2, 5, 1, 1};
static uint16_t lenFrames[18][5] = {
		{5000, 250, 250, 250, 1},
		{1, 1, 1, 1, 1},
		{1000, 1000, 1, 1, 1},
		{1, 1, 1, 1, 1},
		{250, 250, 250, 250, 1},
		{1000, 1000, 1, 1, 1},
		{1000, 1000, 1, 1, 1},
		{1, 1, 1, 1, 1},
		{1, 1, 1, 1, 1},
		{1, 1, 1, 1, 1},
		{1, 1, 1, 1, 1},
		{1, 1, 1, 1, 1},
		{1, 1, 1, 1, 1},
		{1, 1, 1, 1, 1},
		{1000, 1000, 1, 1, 1},
		{500, 500, 500, 500, 500},
		{1, 1, 1, 1, 1},
		{1, 1, 1, 1, 1}
};
static uint8_t emoteFrameNr = 0;

void LogicInit(void) {
	AssignRealEmoteColor();

	if (isBacklight) HAL_GPIO_WritePin(NOKIA_LED_GPIO_Port, NOKIA_LED_Pin, GPIO_PIN_SET);
	else HAL_GPIO_WritePin(NOKIA_LED_GPIO_Port, NOKIA_LED_Pin, GPIO_PIN_RESET);
}

void LogicLoop(void) {
	GAMEPAD_CalculateClick();

	if (menuType == EMOTES_MENU || (menuType == GAMES_MENU && !isInGame) || menuType == EMBLEM_MENU) {
		if (GAMEPAD_GetClickButton(B)) {
			menuType = MAIN_MENU;

			GAMEPAD_SetClickReadFlag(B);
			isNokiaUpdated = false;
		}
	}

	if (menuType == MAIN_MENU) LogicMainMenu();
	else if (menuType == EMOTES_MENU) LogicEmotesMenu();
	else if (menuType == GAMES_MENU) LogicGamesMenu();
	else if (menuType == EMBLEM_MENU) LogicEmblemMenu();

	if (isEmoteRainbowMode || isEmblemRainbowMode) {
		static ColorBitfield lastRainbowColor = {0x0000};
		ColorBitfield rainbowColor = RainbowColorChange();
		if (rainbowColor.color != lastRainbowColor.color) {
			lastRainbowColor.color = rainbowColor.color;

			if (isEmoteRainbowMode) {
				pickedEmoteRed = rainbowColor.bits.r;
				pickedEmoteGreen = rainbowColor.bits.g;
				pickedEmoteBlue = rainbowColor.bits.b;
				AssignRealEmoteColor();
			}

			if (isEmblemRainbowMode) {
				pickedEmblemRed = rainbowColor.bits.r;
				pickedEmblemGreen = rainbowColor.bits.g;
				pickedEmblemBlue = rainbowColor.bits.b;
				AssignRealEmblemColor();
			}

			isNokiaUpdated = false;
		}
	}

	//every frame try to calculate animation of emote
	if (!isInGame) {
		EmoteFrameChange();
	}

	HUB75_StopDrawing();
}

static void DrawMainMenu(void) {
	NOKIA_StartDataPrepare();
	NOKIA_Clear();
	NOKIA_SetStr(" Emotki       ", 0, 0, mainMenuSelected != EMOTES, false, false);
	NOKIA_SetStr(" Gry          ", 0, 8, mainMenuSelected != GAMES, false, false);
	NOKIA_SetStr(" Emblemat     ", 0, 16, mainMenuSelected != EMBLEM, false, false);
	NOKIA_SetStr(" Podswietlenie", 0, 24, mainMenuSelected != BACKLIGHT, false, false);
	NOKIA_SetStr(" Glitch       ", 0, 32, mainMenuSelected != GLITCH, false, false);
	NOKIA_StopDataPrepare();
	NOKIA_SendData();
	isNokiaUpdated = true;
}

static void LogicMainMenu(void) {
	if (!isNokiaUpdated) {
		DrawMainMenu();
	}

	if (GAMEPAD_GetClickButton(UP)) {
		mainMenuSelected = (mainMenuSelected - 1 + MAIN_MENU_SELECTIONS_COUNT) % MAIN_MENU_SELECTIONS_COUNT;
		GAMEPAD_SetClickReadFlag(UP);
		isNokiaUpdated = false;
	}
	else if (GAMEPAD_GetClickButton(DOWN)) {
		mainMenuSelected = (mainMenuSelected + 1) % MAIN_MENU_SELECTIONS_COUNT;
		GAMEPAD_SetClickReadFlag(DOWN);
		isNokiaUpdated = false;
	}
	else if (GAMEPAD_GetClickButton(A)) {
		if (mainMenuSelected == EMOTES) menuType = EMOTES_MENU;
		else if (mainMenuSelected == GAMES) menuType = GAMES_MENU;
		else if (mainMenuSelected == EMBLEM) menuType = EMBLEM_MENU;
		else if (mainMenuSelected == BACKLIGHT) BacklightLogic();
		else if (mainMenuSelected == GLITCH) GlitchLogic();

		GAMEPAD_SetClickReadFlag(A);
		isNokiaUpdated = false;
	}
}

static void DrawEmotesMenu(void) {
	NOKIA_StartDataPrepare();
	NOKIA_Clear();
	for (int i = 0; i < emotesNamesSize; i++) {
		int8_t offsetedi = (int8_t)i - (int8_t)emotesScrollOffset;
		if (offsetedi >= 0 && offsetedi < 6) {
			NOKIA_SetStr(emotesNames[i], 0, offsetedi * 8, !(i == markedEmote && emotesSubMenu == EMOTES_EMOTE), false, false);
			if (i == selectedEmote) NOKIA_SetStr(">", 0, offsetedi * 8, !(i == markedEmote && emotesSubMenu == EMOTES_EMOTE), false, false);
		}
	}

	NOKIA_SetChar(colorNames[selectedEmoteColor], 12 * 6, 8, emotesSubMenu != EMOTES_COLOR, false);
	char rStr[3] = "0";
	char gStr[3] = "0";
	char bStr[3] = "0";

	sprintf(rStr, "%d", pickedEmoteRed);
	sprintf(gStr, "%d", pickedEmoteGreen);
	sprintf(bStr, "%d", pickedEmoteBlue);

	NOKIA_SetStr(rStr, 12 * 6, 3 * 8, emotesSubMenu != EMOTES_R, false, false);
	NOKIA_SetStr(gStr, 12 * 6, 4 * 8, emotesSubMenu != EMOTES_G, false, false);
	NOKIA_SetStr(bStr, 12 * 6, 5 * 8, emotesSubMenu != EMOTES_B, false, false);

	NOKIA_StopDataPrepare();
	NOKIA_SendData();
	isNokiaUpdated = true;
}

static void LogicEmotesMenu(void) {
	if (!isNokiaUpdated) {
		DrawEmotesMenu();
	}

	if (GAMEPAD_GetClickButton(UP)) {
		if (emotesSubMenu == EMOTES_EMOTE) {
			if (markedEmote == 0) markedEmote = emotesNamesSize - 1;
			else markedEmote--;

			if (markedEmote < emotesScrollOffset) {
				emotesScrollOffset -= emotesScrollOffset - markedEmote;
			}
			if (markedEmote >= (emotesScrollOffset + 5)) {
				emotesScrollOffset += markedEmote - (emotesScrollOffset + 4);
			}
		} else if (emotesSubMenu == EMOTES_COLOR) {
			if (selectedEmoteColor == 0) selectedEmoteColor = RAINBOW;
			else selectedEmoteColor--;
		} else if (!isEmoteRainbowMode) {
			if (emotesSubMenu == EMOTES_R) {
				if (pickedEmoteRed >= 31) pickedEmoteRed = 0;
				else pickedEmoteRed++;
				AssignRealEmoteColor();
			} else if (emotesSubMenu == EMOTES_G) {
				if (pickedEmoteGreen >= 31) pickedEmoteGreen = 0;
				else pickedEmoteGreen++;
				AssignRealEmoteColor();
			} else if (emotesSubMenu == EMOTES_B) {
				if (pickedEmoteBlue >= 31) pickedEmoteBlue = 0;
				else pickedEmoteBlue++;
				AssignRealEmoteColor();
			}
		}
		GAMEPAD_SetClickReadFlag(UP);
		isNokiaUpdated = false;
	}
	else if (GAMEPAD_GetClickButton(DOWN)) {
		if (emotesSubMenu == EMOTES_EMOTE) {
			if (markedEmote == emotesNamesSize - 1) markedEmote = 0;
			else markedEmote++;

			if (markedEmote < emotesScrollOffset) {
				emotesScrollOffset -= emotesScrollOffset - markedEmote;
			}
			if (markedEmote >= (emotesScrollOffset + 5)) {
				emotesScrollOffset += markedEmote - (emotesScrollOffset + 4);
			}
		} else if (emotesSubMenu == EMOTES_COLOR) {
			if (selectedEmoteColor >= RAINBOW) selectedEmoteColor = 0;
			else selectedEmoteColor++;
		} else if (!isEmoteRainbowMode) {
			if (emotesSubMenu == EMOTES_R) {
				if (pickedEmoteRed == 0) pickedEmoteRed = 31;
				else pickedEmoteRed--;
				AssignRealEmoteColor();
			} else if (emotesSubMenu == EMOTES_G) {
				if (pickedEmoteGreen == 0) pickedEmoteGreen = 31;
				else pickedEmoteGreen--;
				AssignRealEmoteColor();
			} else if (emotesSubMenu == EMOTES_B) {
				if (pickedEmoteBlue == 0) pickedEmoteBlue = 31;
				else pickedEmoteBlue--;
				AssignRealEmoteColor();
			}
		}
		GAMEPAD_SetClickReadFlag(DOWN);
		isNokiaUpdated = false;
	}
	else if (GAMEPAD_GetClickButton(RIGHT)) {
		if (emotesSubMenu >= EMOTES_B) {
			emotesSubMenu = EMOTES_EMOTE;
		} else {
			emotesSubMenu++;
		}
		GAMEPAD_SetClickReadFlag(RIGHT);
		isNokiaUpdated = false;
	}
	else if (GAMEPAD_GetClickButton(LEFT)) {
		if (emotesSubMenu == EMOTES_EMOTE) {
			emotesSubMenu = EMOTES_B;
		} else {
			emotesSubMenu--;
		}
		GAMEPAD_SetClickReadFlag(LEFT);
		isNokiaUpdated = false;
	}
	else if (GAMEPAD_GetClickButton(A)) {
		if (emotesSubMenu == EMOTES_EMOTE) {
			selectedEmote = markedEmote;
			emoteFrameNr = 0;
			if (emotesDefaultNoColor[selectedEmote]) {
				selectedEmoteColor = NO_COLOR;
			}
			ApplyEmoteColor();
		} else if (emotesSubMenu == EMOTES_COLOR) {
			ApplyEmoteColor();
		}
		GAMEPAD_SetClickReadFlag(A);
		isNokiaUpdated = false;
	}
}

static void ApplyEmoteColor(void) {
	if (selectedEmoteColor != RAINBOW) {
		//everything except rainbow
		isEmoteRainbowMode = false;
		pickedEmoteRed = possibleColors[selectedEmoteColor].bits.r;
		pickedEmoteGreen = possibleColors[selectedEmoteColor].bits.g;
		pickedEmoteBlue = possibleColors[selectedEmoteColor].bits.b;
		AssignRealEmoteColor();
	} else if (selectedEmoteColor == RAINBOW) {
		isEmoteRainbowMode = true;
	}
}

static void AssignRealEmoteColor(void) {
	if(HUB75_StartDrawing()) {
		HUB75_CopyFrame((ColorBitfield*)protogen_emotes[selectedEmote][emoteFrameNr], HUB75_PANEL_HEIGHT * HUB75_PANEL_WIDTH);
		ColorBitfield pickedEmoteColor = { .bits.r = pickedEmoteRed, .bits.g = pickedEmoteGreen, .bits.b = pickedEmoteBlue };
		if (pickedEmoteColor.color != possibleColors[NO_COLOR].color)	//if no color then don't change color
			HUB75_ChangeDrawFrameColor(pickedEmoteColor);
		ColorBitfield emblemColor = { .bits.r = pickedEmblemRed, .bits.g = pickedEmblemGreen, .bits.b = pickedEmblemBlue};
		if (emblemColor.color != possibleColors[NO_COLOR].color)	//if no color then don't override color
			DrawEmblem(emblemColor);
	}
}

static void DrawGamesMenu(void) {

}

static void LogicGamesMenu(void) {

}

static void DrawEmblemMenu(void) {
	NOKIA_StartDataPrepare();
	NOKIA_Clear();

	NOKIA_SetStr("Color:", 6, 8, true, false, false);
	NOKIA_SetChar(colorNames[selectedEmblemColor], 7 * 6, 8, emblemSubMenu != EMBLEM_COLOR, false);
	char rStr[3] = "0";
	char gStr[3] = "0";
	char bStr[3] = "0";

	sprintf(rStr, "%d", pickedEmblemRed);
	sprintf(gStr, "%d", pickedEmblemGreen);
	sprintf(bStr, "%d", pickedEmblemBlue);

	NOKIA_SetStr("Red:", 6, 3 * 8, true, false, false);
	NOKIA_SetStr("Green:", 6, 4 * 8, true, false, false);
	NOKIA_SetStr("Blue:", 6, 5 * 8, true, false, false);
	NOKIA_SetStr(rStr, 7 * 6, 3 * 8, emblemSubMenu != EMBLEM_R, false, false);
	NOKIA_SetStr(gStr, 7 * 6, 4 * 8, emblemSubMenu != EMBLEM_G, false, false);
	NOKIA_SetStr(bStr, 7 * 6, 5 * 8, emblemSubMenu != EMBLEM_B, false, false);

	NOKIA_StopDataPrepare();
	NOKIA_SendData();
	isNokiaUpdated = true;
}

static void LogicEmblemMenu(void) {
	if (!isNokiaUpdated) {
			DrawEmblemMenu();
		}

		if (GAMEPAD_GetClickButton(UP)) {
			if (emblemSubMenu == EMBLEM_COLOR) {
				if (selectedEmblemColor == 0) selectedEmblemColor = RAINBOW;
				else selectedEmblemColor--;
			} else if (!isEmblemRainbowMode) {
				if (emblemSubMenu == EMBLEM_R) {
					if (pickedEmblemRed >= 31) pickedEmblemRed = 0;
					else pickedEmblemRed++;
					AssignRealEmblemColor();
				} else if (emblemSubMenu == EMBLEM_G) {
					if (pickedEmblemGreen >= 31) pickedEmblemGreen = 0;
					else pickedEmblemGreen++;
					AssignRealEmblemColor();
				} else if (emblemSubMenu == EMBLEM_B) {
					if (pickedEmblemBlue >= 31) pickedEmblemBlue = 0;
					else pickedEmblemBlue++;
					AssignRealEmblemColor();
				}
			}
			GAMEPAD_SetClickReadFlag(UP);
			isNokiaUpdated = false;
		}
		else if (GAMEPAD_GetClickButton(DOWN)) {
			if (emblemSubMenu == EMBLEM_COLOR) {
				if (selectedEmblemColor >= RAINBOW) selectedEmblemColor = 0;
				else selectedEmblemColor++;
			} else if (!isEmblemRainbowMode) {
				if (emblemSubMenu == EMBLEM_R) {
					if (pickedEmblemRed == 0) pickedEmblemRed = 31;
					else pickedEmblemRed--;
					AssignRealEmblemColor();
				} else if (emblemSubMenu == EMBLEM_G) {
					if (pickedEmblemGreen == 0) pickedEmblemGreen = 31;
					else pickedEmblemGreen--;
					AssignRealEmblemColor();
				} else if (emblemSubMenu == EMBLEM_B) {
					if (pickedEmblemBlue == 0) pickedEmblemBlue = 31;
					else pickedEmblemBlue--;
					AssignRealEmblemColor();
				}
			}
			GAMEPAD_SetClickReadFlag(DOWN);
			isNokiaUpdated = false;
		}
		else if (GAMEPAD_GetClickButton(RIGHT)) {
			if (emblemSubMenu >= EMBLEM_B) {
				emblemSubMenu = EMBLEM_COLOR;
			} else {
				emblemSubMenu++;
			}
			GAMEPAD_SetClickReadFlag(RIGHT);
			isNokiaUpdated = false;
		}
		else if (GAMEPAD_GetClickButton(LEFT)) {
			if (emblemSubMenu == EMBLEM_COLOR) {
				emblemSubMenu = EMBLEM_B;
			} else {
				emblemSubMenu--;
			}
			GAMEPAD_SetClickReadFlag(LEFT);
			isNokiaUpdated = false;
		}
		else if (GAMEPAD_GetClickButton(A)) {
			if (emblemSubMenu == EMBLEM_COLOR) {
				ApplyEmblemColor();
			}
			GAMEPAD_SetClickReadFlag(A);
			isNokiaUpdated = false;
		}
}

static void ApplyEmblemColor(void) {
	if (selectedEmblemColor != RAINBOW) {
		//everything except rainbow
		isEmblemRainbowMode = false;
		pickedEmblemRed = possibleColors[selectedEmblemColor].bits.r;
		pickedEmblemGreen = possibleColors[selectedEmblemColor].bits.g;
		pickedEmblemBlue = possibleColors[selectedEmblemColor].bits.b;
		AssignRealEmblemColor();
	} else if (selectedEmblemColor == RAINBOW) {
		isEmblemRainbowMode = true;
	}
}

static void AssignRealEmblemColor(void) {
	if(HUB75_StartDrawing()) {
		HUB75_CopyPreviousFrame();
		ColorBitfield emblemColor = { .bits.r = pickedEmblemRed, .bits.g = pickedEmblemGreen, .bits.b = pickedEmblemBlue};
		if (emblemColor.color != possibleColors[NO_COLOR].color)	//if no color then don't override color
			DrawEmblem(emblemColor);
	}
}

static ColorBitfield RainbowColorChange(void) {
	static uint8_t hueDirection = 4;	//0 - G grow, 1 - R decline, 2 - B grow, 3 - G decline, 4 - R grow, 5 - B decline
	static ColorBitfield rgb = {0x001f};
	static uint32_t lastMillis = 0;
	uint32_t actualMillis = HAL_GetTick();

	while (actualMillis > lastMillis) {
		//don't try to catch up if long time passed
		if (actualMillis > (lastMillis + 1000)) lastMillis = actualMillis;

		lastMillis += maxMillisRainbowStep;

		switch(hueDirection) {
		case 0:
			rgb.bits.g += 5;
			if (rgb.bits.g >= 31) {
				rgb.bits.g = 31;
				hueDirection = 1;
			}
			break;
		case 1:
			rgb.bits.r -= 5;
			if (rgb.bits.r == 0 || rgb.bits.r >= 32) {
				rgb.bits.r = 0;
				hueDirection = 2;
			}
			break;
		case 2:
			rgb.bits.b += 5;
			if (rgb.bits.b >= 31) {
				rgb.bits.b = 31;
				hueDirection = 3;
			}
			break;
		case 3:
			rgb.bits.g -= 5;
			if (rgb.bits.g == 0 || rgb.bits.g >= 32) {
				rgb.bits.g = 0;
				hueDirection = 4;
			}
			break;
		case 4:
			rgb.bits.r += 5;
			if (rgb.bits.r >= 31) {
				rgb.bits.r = 31;
				hueDirection = 5;
			}
			break;
		case 5:
			rgb.bits.b -= 5;
			if (rgb.bits.b == 0 || rgb.bits.b >= 32) {
				rgb.bits.b = 0;
				hueDirection = 0;
			}
			break;
		}
	}

	return rgb;
}

static void EmoteFrameChange(void) {
	static uint32_t lastMillis = 0;

	if (nrFrames[selectedEmote] > 1) {
		uint32_t actualMillis = HAL_GetTick();
		while (actualMillis > lastMillis) {
			//don't try to catch up if long time passed
			if (actualMillis > (lastMillis + 10000)) lastMillis = actualMillis;

			emoteFrameNr++;
			if (emoteFrameNr >= nrFrames[selectedEmote]) {
				emoteFrameNr = 0;
			}

			lastMillis += lenFrames[selectedEmote][emoteFrameNr];

			AssignRealEmoteColor();
		}
	}
}

//ALWAYS put it between StartDrawing - StopDrawing
static void DrawEmblem(ColorBitfield color) {
	if(HUB75_StartDrawing()) {
		for (int i = 31; i >= 19; i--) {
			for (int j = 0; j <= 4; j++) {
				HUB75_SetPixelColor(j, i, color);
				HUB75_SetPixelColor(127 - j , i, color);
			}
		}

		for (int i = 31; i >= 20; i--) {
			for (int j = 5; j <= 6; j++) {
				HUB75_SetPixelColor(j, i, color);
				HUB75_SetPixelColor(127 - j , i, color);
			}
		}

		for (int i = 31; i >= 21; i--) {
			for (int j = 7; j <= 8; j++) {
				HUB75_SetPixelColor(j, i, color);
				HUB75_SetPixelColor(127 - j , i, color);
			}
		}

		for (int i = 31; i >= 22; i--) {
			for (int j = 9; j <= 9; j++) {
				HUB75_SetPixelColor(j, i, color);
				HUB75_SetPixelColor(127 - j , i, color);
			}
		}

		for (int i = 31; i >= 23; i--) {
			for (int j = 10; j <= 10; j++) {
				HUB75_SetPixelColor(j, i, color);
				HUB75_SetPixelColor(127 - j , i, color);
			}
		}

		for (int i = 31; i >= 24; i--) {
			for (int j = 11; j <= 11; j++) {
				HUB75_SetPixelColor(j, i, color);
				HUB75_SetPixelColor(127 - j , i, color);
			}
		}

		for (int i = 31; i >= 26; i--) {
			for (int j = 12; j <= 12; j++) {
				HUB75_SetPixelColor(j, i, color);
				HUB75_SetPixelColor(127 - j , i, color);
			}
		}

		for (int i = 31; i >= 28; i--) {
			for (int j = 13; j <= 13; j++) {
				HUB75_SetPixelColor(j, i, color);
				HUB75_SetPixelColor(127 - j , i, color);
			}
		}
	}
}

static void BacklightLogic(void) {
	if (isBacklight) {
		isBacklight = false;
		HAL_GPIO_WritePin(NOKIA_LED_GPIO_Port, NOKIA_LED_Pin, GPIO_PIN_RESET);
	} else {
		isBacklight = true;
		HAL_GPIO_WritePin(NOKIA_LED_GPIO_Port, NOKIA_LED_Pin, GPIO_PIN_SET);
	}
}

static void GlitchLogic(void) {
	if (isGlitch) {
		isGlitch = false;
	} else {
		isGlitch = true;
	}
}
