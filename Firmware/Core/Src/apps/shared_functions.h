/*
 * shared_functions.h
 *
 *  Created on: 06.07.2022
 *      Author: tobias
 */

#ifndef SRC_APPS_SHARED_FUNCTIONS_H_
#define SRC_APPS_SHARED_FUNCTIONS_H_

/* Decode a 24-bit number stored in a 3-byte uint8_t array */
int32_t decode_24bit(const uint8_t* buffer);

/* Decode a 40-bit number stored in a 5-byte uint8_t array */
uint64_t decode_40bit_timestamp(const uint8_t buffer[5]);

/* Rotate the stepper motor */
void rotate_reciever(int degrees);

#endif /* SRC_APPS_SHARED_FUNCTIONS_H_ */
