/*! ----------------------------------------------------------------------------
 * @file    port.h
 * @brief   HW specific definitions and functions for portability
 *
 * @attention
 *
 * Copyright 2015-2020 (c) DecaWave Ltd, Dublin, Ireland.
 *
 * All rights reserved.
 *
 * @author DecaWave
 */


#ifndef PORT_H_
#define PORT_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <string.h>
#include <sys/types.h>
//#include <compiler.h>

#include <stm32f4xx_hal.h>

#define EVB1000_LED_SUPPORT 0
#define EVB1000_LCD_SUPPORT 0

/* DW IC IRQ (EXTI15_10_IRQ) handler type. */
typedef void (*port_dwic_isr_t)(void);

/*! ------------------------------------------------------------------------------------------------------------------
 * @fn port_set_DWIC_isr()
 *
 * @brief This function is used to install the handling function for DW1000 IRQ.
 *
 * NOTE:
 *   - As EXTI9_5_IRQHandler does not check that port_deca_isr is not null, the user application must ensure that a
 *     proper handler is set by calling this function before any DW1000 IRQ occurs!
 *   - This function makes sure the DW1000 IRQ line is deactivated while the handler is installed.
 *
 * @param deca_isr function pointer to DW1000 interrupt handler to install
 *
 * @return none
 */
void port_set_dwic_isr(port_dwic_isr_t isr);

#define BUFFLEN     (4096+128)

#define BUF_SIZE    (64)

#define USB_SUPPORT

typedef struct
{
    uint16_t        usblen;                 /**< for RX from USB */
    uint8_t         usbbuf[BUF_SIZE*3];     /**< for RX from USB */
}__packed app_t;


extern app_t    app;


/*****************************************************************************************************************//*
**/

 /****************************************************************************//**
  *
  *                                 Types definitions
  *
  *******************************************************************************/

#ifndef FALSE
#define FALSE               0
#endif

#ifndef TRUE
#define TRUE                1
#endif

typedef enum
{
    LED_PC6, //LED5
    LED_PC7, //LED6
    LED_PC8, //LED7
    LED_PC9, //LED8
    LED_ALL,
    LEDn
} led_t;

/****************************************************************************//**
 *
 *                              MACRO
 *
 *******************************************************************************/


#define DECAIRQ_EXTI_IRQn       (EXTI15_10_IRQn)

#define DW_RSTn                     DW_RESET_Pin
#define DW_RSTn_GPIO                DW_RESET_GPIO_Port

#define DECAIRQ                     DW_IRQn_Pin
#define DECAIRQ_GPIO                DW_IRQn_GPIO_Port

#define TA_BOOT1                    GPIO_PIN_2
#define TA_BOOT1_GPIO               GPIOB

#define TA_RESP_DLY                 GPIO_PIN_0
#define TA_RESP_DLY_GPIO            GPIOC

#define TA_SW1_3                    GPIO_PIN_0
#define TA_SW1_4                    GPIO_PIN_1
#define TA_SW1_5                    GPIO_PIN_2
#define TA_SW1_6                    GPIO_PIN_3
#define TA_SW1_7                    GPIO_PIN_4
#define TA_SW1_8                    GPIO_PIN_5
#define TA_SW1_GPIO                 GPIOC

/****************************************************************************//**
 *
 *                              MACRO function
 *
 *******************************************************************************/

#define GPIO_ResetBits(x,y)             HAL_GPIO_WritePin(x,y, RESET)
#define GPIO_SetBits(x,y)               HAL_GPIO_WritePin(x,y, SET)
#define GPIO_ReadInputDataBit(x,y)      HAL_GPIO_ReadPin (x,y)


/* NSS pin is SW controllable */
#define port_SPIx_set_chip_select()     HAL_GPIO_WritePin(DW_NSS_GPIO_Port, DW_NSS_Pin, GPIO_PIN_SET)
#define port_SPIx_clear_chip_select()   HAL_GPIO_WritePin(DW_NSS_GPIO_Port, DW_NSS_Pin, GPIO_PIN_RESET)

/* NSS pin is SW controllable */
#if (EVB1000_LCD_SUPPORT == 1)
#define port_SPIy_set_chip_select()     HAL_GPIO_WritePin(LCD_NSS_GPIO_Port, LCD_NSS_Pin, GPIO_PIN_SET)
#define port_SPIy_clear_chip_select()   HAL_GPIO_WritePin(LCD_NSS_GPIO_Port, LCD_NSS_Pin, GPIO_PIN_RESET)
#endif

/****************************************************************************//**
 *
 *                              port function prototypes
 *
 *******************************************************************************/

int usleep(useconds_t usec);

void Sleep(uint32_t Delay);
unsigned long portGetTickCnt(void);

#define S1_SWITCH_ON  (1)
#define S1_SWITCH_OFF (0)
//when switch (S1) is 'on' the pin is low
int port_is_boot1_on(uint16_t x);
int port_is_switch_on(uint16_t GPIOpin);
int port_is_boot1_low(void);


