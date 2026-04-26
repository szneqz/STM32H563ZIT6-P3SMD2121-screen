#ifndef INC_GAMEPAD_H_
#define INC_GAMEPAD_H_

#include "main.h"
#include <stdbool.h>

#define GAMEPAD_UP_PORT   		GAMEPAD_UP_GPIO_Port
#define GAMEPAD_UP_PIN      	GAMEPAD_UP_Pin

#define GAMEPAD_DOWN_PORT   	GAMEPAD_DOWN_GPIO_Port
#define GAMEPAD_DOWN_PIN    	GAMEPAD_DOWN_Pin

#define GAMEPAD_LEFT_PORT   	GAMEPAD_LEFT_GPIO_Port
#define GAMEPAD_LEFT_PIN    	GAMEPAD_LEFT_Pin

#define GAMEPAD_RIGHT_PORT  	GAMEPAD_RIGHT_GPIO_Port
#define GAMEPAD_RIGHT_PIN   	GAMEPAD_RIGHT_Pin

#define GAMEPAD_A_PORT      	GAMEPAD_A_GPIO_Port
#define GAMEPAD_A_PIN       	GAMEPAD_A_Pin

#define GAMEPAD_B_PORT      	GAMEPAD_B_GPIO_Port
#define GAMEPAD_B_PIN       	GAMEPAD_B_Pin

#define MAX_COUNT_VALUE			100
#define LOW_COUNT_THRESHOLD 	10
#define HIGH_COUNT_THRESHOLD	90

enum GAMEPAD_BUTTON {
	UP, DOWN, LEFT, RIGHT, A, B
};

void GAMEPAD_Reset(void);
void GAMEPAD_CalculateClick(void);
bool GAMEPAD_GetHoldButton(enum GAMEPAD_BUTTON button);
bool GAMEPAD_GetClickButton(enum GAMEPAD_BUTTON button);
void GAMEPAD_SetClickReadFlag(enum GAMEPAD_BUTTON button);

#endif /* INC_GAMEPAD_H_ */
