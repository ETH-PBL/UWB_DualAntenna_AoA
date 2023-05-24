/*
 * application_twr_tag.c
 *
 *  Created on: July 13, 2022
 *      Author: Tobias Margiani
 */

#include "applications.h"

#ifdef APPLICATION_TX_TEST

#include <stdio.h>
#include <string.h>

#include "main.h"
#include "deca_regs.h"
#include "deca_spi.h"
#include "port.h"
#include "uart_stdio.h"

#include "application_config.h"
#include "shared_functions.h"

static void tx_done_cb(const dwt_cb_data_t *cb_data);
static void rx_ok_cb(const dwt_cb_data_t *cb_data);
static void rx_err_cb(const dwt_cb_data_t *cb_data);

volatile uint8_t rx_done = 0;  /* Flag to indicate a new frame was received from the interrupt */
volatile uint16_t new_frame_length = 0;
volatile uint8_t tx_done = 0;

char print_buffer[64];

twr_base_frame_t sync_frame = {
		{ 0x41, 0x88 },	/* Frame Control: data frame, short addresses */
		0,				/* Sequence number */
		{ 'X', 'X' },	/* PAN ID */
		{ 'A', 'A' },	/* Destination address */
		{ 'T', 'T' },	/* Source address */
		0x20,			/* Function code: 0x20 ranging initiation */
};

/**
 * Application entry point.
 */
int dw_main(void)
{
    stdio_write("DW3000 TEST\n");

    /* Configure SPI rate, DW IC supports up to 38 MHz */
    port_set_dw_ic_spi_fastrate();

    /* Reset DW IC */
    reset_DWIC(); /* Target specific drive of RSTn line into DW IC low for a period. */

    Sleep(20); // Time needed for DW3000 to start up (transition from INIT_RC to IDLE_RC, or could wait for SPIRDY event)

    while (!dwt_checkidlerc()) /* Need to make sure DW IC is in IDLE_RC before proceeding */
    { };

    if (dwt_initialise(DWT_DW_INIT) == DWT_ERROR)
    {
    	stdio_write("INIT FAILED\n");
        while (1) { };
    }

    stdio_write("INITIALIZED\n");

    /* Enabling LEDs here for debug so that for each RX-enable the D2 LED will flash on DW3000 red eval-shield boards. */
    dwt_setleds(DWT_LEDS_ENABLE | DWT_LEDS_INIT_BLINK);

    /* Configure DW IC. */
    if(dwt_configure(&config)) /* if the dwt_configure returns DWT_ERROR either the PLL or RX calibration has failed the host should reset the device */
    {
    	stdio_write("CONFIG FAILED\n");
        while (1)
        { };
    }

    stdio_write("CONFIGURED\n");

    /* Register RX call-back. */
    dwt_setcallbacks(tx_done_cb, rx_ok_cb, rx_err_cb, rx_err_cb, NULL, NULL);

    /* Enable wanted interrupts (TX confirmation). */
    dwt_setinterrupt(SYS_ENABLE_LO_TXFRS_ENABLE_BIT_MASK, 0, DWT_ENABLE_INT);

    /*Clearing the SPI ready interrupt*/
    dwt_write32bitreg(SYS_STATUS_ID, SYS_STATUS_RCINIT_BIT_MASK | SYS_STATUS_SPIRDY_BIT_MASK);

    /* Install DW IC IRQ handler. */
    port_set_dwic_isr(dwt_isr);

    uint32_t last_sync_time = HAL_GetTick();

	while (1)
	{
		if ((HAL_GetTick() - last_sync_time) > 2000) {
			tx_done = 0;
			last_sync_time = HAL_GetTick();

			uint32_t sys_state = dwt_read32bitreg(SYS_STATE_LO_ID);
			snprintf(print_buffer, sizeof(print_buffer), "sys_state pre: 0x%lX\n", sys_state);
			stdio_write(print_buffer);

			/* At this point the receiver will still be turned on from the last tx with response expected
			 * (at least if there is no response). Without forcing the transmitter off here, a tx start
			 * will not work. */
			dwt_forcetrxoff();

			sys_state = dwt_read32bitreg(SYS_STATE_LO_ID);
			snprintf(print_buffer, sizeof(print_buffer), "sys_state off: 0x%lX\n", sys_state);
			stdio_write(print_buffer);

			stdio_write("start tx\n");
			dwt_writetxdata(sizeof(sync_frame), (uint8_t *)&sync_frame, 0);
			dwt_writetxfctrl(sizeof(sync_frame)+2, 0, 1); /* Zero offset in TX buffer, ranging. */

			// option 1
			int r = dwt_starttx(DWT_START_TX_IMMEDIATE | DWT_RESPONSE_EXPECTED);

			// option 2
			//int r = dwt_starttx(DWT_START_TX_IMMEDIATE);

			if (r == DWT_ERROR) {
				stdio_write("tx error\n");
			} else {
				stdio_write("tx success\n");
			}

			sys_state = dwt_read32bitreg(SYS_STATE_LO_ID);
			snprintf(print_buffer, sizeof(print_buffer), "sys_state post: 0x%lX\n", sys_state);
			stdio_write(print_buffer);
		}

		if (tx_done == 1) {
			tx_done = 0;
			stdio_write("TX: Interrupt\n");

			// option 2
			//dwt_rxenable(DWT_START_RX_IMMEDIATE);
		}
	}

    return DWT_SUCCESS;
}

/*! ------------------------------------------------------------------------------------------------------------------
 * @fn tx_done_cb()
 *
 * @brief Callback called after TX
 *
 * @param  cb_data  callback data
 *
 * @return  none
 */
static void tx_done_cb(const dwt_cb_data_t *cb_data)
{
	UNUSED(cb_data);
	tx_done = 1;
}

/*! ------------------------------------------------------------------------------------------------------------------
 * @fn rx_ok_cb()
 *
 * @brief Callback to process RX good frame events
 *
 * @param  cb_data  callback data
 *
 * @return  none
 */
static void rx_ok_cb(const dwt_cb_data_t *cb_data)
{
	rx_done = 1;
	new_frame_length = cb_data->datalength;
}

/*! ------------------------------------------------------------------------------------------------------------------
 * @fn rx_err_cb()
 *
 * @brief Callback to process RX error and timeout events
 *
 * @param  cb_data  callback data
 *
 * @return  none
 */
static void rx_err_cb(const dwt_cb_data_t *cb_data)
{
	UNUSED(cb_data);
	dwt_rxenable(DWT_START_RX_IMMEDIATE);
}

#endif