void port_set_dw_ic_spi_slowrate(void);
void port_set_dw_ic_spi_fastrate(void);

void process_dwRSTn_irq(void);
void process_deca_irq(void);

void led_on(led_t led);
void led_off(led_t led);

int  peripherals_init(void);
void spi_peripheral_init(void);

void setup_DWICRSTnIRQ(int enable);

void reset_DWIC(void);


void port_LCD_RS_set(void);
void port_LCD_RS_clear(void);
void port_LCD_RW_set(void);
void port_LCD_RW_clear(void);

ITStatus EXTI_GetITEnStatus(IRQn_Type x);

uint32_t port_GetEXT_IRQStatus(void);
uint32_t port_CheckEXT_IRQ(void);
void port_DisableEXT_IRQ(void);
void port_EnableEXT_IRQ(void);
extern uint32_t     HAL_GetTick(void);
HAL_StatusTypeDef   flush_report_buff(void);

/*! ------------------------------------------------------------------------------------------------------------------
* @fn wakeup_device_with_io()
*
* @brief This function wakes up the device by toggling io with a delay.
*
* input None
*
* output -None
*
*/
void wakeup_device_with_io(void);

/*! ------------------------------------------------------------------------------------------------------------------
* @fn make_very_short_wakeup_io()
*
* @brief This will toggle the wakeup pin for a very short time. The device should not wakeup
*
* input None
*
* output -None
*
*/
void make_very_short_wakeup_io(void);


//This set the GPIOC Low or High in order to measure times.
#define SET_MEAS_PIN_IO_LOW     HAL_GPIO_WritePin(DW_MEAS_TIME_GPIO_Port, DW_MEAS_TIME_Pin, GPIO_PIN_RESET)
#define SET_MEAS_PIN_IO_HIGH    HAL_GPIO_WritePin(DW_MEAS_TIME_GPIO_Port, DW_MEAS_TIME_Pin, GPIO_PIN_SET)

//This set the IO for waking up the chip
#define SET_WAKEUP_PIN_IO_LOW     HAL_GPIO_WritePin(DW_NSS_GPIO_Port, DW_NSS_Pin, GPIO_PIN_RESET)
#define SET_WAKEUP_PIN_IO_HIGH    HAL_GPIO_WritePin(DW_NSS_GPIO_Port, DW_NSS_Pin, GPIO_PIN_SET)

#define WAIT_500uSEC    Sleep(1)/*This is should be a delay of 500uSec at least. In our example it is more than that*/

#if (EVB1000_LCD_SUPPORT == 1)
/*! ------------------------------------------------------------------------------------------------------------------
 * Function: writetoLCD()
 *
 * Low level abstract function to write data to the LCD display via SPI2 peripheral
 * Takes byte buffer and rs_enable signals
 * or returns -1 if there was an error
 */
void writetoLCD
(
    uint32_t       bodylength,
    uint8_t        rs_enable,
    const uint8_t *bodyBuffer
);

/*! ------------------------------------------------------------------------------------------------------------------
 * @fn lcd_display_str()
 *
 * @brief Display a string on the LCD screen.
 * /!\ The string must be 16 chars long maximum!
 *
 * @param  string  the string to display
 *
 * @return none
 */
void lcd_display_str(const char *string1);
void lcd_display_str2(const char *string1, const char *string2);
#else
#define writetoLCD(x)
#define lcd_display_str(x) ((void)0)
#define lcd_display_str2(x) ((void)0)
#endif

#ifdef __cplusplus
}
#endif

#endif /* PORT_H_ */


/*
 * Taken from the Linux Kernel
 *
 */

#ifndef _LINUX_CIRC_BUF_H
#define _LINUX_CIRC_BUF_H 1

struct circ_buf {
    char *buf;
    int head;
    int tail;
};

/* Return count in buffer.  */
#define CIRC_CNT(head,tail,size) (((head) - (tail)) & ((size)-1))

/* Return space available, 0..size-1.  We always leave one free char
   as a completely full buffer has head == tail, which is the same as
   empty.  */
#define CIRC_SPACE(head,tail,size) CIRC_CNT((tail),((head)+1),(size))

/* Return count up to the end of the buffer.  Carefully avoid
   accessing head and tail more than once, so they can change
   underneath us without returning inconsistent results.  */
#define CIRC_CNT_TO_END(head,tail,size) \
    ({int end = (size) - (tail); \
      int n = ((head) + end) & ((size)-1); \
      n < end ? n : end;})

/* Return space available up to the end of the buffer.  */
#define CIRC_SPACE_TO_END(head,tail,size) \
    ({int end = (size) - 1 - (head); \
      int n = (end + (tail)) & ((size)-1); \
      n <= end ? n : end+1;})

#endif /* _LINUX_CIRC_BUF_H  */

