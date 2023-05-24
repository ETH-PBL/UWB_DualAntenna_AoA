/*
 * application_twr_tag.c
 *
 *  Created on: July 05, 2022
 *      Author: Tobias Margiani
 */

#include "applications.h"

#ifdef APPLICATION_TWR_TAG

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
		/* According to ISO/IEC 24730-62:2013 this should be sent by the anchor and end with a short
		 * address temporarily assigned to the tag. We invert the whole tag/anchor process to compute
		 * the ranging on the tag where we have access to the AoA estimation and skip the two bytes
		 * for short address in this message for simplicity. */
};

twr_base_frame_t response_frame = {
		{ 0x41, 0x88 },	/* Frame Control: data frame, short addresses */
		0,				/* Sequence number */
		{ 'X', 'X' },	/* PAN ID */
		{ 'A', 'A' },	/* Destination address */
		{ 'T', 'T' },	/* Source address */
		0x10,			/* Function code: 0x10 activity control */
		/* According to ISO/IEC 24730-62:2013 this frame should have another 3 octets added for an
		 * option code and parameters we skip this here fore simplicity. */
};

const static size_t max_frame_length = sizeof(twr_final_frame_t) + 2;

const static uint64_t round_tx_delay = 10llu*1000llu*US_TO_DWT_TIME;  // reply time (10ms)

uint64_t rx_timestamp_poll = 0;
uint64_t tx_timestamp_response = 0;
uint64_t rx_timestamp_final = 0;

uint8_t next_sequence_number = 0;

enum state_t {
	TWR_SYNC_STATE,
	TWR_POLL_RESPONSE_STATE,
	TWR_FINAL_STATE,
	TWR_ERROR,
};

enum state_t state = TWR_SYNC_STATE;

/* timeout before the ranging exchange will be abandoned and restarted */
const static int ranging_timeout = 2000;

/**
 * Application entry point.
 */
