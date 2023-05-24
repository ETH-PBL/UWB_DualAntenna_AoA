/*
 * application_twr_tag.c
 *
 *  Created on: July 05, 2022
 *      Author: Tobias Margiani
 */

#include "applications.h"

#ifdef APPLICATION_TWR_ANCHOR

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

volatile uint8_t new_frame = 0;  /* Flag to indicate a new frame was received from the interrupt */
volatile uint16_t new_frame_length = 0;
volatile uint8_t tx_done = 0;

char print_buffer[64];

twr_base_frame_t poll_frame = {
		{ 0x41, 0x88 },	/* Frame Control: data frame, short addresses */
		0,				/* Sequence number */
		{ 'X', 'X' },	/* PAN ID */
		{ 'T', 'T' },	/* Destination address */
		{ 'A', 'A' },	/* Source address */
		0x21,			/* Function code: 0x21 ranging poll */
};

twr_final_frame_t final_frame = {
		{ 0x41, 0x88 },		/* Frame Control: data frame, short addresses */
		0,					/* Sequence number */
		{ 'X', 'X' },		/* PAN ID */
		{ 'T', 'T' },		/* Destination address */
		{ 'A', 'A' },		/* Source address */
		0x23,				/* Function code: 0x22 ranging final with embedded timestamp */
		{ 0, 0, 0, 0, 0 },	/* Time from TX of poll to RX of response frame (i.e. Tround1) */
		{ 0, 0, 0, 0, 0 },	/* Time from RX of response to TX of final frame (i.e. Treply2) */
		/* According to ISO/IEC 24730-62:2013 the thre timestamps at the end should be only 32-bits each
		 * but then we would just discard values and loose accuracy. */
};

const static size_t max_frame_length = sizeof(twr_final_frame_t) + 2;

const static uint64_t round_tx_delay = 10llu*1000llu*US_TO_DWT_TIME;  // reply time (10ms)

uint64_t tx_timestamp_poll = 0;
uint64_t rx_timestamp_response = 0;
uint64_t tx_timestamp_final = 0;

uint8_t next_sequence_number = 0;

enum state_t {
	TWR_SYNC_STATE,
	TWR_POLL_RESPONSE_STATE,
	TWR_FINAL_STATE,
	TWR_ERROR,
};

enum state_t state = TWR_SYNC_STATE;

/**
 * Application entry point.
 */
