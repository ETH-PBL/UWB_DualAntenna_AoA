/*! ----------------------------------------------------------------------------
 * @file    port.c
 * @brief   HW specific definitions and functions for portability
 *
 * @attention
 *
 * Copyright 2016-2020 (c) DecaWave Ltd, Dublin, Ireland.
 *
 * All rights reserved.
 *
 * @author DecaWave
 */

#include <port.h>
//#include <stm32f1xx_hal_conf.h>
#include "main.h"

/****************************************************************************//**
 *
 *                              APP global variables
 *
 *******************************************************************************/
extern SPI_HandleTypeDef hspi5;

/****************************************************************************//**
 *
 *                  Port private variables and function prototypes
 *
 *******************************************************************************/
static volatile uint32_t signalResetDone;

/* DW IC IRQ handler definition. */
static port_dwic_isr_t port_dwic_isr = NULL;

/****************************************************************************//**
 *
 *                              Time section
 *
 *******************************************************************************/

/* @fn    portGetTickCnt
 * @brief wrapper for to read a SysTickTimer, which is incremented with
 *        CLOCKS_PER_SEC frequency.
 *        The resolution of time32_incr is usually 1/1000 sec.
 * */
__INLINE uint32_t
portGetTickCnt(void)
{
    return HAL_GetTick();
}


/* @fn    usleep
 * @brief precise usleep() delay
 * */
#pragma GCC optimize ("O0")
int usleep(useconds_t usec)
{
    unsigned int i;

    usec*=12;
    for(i=0;i<usec;i++)
    {
        __NOP();
    }
    return 0;
}


/* @fn    Sleep
 * @brief Sleep delay in ms using SysTick timer
 * */
__INLINE void
Sleep(uint32_t x)
{
    HAL_Delay(x);
}

/****************************************************************************//**
 *
 *                              END OF Time section
 *
 *******************************************************************************/

/****************************************************************************//**
 *
 *                              Configuration section
 *
 *******************************************************************************/

/* @fn    peripherals_init
 * */
int peripherals_init (void)
{
    /* All has been initialized in the CubeMx code, see main.c */
    return 0;
}


/* @fn    spi_peripheral_init
 * */
void spi_peripheral_init()
{

    /* SPI's has been initialized in the CubeMx code, see main.c */

    port_LCD_RS_clear();

    port_LCD_RW_clear();
}



/**
  * @brief  Checks whether the specified IRQn line is enabled or not.
  * @param  IRQn: specifies the IRQn line to check.
  * @return "0" when IRQn is "not enabled" and !0 otherwise
  */
ITStatus EXTI_GetITEnStatus(IRQn_Type IRQn)
{
        return ((NVIC->ISER[(((uint32_t)(int32_t)IRQn) >> 5UL)] &\
            (uint32_t)(1UL << (((uint32_t)(int32_t)IRQn) & 0x1FUL)) ) == (uint32_t)RESET)?(RESET):(SET);
}
/****************************************************************************//**
 *
 *                          End of configuration section
 *
 *******************************************************************************/

/****************************************************************************//**
 *
 *                          DW IC port section
 *
 *******************************************************************************/

/* @fn      reset_DW IC
 * @brief   DW_RESET pin on DW IC has 2 functions
 *          In general it is output, but it also can be used to reset the digital
 *          part of DW IC by driving this pin low.
 *          Note, the DW_RESET pin should not be driven high externally.
 * */
void reset_DWIC(void)
{
    GPIO_InitTypeDef    GPIO_InitStruct;

    // Enable GPIO used for DW1000 reset as open collector output
    GPIO_InitStruct.Pin = DW_RESET_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(DW_RESET_GPIO_Port, &GPIO_InitStruct);

    //drive the RSTn pin low
    HAL_GPIO_WritePin(DW_RESET_GPIO_Port, DW_RESET_Pin, GPIO_PIN_RESET);

    usleep(1);

    //put the pin back to output open-drain (not active)
    setup_DWICRSTnIRQ(0);
    Sleep(2);

}

/* @fn      setup_DWICRSTnIRQ
 * @brief   setup the DW_RESET pin mode
 *          0 - output Open collector mode
 *          !0 - input mode with connected EXTI0 IRQ
 * */
void setup_DWICRSTnIRQ(int enable)
{
    GPIO_InitTypeDef GPIO_InitStruct;

    if(enable)
    {
        // Enable GPIO used as DECA RESET for interrupt
        GPIO_InitStruct.Pin = DW_RESET_Pin;
        GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        HAL_GPIO_Init(DW_RESET_GPIO_Port, &GPIO_InitStruct);

        HAL_NVIC_EnableIRQ(EXTI0_IRQn);     //pin #0 -> EXTI #0
        HAL_NVIC_SetPriority(EXTI0_IRQn, 5, 0);
    }
    else
    {
        HAL_NVIC_DisableIRQ(EXTI0_IRQn);    //pin #0 -> EXTI #0

        //put the pin back to tri-state ... as
        //output open-drain (not active)
        GPIO_InitStruct.Pin = DW_RESET_Pin;
        GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
        HAL_GPIO_Init(DW_RESET_GPIO_Port, &GPIO_InitStruct);
        HAL_GPIO_WritePin(DW_RESET_GPIO_Port, DW_RESET_Pin, GPIO_PIN_SET);
    }
}

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
void wakeup_device_with_io(void)
{
    SET_WAKEUP_PIN_IO_HIGH;
    WAIT_500uSEC;
    SET_WAKEUP_PIN_IO_LOW;
}

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
void make_very_short_wakeup_io(void)
{
    uint8_t   cnt;

    SET_WAKEUP_PIN_IO_HIGH;
    for (cnt=0;cnt<10;cnt++)
        __NOP();
    SET_WAKEUP_PIN_IO_LOW;
}


