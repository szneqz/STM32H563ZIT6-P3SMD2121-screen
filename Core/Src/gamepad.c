#include "gamepad.h"

static uint8_t gamepad_up_count = 0;
static uint8_t gamepad_down_count = 0;
static uint8_t gamepad_left_count = 0;
static uint8_t gamepad_right_count = 0;
static uint8_t gamepad_a_count = 0;
static uint8_t gamepad_b_count = 0;

static bool gamepad_up_hold = false;
static bool gamepad_down_hold = false;
static bool gamepad_left_hold = false;
static bool gamepad_right_hold = false;
static bool gamepad_a_hold = false;
static bool gamepad_b_hold = false;

static bool gamepad_up_click = false;
static bool gamepad_down_click = false;
static bool gamepad_left_click = false;
static bool gamepad_right_click = false;
static bool gamepad_a_click = false;
static bool gamepad_b_click = false;

static bool gamepad_up_click_read = false;
static bool gamepad_down_click_read = false;
static bool gamepad_left_click_read = false;
static bool gamepad_right_click_read = false;
static bool gamepad_a_click_read = false;
static bool gamepad_b_click_read = false;

void GAMEPAD_Reset(void) {
	gamepad_up_count = 0;
	gamepad_down_count = 0;
	gamepad_left_count = 0;
	gamepad_right_count = 0;
	gamepad_a_count = 0;
	gamepad_b_count = 0;

	gamepad_up_hold = false;
	gamepad_down_hold = false;
	gamepad_left_hold = false;
	gamepad_right_hold = false;
	gamepad_a_hold = false;
	gamepad_b_hold = false;

	gamepad_up_click = false;
	gamepad_down_click = false;
	gamepad_left_click = false;
	gamepad_right_click = false;
	gamepad_a_click = false;
	gamepad_b_click = false;

	gamepad_up_click_read = false;
	gamepad_down_click_read = false;
	gamepad_left_click_read = false;
	gamepad_right_click_read = false;
	gamepad_a_click_read = false;
	gamepad_b_click_read = false;
}

static inline void prv_CountPress(const GPIO_TypeDef *GPIOx, uint16_t GPIO_Pin, uint8_t *gamepad_count,
		bool *gamepad_count_hold, bool *gamepad_count_click, bool *gamepad_count_click_read) {
	if (HAL_GPIO_ReadPin(GPIOx, GPIO_Pin) == GPIO_PIN_RESET) {
		if ((*gamepad_count) < MAX_COUNT_VALUE)
			(*gamepad_count)++;

		if ((*gamepad_count) >= HIGH_COUNT_THRESHOLD) {
			*gamepad_count_hold = true;
			if ((*gamepad_count_click) == false) {
				*gamepad_count_click = true;
				*gamepad_count_click_read = false;
			}
		}
	}
	else {
		if ((*gamepad_count) > 0)
			(*gamepad_count)--;

		if ((*gamepad_count) <= LOW_COUNT_THRESHOLD) {
			*gamepad_count_hold = false;
			*gamepad_count_click = false;
			*gamepad_count_click_read = false;
		}
	}
}

void GAMEPAD_CalculateClick(void) {
	prv_CountPress(GAMEPAD_UP_PORT, GAMEPAD_UP_PIN, &gamepad_up_count, &gamepad_up_hold, &gamepad_up_click, &gamepad_up_click_read);
	prv_CountPress(GAMEPAD_DOWN_PORT, GAMEPAD_DOWN_PIN, &gamepad_down_count, &gamepad_down_hold, &gamepad_down_click, &gamepad_down_click_read);
	prv_CountPress(GAMEPAD_LEFT_PORT, GAMEPAD_LEFT_PIN, &gamepad_left_count, &gamepad_left_hold, &gamepad_left_click, &gamepad_left_click_read);
	prv_CountPress(GAMEPAD_RIGHT_PORT, GAMEPAD_RIGHT_PIN, &gamepad_right_count, &gamepad_right_hold, &gamepad_right_click, &gamepad_right_click_read);
	prv_CountPress(GAMEPAD_A_PORT, GAMEPAD_A_PIN, &gamepad_a_count, &gamepad_a_hold, &gamepad_a_click, &gamepad_a_click_read);
	prv_CountPress(GAMEPAD_B_PORT, GAMEPAD_B_PIN, &gamepad_b_count, &gamepad_b_hold, &gamepad_b_click, &gamepad_b_click_read);
}

bool GAMEPAD_GetHoldButton(enum GAMEPAD_BUTTON button) {
	if (button == UP) return gamepad_up_hold;
	if (button == DOWN) return gamepad_down_hold;
	if (button == LEFT) return gamepad_left_hold;
	if (button == RIGHT) return gamepad_right_hold;
	if (button == A) return gamepad_a_hold;
	if (button == B) return gamepad_b_hold;

	return false;
}

bool GAMEPAD_GetClickButton(enum GAMEPAD_BUTTON button) {
	if (button == UP && !gamepad_up_click_read) return gamepad_up_click;
	if (button == DOWN && !gamepad_down_click_read) return gamepad_down_click;
	if (button == LEFT && !gamepad_left_click_read) return gamepad_left_click;
	if (button == RIGHT && !gamepad_right_click_read) return gamepad_right_click;
	if (button == A && !gamepad_a_click_read) return gamepad_a_click;
	if (button == B && !gamepad_b_click_read) return gamepad_b_click;

	return false;
}

void GAMEPAD_SetClickReadFlag(enum GAMEPAD_BUTTON button) {
	if (button == UP) gamepad_up_click_read = true;
	if (button == DOWN) gamepad_down_click_read = true;
	if (button == LEFT) gamepad_left_click_read = true;
	if (button == RIGHT) gamepad_right_click_read = true;
	if (button == A) gamepad_a_click_read = true;
	if (button == B) gamepad_b_click_read = true;
}