int dw_main(void)
{
    stdio_write("DW3000 TEST TWR Anchor\n");

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

    /* Enable wanted interrupts (TX confirmation, RX good frames, RX timeouts and RX errors). */
    dwt_setinterrupt(SYS_ENABLE_LO_TXFRS_ENABLE_BIT_MASK | SYS_ENABLE_LO_RXFCG_ENABLE_BIT_MASK | SYS_ENABLE_LO_RXFTO_ENABLE_BIT_MASK |
            SYS_ENABLE_LO_RXPTO_ENABLE_BIT_MASK | SYS_ENABLE_LO_RXPHE_ENABLE_BIT_MASK | SYS_ENABLE_LO_RXFCE_ENABLE_BIT_MASK |
            SYS_ENABLE_LO_RXFSL_ENABLE_BIT_MASK | SYS_ENABLE_LO_RXSTO_ENABLE_BIT_MASK, 0, DWT_ENABLE_INT);

    /*Clearing the SPI ready interrupt*/
    dwt_write32bitreg(SYS_STATUS_ID, SYS_STATUS_RCINIT_BIT_MASK | SYS_STATUS_SPIRDY_BIT_MASK);

    /* Install DW IC IRQ handler. */
    port_set_dwic_isr(dwt_isr);

    /* Enable IC diagnostic calculation and logging */
    dwt_configciadiag(DW_CIA_DIAG_LOG_ALL);

    /* Activate reception immediately. */
    dwt_rxenable(DWT_START_RX_IMMEDIATE);

    stdio_write("Waiting for frames\n");

    uint8_t timestamp_buffer[5];
    uint8_t rx_buffer[max_frame_length];
    twr_base_frame_t *rx_frame_pointer;
    int16_t sts_quality_index;

	while (1)
	{
		switch (state) {
		case TWR_SYNC_STATE:
			/* Wait for sync frame (1/4) */
			if (new_frame)
			{
				new_frame = 0;

				if (new_frame_length != sizeof(twr_base_frame_t)+2) {
					stdio_write("RX ERR: wrong frame length\n");
					state = TWR_ERROR;
					continue;
				}

				int sts_quality = dwt_readstsquality(&sts_quality_index);
				if (sts_quality < 0) { /* >= 0 good STS, < 0 bad STS */
					stdio_write("RX ERR: bad STS quality\n");
					state = TWR_ERROR;
					continue;
				}

				dwt_readrxdata(rx_buffer, new_frame_length, 0);
				/* We assume this is a TWR frame, but not necessarily the right one */
				rx_frame_pointer = (twr_base_frame_t *)rx_buffer;

				if (rx_frame_pointer->twr_function_code != 0x20) {  /* ranging init */
					stdio_write("RX ERR: wrong frame (expected sync)\n");
					state = TWR_ERROR;
					continue;
				}

				stdio_write("RX: Sync frame\n");

				/* Initialize the sequence number for this ranging exchange */
				next_sequence_number = rx_frame_pointer->sequence_number + 1;

				/* Send poll frame (2/4) */
				state = TWR_POLL_RESPONSE_STATE; /* Set early to ensure tx done interrupt arrives in new state */
				poll_frame.sequence_number = next_sequence_number++;
				dwt_writetxdata(sizeof(poll_frame), (uint8_t *)&poll_frame, 0);
				dwt_writetxfctrl(sizeof(poll_frame)+2, 0, 1); /* Zero offset in TX buffer, ranging. */
				int r = dwt_starttx(DWT_START_TX_IMMEDIATE | DWT_RESPONSE_EXPECTED);
				if (r != DWT_SUCCESS) {
					stdio_write("TX ERR: could not send poll frame\n");
					state = TWR_ERROR;
					continue;
				}
			}
			break;
		case TWR_POLL_RESPONSE_STATE:
			if (tx_done == 1) {
				tx_done = 2;
				stdio_write("TX: Poll frame\n");
				dwt_readtxtimestamp(timestamp_buffer);
				tx_timestamp_poll = decode_40bit_timestamp(timestamp_buffer);
			}

			/* Wait for response frame (3/4) */
			if (new_frame == 1) {
				new_frame = 0; /* reset */

				if (new_frame_length != sizeof(twr_base_frame_t)+2) {
					stdio_write("RX ERR: wrong frame length\n");
					state = TWR_ERROR;
					continue;
				}

				int sts_quality = dwt_readstsquality(&sts_quality_index);
				if (sts_quality < 0) { /* >= 0 good STS, < 0 bad STS */
					stdio_write("RX ERR: bad STS quality\n");
					state = TWR_ERROR;
					continue;
				}

				dwt_readrxdata(rx_buffer, new_frame_length, 0);
				/* We assume this is a TWR frame, but not necessarily the right one */
				rx_frame_pointer = (twr_base_frame_t *)rx_buffer;

				if (rx_frame_pointer->twr_function_code != 0x10) { /* response */
					stdio_write("RX ERR: wrong frame (expected response)\n");
					state = TWR_ERROR;
					continue;
				}

				if (rx_frame_pointer->sequence_number != next_sequence_number) {
					stdio_write("RX ERR: wrong sequence number\n");
					state = TWR_ERROR;
					continue;
				}

				stdio_write("RX: Response frame\n");
				dwt_readrxtimestamp(timestamp_buffer);
				rx_timestamp_response = decode_40bit_timestamp(timestamp_buffer);

				/* Accept frame and continue ranging */
				next_sequence_number++;
				new_frame = 2;
			}

			if ((tx_done == 2) && (new_frame == 2)) {
				tx_done = 0;
				new_frame = 0;

				/* Send final frame (4/4) */
				final_frame.sequence_number = next_sequence_number++;

				tx_timestamp_final = rx_timestamp_response + round_tx_delay;

				uint64_t Tround1 = rx_timestamp_response - tx_timestamp_poll;
				uint64_t Treply2 = tx_timestamp_final - rx_timestamp_response;

				final_frame.poll_resp_round_time[0] = (uint8_t)Tround1;
				final_frame.poll_resp_round_time[1] = (uint8_t)(Tround1 >> 8);
				final_frame.poll_resp_round_time[2] = (uint8_t)(Tround1 >> 16);
				final_frame.poll_resp_round_time[3] = (uint8_t)(Tround1 >> 32);

				final_frame.resp_final_reply_time[0] = (uint8_t)Treply2;
				final_frame.resp_final_reply_time[1] = (uint8_t)(Treply2 >> 8);
				final_frame.resp_final_reply_time[2] = (uint8_t)(Treply2 >> 16);
				final_frame.resp_final_reply_time[3] = (uint8_t)(Treply2 >> 32);

				dwt_writetxdata(sizeof(final_frame), (uint8_t *)&final_frame, 0);
				dwt_writetxfctrl(sizeof(final_frame)+2, 0, 1); /* Zero offset in TX buffer, ranging. */

				/* Start transmission at the time we embedded into the message */
				state = TWR_FINAL_STATE; /* Set early to ensure tx done interrupt arrives in new state */
				dwt_setdelayedtrxtime(tx_timestamp_final >> 8);
				int r = dwt_starttx(DWT_START_RX_DELAYED | DWT_RESPONSE_EXPECTED);
				if (r != DWT_SUCCESS) {
					stdio_write("TX ERR: delayed send time missed");
					state = TWR_ERROR;
					continue;
				}
			}
			break;
		case TWR_FINAL_STATE:
			if (tx_done == 1) {
				tx_done = 0;
				stdio_write("TX: Final frame\n");
				state = TWR_SYNC_STATE;
			}
			break;
		case TWR_ERROR:
			stdio_write("Ranging error -> reset\n");
			state = TWR_SYNC_STATE;
			Sleep(500);
			dwt_rxenable(DWT_START_RX_IMMEDIATE);
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
	new_frame = 1;
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