int dw_main(void)
{
    stdio_write("DW3000 TEST TWR Tag\n");

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

    uint8_t timestamp_buffer[5];
    uint8_t rx_buffer[max_frame_length];
    twr_base_frame_t *rx_frame_pointer;
    twr_final_frame_t *rx_final_frame_pointer;
    int16_t sts_quality_index;
    uint32_t last_sync_time = HAL_GetTick();

	while (1)
	{
		/* check timeout and restart ranging if necessary (if there is an overflow in the tick counter the difference
		 * will overflow too and will trigger the timeout, but that shouldn't be much of an issue) */
		if ((HAL_GetTick() - last_sync_time) > ranging_timeout) {
			dwt_forcetrxoff();  // make sure receiver is off after a timeout
			last_sync_time = HAL_GetTick();
			stdio_write("Timeout -> reset\n");
			state = TWR_SYNC_STATE;
			rx_timestamp_poll = 0;
			tx_timestamp_response = 0;
			rx_timestamp_final = 0;
			tx_done = 0;
			rx_done = 0;
		}

		switch (state) {
		case TWR_SYNC_STATE:
			/* Send sync frame (1/4) */
			last_sync_time = HAL_GetTick();
			sync_frame.sequence_number = next_sequence_number++;
			dwt_writetxdata(sizeof(sync_frame), (uint8_t *)&sync_frame, 0);
			dwt_writetxfctrl(sizeof(sync_frame)+2, 0, 1); /* Zero offset in TX buffer, ranging. */

			state = TWR_POLL_RESPONSE_STATE; /* Set early to ensure tx done interrupt arrives in new state */
			int r = dwt_starttx(DWT_START_TX_IMMEDIATE | DWT_RESPONSE_EXPECTED);
			if (r != DWT_SUCCESS) {
				state = TWR_ERROR;
				stdio_write("TX ERR: could not send sync frame");
				continue;
			}
			break;
		case TWR_POLL_RESPONSE_STATE:
			if (tx_done == 1) {
				tx_done = 2;
				stdio_write("TX: Sync frame\n");
			}

			/* Wait for poll frame (2/4) */
			if (rx_done == 1) {
				rx_done = 0; /* reset */

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

				if (rx_frame_pointer->twr_function_code != 0x21) { /* poll */
					stdio_write("RX ERR: wrong frame (expected poll)\n");
					state = TWR_ERROR;
					continue;
				}

				if (rx_frame_pointer->sequence_number != next_sequence_number) {
					stdio_write("RX ERR: wrong sequence number\n");
					state = TWR_ERROR;
					continue;
				}

				stdio_write("RX: Poll frame\n");

				// TODO: Collect PDoA, CIR and RSSI

				dwt_readrxtimestamp(timestamp_buffer);
				rx_timestamp_poll = decode_40bit_timestamp(timestamp_buffer);

				/* Accept frame and continue ranging */
				next_sequence_number++;
				rx_done = 2;
			}

			if ((tx_done == 2) && (rx_done == 2)) {
				tx_done = 0;
				rx_done = 0;

				/* Send response frame (3/4) */
				response_frame.sequence_number = next_sequence_number++;
				dwt_writetxdata(sizeof(response_frame), (uint8_t *)&response_frame, 0);
				dwt_writetxfctrl(sizeof(response_frame)+2, 0, 1); /* Zero offset in TX buffer, ranging. */

				// Send response after a fixed delay
				state = TWR_FINAL_STATE; /* Set early to ensure tx done interrupt arrives in new state */
				dwt_setdelayedtrxtime((uint32_t)((rx_timestamp_poll + round_tx_delay) >> 8));
				int r = dwt_starttx(DWT_START_TX_DELAYED | DWT_RESPONSE_EXPECTED);
				if (r != DWT_SUCCESS) {
					stdio_write("TX ERR: delayed send time missed\n");
					state = TWR_ERROR;
					continue;
				}
			}
			break;
		case TWR_FINAL_STATE:
			if (tx_done == 1) {
				tx_done = 2;
				stdio_write("TX: Response frame\n");
				dwt_readtxtimestamp(timestamp_buffer);
				tx_timestamp_response = decode_40bit_timestamp(timestamp_buffer);
			}

			/* Wait for final frame (4/4) */
			if (rx_done == 1) {
				rx_done = 0; /* reset */

				if (new_frame_length != sizeof(twr_final_frame_t)+2) {
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
				/* For simplicity we assume this is a TWR frame, but not necessarily the right one */
				rx_frame_pointer = (twr_base_frame_t *)rx_buffer;

				if (rx_frame_pointer->twr_function_code != 0x23) { /* final */
					stdio_write("RX ERR: wrong frame (expected final)\n");
					state = TWR_ERROR;
					continue;
				}

				if (rx_frame_pointer->sequence_number != next_sequence_number) {
					stdio_write("RX ERR: wrong sequence number\n");
					state = TWR_ERROR;
					continue;
				}

				stdio_write("RX: Final frame\n");

				// TODO: Collect PDoA, CIR and RSSI

				dwt_readrxtimestamp(timestamp_buffer);
				rx_timestamp_final = decode_40bit_timestamp(timestamp_buffer);

				/* Accept frame continue with ranging */
				next_sequence_number++;
				rx_done = 2;
			}

			if ((tx_done == 2) && (rx_done == 2)) {
				rx_final_frame_pointer = (twr_final_frame_t *)rx_buffer;

				const uint64_t Treply1 = tx_timestamp_response - rx_timestamp_poll;
				const uint64_t Tround2 = rx_timestamp_final - tx_timestamp_response;

				const uint64_t Tround1 = decode_40bit_timestamp(rx_final_frame_pointer->poll_resp_round_time);
				const uint64_t Treply2 = decode_40bit_timestamp(rx_final_frame_pointer->resp_final_reply_time);

				const uint64_t subtraction = (Tround1*Tround2 - Treply1*Treply2);
				const uint64_t denominator = (Tround1 + Tround2 + Treply1 + Treply2);

				// timestamp resolution is approximately u=15.65ps => 1ns = 63.898*u
				// to get ns the division by 63.898 is approximated by an division by 64 using a bit shift
				const float tprop_ns = ((double)subtraction) / (denominator << 6);
				const uint32_t dist_mm = (uint32_t)(tprop_ns*299.792458);  // usint c = 299.7... mm/ns

				snprintf(print_buffer, sizeof(print_buffer), "dist_mm: %lu\n", dist_mm);
				stdio_write(print_buffer);

				/* Begin next ranging exchange */
				tx_done = 0;
				rx_done = 0;
				Sleep(500);
				state = TWR_SYNC_STATE;
			}
			break;
		case TWR_ERROR:
			dwt_forcetrxoff();  // make sure receiver is off after an error
			stdio_write("Ranging error -> reset\n");
			state = TWR_SYNC_STATE;
			Sleep(3000);
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
	/* restart rx on error */
	dwt_forcetrxoff();
	dwt_rxenable(DWT_START_RX_IMMEDIATE);
}

#endif
