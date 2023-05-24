/**
* @file       port_stdio.c
*
* @brief      HW specific functions for standard IO interface
*
* @attention  Copyright 2018-2019 (c) Decawave Ltd, Dublin, Ireland.
*             All rights reserved.
*
* @author     Decawave
*
* This file contains target specific implementations of functions used by the
* production test program for reading from the standard input and writing to the
* standard output. This standard I/O can be a UART peripheral, Segger RTT,
* semihosting, an LCD, ... As long a it can handle sending a data .
*/

#include <stdint.h>
#include <string.h>

#include "uart_stdio.h"

/* Platform specific includes */
#include "main.h"

static UART_HandleTypeDef* uart = NULL;

/*! ----------------------------------------------------------------------------
 * @fn port_stdio_init
 * @brief Initialize stdio on the given UART
 *
 * @param[in] huart Pointer to the STM32 HAL UART peripheral instance
 */
void stdio_init(UART_HandleTypeDef* huart) {
    uart = huart;
}

/*! ----------------------------------------------------------------------------
 * @fn stdio_write
 * @brief Transmit/write data to standard output
 *
 * @param[in] data Pointer to null terminated string
 * @return Number of bytes transmitted or -1 if an error occurred
 */
inline int stdio_write(const char *data)
{
    uint16_t len = strlen(data);
    if (HAL_UART_Transmit(uart, (uint8_t*) data, len, HAL_MAX_DELAY) == HAL_OK) {
        return len;
    }
    return -1;
}

inline int stdio_write_binary(const uint8_t *data, uint16_t length)
{
    if (HAL_UART_Transmit(uart, data, length, HAL_MAX_DELAY) == HAL_OK) {
        return length;
    }
    return -1;
}
