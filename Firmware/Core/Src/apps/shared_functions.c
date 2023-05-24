/*
 * shared_functions.c
 *
 *  Created on: 06.07.2022
 *      Author: tobias
 */

#include <stdio.h>
#include "port.h"
#include "main.h"
#include "shared_functions.h"


int32_t decode_24bit(const uint8_t* buffer) {
	/* 24th bit a 1 to compute sign extension of the three byte CIR sample values */
	const int32_t m = 1u << 23; // 24th bit is 1

	/* combine three bytes into one integer */
	int32_t value = ((uint32_t)buffer[0]) \
					+ ((uint32_t)buffer[1] << 8) \
					+ ((uint32_t)buffer[2] << 16);
	/* Sign extension:
	 * a positive number will have a 0 as 24th bit => this does nothing,
	 * a negative number will have a 1 as 24th bit => xor makes a 0, subtraction
	 * adds 1s until the end of the number effectively doing sign extension
	 */
	value = (value ^ m) - m;
	return value;
}


uint64_t decode_40bit_timestamp(const uint8_t buffer[5]) {
	/* combine five bytes into one integer */
	const uint64_t value = ((uint64_t)buffer[0]) \
							+ ((uint64_t)buffer[1] << 8) \
							+ ((uint64_t)buffer[2] << 16) \
							+ ((uint64_t)buffer[3] << 24) \
							+ ((uint64_t)buffer[4] << 32);
	return value;
}


void rotate_reciever(int degrees) {
	if (0 == degrees) {
		return;
	} else if (0 < degrees) {
		HAL_GPIO_WritePin(MOTOR_DIR_GPIO_Port, MOTOR_DIR_Pin, GPIO_PIN_RESET);
	} else {
		HAL_GPIO_WritePin(MOTOR_DIR_GPIO_Port, MOTOR_DIR_Pin, GPIO_PIN_SET);
		degrees = -degrees;
	}

	for (int i = 0; i < degrees; i++) {
		HAL_GPIO_WritePin(MOTOR_STEP_GPIO_Port, MOTOR_STEP_Pin, GPIO_PIN_SET);
		Sleep(40);
		HAL_GPIO_WritePin(MOTOR_STEP_GPIO_Port, MOTOR_STEP_Pin, GPIO_PIN_RESET);
		Sleep(40);
	}
}