/* @fn      port_is_boot1_low
 * @brief   check the BOOT1 pin status.
 * @return  1 if ON and 0 for OFF
 * */
int port_is_boot1_low(void)
{
    return ((GPIO_ReadInputDataBit(TA_BOOT1_GPIO, TA_BOOT1))?(0):(1));
}

/* @fn      port_is_boot1_on
 * @brief   check the BOOT1 pin is on.
 * @return  1 if ON and 0 for OFF
 * */
int port_is_boot1_on(uint16_t x)
{
    UNUSED(x);
    return ((GPIO_ReadInputDataBit(TA_BOOT1_GPIO, TA_BOOT1))?(0):(1));
}

/* @fn      port_is_switch_on
 * @brief   check the switch status.
 *          when switch (S1) is 'on' the pin is low
 * @return  1 if ON and 0 for OFF
 * */
int port_is_switch_on(uint16_t GPIOpin)
{
    return ((GPIO_ReadInputDataBit(TA_SW1_GPIO, GPIOpin))?(0):(1));
}


/* @fn      led_off
 * @brief   switch off the led from led_t enumeration
 * */
void led_off (led_t led)
{
#if (EVB1000_LCD_SUPPORT == 1)
    switch (led)
    {
    case LED_PC6:
        GPIO_ResetBits(LED5_GPIO_Port, LED5_Pin);
        break;
    case LED_PC7:
        GPIO_ResetBits(LED6_GPIO_Port, LED6_Pin);
        break;
    case LED_PC8:
        GPIO_ResetBits(LED7_GPIO_Port, LED7_Pin);
        break;
    case LED_PC9:
        GPIO_ResetBits(LED8_GPIO_Port, LED8_Pin);
        break;
    case LED_ALL:
        GPIO_ResetBits(LED5_GPIO_Port, LED5_Pin);
        GPIO_ResetBits(LED6_GPIO_Port, LED6_Pin);
        GPIO_ResetBits(LED7_GPIO_Port, LED7_Pin);
        GPIO_ResetBits(LED8_GPIO_Port, LED8_Pin);
        break;
    default:
        // do nothing for undefined led number
        break;
    }
#else
    UNUSED(led);
#endif
}

/* @fn      led_on
 * @brief   switch on the led from led_t enumeration
 * */
void led_on (led_t led)
{
#if (EVB1000_LCD_SUPPORT == 1)
    switch (led)
    {
    case LED_PC6:
        GPIO_SetBits(LED5_GPIO_Port, LED5_Pin);
        break;
    case LED_PC7:
        GPIO_SetBits(LED6_GPIO_Port, LED6_Pin);
        break;
    case LED_PC8:
        GPIO_SetBits(LED7_GPIO_Port, LED7_Pin);
        break;
    case LED_PC9:
        GPIO_SetBits(LED8_GPIO_Port, LED8_Pin);
        break;
    case LED_ALL:
        GPIO_SetBits(LED5_GPIO_Port, LED5_Pin);
        GPIO_SetBits(LED6_GPIO_Port, LED6_Pin);
        GPIO_SetBits(LED7_GPIO_Port, LED7_Pin);
        GPIO_SetBits(LED8_GPIO_Port, LED8_Pin);
        break;
    default:
        // do nothing for undefined led number
        break;
    }
#else
    UNUSED(led);
#endif
}


/* @fn      port_set_dw_ic_spi_slowrate
 * @brief   set 4.5MHz
 *          note: hspi5 is clocked from 72MHz
 * */
void port_set_dw_ic_spi_slowrate(void)
{
	hspi5.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_16;
    HAL_SPI_Init(&hspi5);
}

/* @fn      port_set_dw_ic_spi_fastrate
 * @brief   set 18MHz
 *          note: hspi5 is clocked from 72MHz
 * */
void port_set_dw_ic_spi_fastrate(void)
{
	hspi5.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_2;
    HAL_SPI_Init(&hspi5);
}

/* @fn      port_LCD_RS_set
 * @brief   wrapper to set LCD_RS pin
 * */
void port_LCD_RS_set(void)
{
#if (EVB1000_LCD_SUPPORT == 1)
    HAL_GPIO_WritePin(LCD_RS_GPIO_Port, LCD_RS_Pin, GPIO_PIN_SET);
#endif
}

/* @fn      port_LCD_RS_clear
 * @brief   wrapper to clear LCD_RS pin
 * */
