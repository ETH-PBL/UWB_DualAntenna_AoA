/*! ----------------------------------------------------------------------------
 * @file      stdio.h
 *
 * @brief     HW specific functions for standard IO interface
 *
 * @author    Decawave
 *
 * @attention Copyright 2020 (c) Decawave Ltd, Dublin, Ireland.
 *            All rights reserved.
 */

#ifndef _PORT_STDIO_H_
#define _PORT_STDIO_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* Platform specific includes */
#include "stm32f4xx_hal.h"

/*! ----------------------------------------------------------------------------
 * @fn stdio_init
 * @brief Initialize stdio on the given UART
 *
 * @param[in] huart Pointer to the STM32 HAL UART peripherial instance
 */
void stdio_init(UART_HandleTypeDef* huart);

/*! ----------------------------------------------------------------------------
 * @fn stdio_write
 * @brief Transmit/write data to standard output
 *
 * @param[in] data Pointer to null terminated string
 * @return Number of bytes transmitted or -1 if an error occured
 */
int stdio_write(const char *data);

int stdio_write_binary(const uint8_t *data, uint16_t length);

#ifdef __cplusplus
}
#endif

#endif /* _PORT_STDIO_H_ */