void port_LCD_RS_clear(void)
{
#if (EVB1000_LCD_SUPPORT == 1)
    HAL_GPIO_WritePin(LCD_RS_GPIO_Port, LCD_RS_Pin, GPIO_PIN_RESET);
#endif
}

/* @fn      port_LCD_RW_clear
 * @brief   wrapper to set LCD_RW pin
 * */
void port_LCD_RW_set(void)
{
#if (EVB1000_LCD_SUPPORT == 1)
    HAL_GPIO_WritePin(LCD_RW_GPIO_Port, LCD_RW_Pin, GPIO_PIN_SET);
#endif
}

/* @fn      port_LCD_RW_clear
 * @brief   wrapper to clear LCD_RW pin
 * */
void port_LCD_RW_clear(void)
{
#if (EVB1000_LCD_SUPPORT == 1)
    HAL_GPIO_WritePin(LCD_RW_GPIO_Port, LCD_RW_Pin, GPIO_PIN_RESET);
#endif
}

/****************************************************************************//**
 *
 *                          End APP port section
 *
 *******************************************************************************/



/****************************************************************************//**
 *
 *                              IRQ section
 *
 *******************************************************************************/

/* @fn         HAL_GPIO_EXTI_Callback
 * @brief      EXTI line detection callback from HAL layer
 * @param      GPIO_Pin: Specifies the port pin connected to corresponding EXTI line.
 *             i.e. DW_RESET_Pin and DW_IRQn_Pin
 */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    switch ( GPIO_Pin )
    {
//    case DW_RST_B_Pin :
//    case DW_RST_A_Pin :
//        {
//            dw_rst_pin_irq_cb(); /* bare-metal signal */
//            break;
//        }
//
    case DW_RESET_Pin :
        signalResetDone = 1;
        break;

    case DW_IRQn_Pin :
        {
            //while (HAL_GPIO_ReadPin(DECAIRQ_GPIO, DW_IRQn_Pin) == GPIO_PIN_SET)
            {
                process_deca_irq();
                //dwt_isr();
            }

            break;
        }

    default :
        break;
    }
}

/* @fn      process_deca_irq
 * @brief   main call-back for processing of DW3000 IRQ
 *          it re-enters the IRQ routing and processes all events.
 *          After processing of all events, DW3000 will clear the IRQ line.
 * */
__INLINE void process_deca_irq(void)
{
    while(port_CheckEXT_IRQ() != 0)
    {
        if(port_dwic_isr)
        {
            port_dwic_isr();
        }
        else
        {
        	/* The interrupt will never be cleared if port_dwic_isr is not
        	 * defined => skip handling this interrupt now or we will end up
             * in an infinite loop.
        	 */
        	break;
        }
    } //while DW3000 IRQ line active
}


/* @fn      port_DisableEXT_IRQ
 * @brief   wrapper to disable DW_IRQ pin IRQ
 *          in current implementation it disables all IRQ from lines 5:9
 * */
__INLINE void port_DisableEXT_IRQ(void)
{
    NVIC_DisableIRQ(DECAIRQ_EXTI_IRQn);
}

/* @fn      port_EnableEXT_IRQ
 * @brief   wrapper to enable DW_IRQ pin IRQ
 *          in current implementation it enables all IRQ from lines 5:9
 * */
__INLINE void port_EnableEXT_IRQ(void)
{
    NVIC_EnableIRQ(DECAIRQ_EXTI_IRQn);
}


/* @fn      port_GetEXT_IRQStatus
 * @brief   wrapper to read a DW_IRQ pin IRQ status
 * */
__INLINE uint32_t port_GetEXT_IRQStatus(void)
{
    return EXTI_GetITEnStatus(DECAIRQ_EXTI_IRQn);
}


/* @fn      port_CheckEXT_IRQ
 * @brief   wrapper to read DW_IRQ input pin state
 * */
__INLINE uint32_t port_CheckEXT_IRQ(void)
{
    return HAL_GPIO_ReadPin(DECAIRQ_GPIO, DECAIRQ);
}


/*! ------------------------------------------------------------------------------------------------------------------
 * @fn port_set_dwic_isr()
 *
 * @brief This function is used to install the handling function for DW IC IRQ.
 *
 * NOTE:
 *   - The user application shall ensure that a proper handler is set by calling this function before any DW IC IRQ occurs.
 *   - This function deactivates the DW IC IRQ line while the handler is installed.
 *
 * @param deca_isr function pointer to DW IC interrupt handler to install
 *
 * @return none
 */
void port_set_dwic_isr(port_dwic_isr_t dwic_isr)
{
    /* Check DW IC IRQ activation status. */
    ITStatus en = port_GetEXT_IRQStatus();

    /* If needed, deactivate DW IC IRQ during the installation of the new handler. */
    port_DisableEXT_IRQ();

    port_dwic_isr = dwic_isr;

    if (!en)
    {
        port_EnableEXT_IRQ();
    }
}


/****************************************************************************//**
 *
 *                              END OF IRQ section
 *
 *******************************************************************************/
