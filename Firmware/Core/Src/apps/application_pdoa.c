/*
 * application_pdoa.c
 *
 *  Created on: June 27, 2022
 *      Author: Tobias Margiani
 */

#include "applications.h"

#ifdef APPLICATION_PDOA

#include <stdio.h>
#include <string.h>

#include "deca_regs.h"
#include "deca_spi.h"
#include "port.h"
#include "uart_stdio.h"

#include "application_config.h"
#include "shared_functions.h"

static void rx_ok_cb(const dwt_cb_data_t *cb_data);
static void rx_err_cb(const dwt_cb_data_t *cb_data);

uint8_t new_frame = 0;  /* Flag to indicate a new frame was received from the interrupt */

#define CIR_IP_INDEX		(0)
#define CIR_IP_SAMPLES		(1016)
#define CIR_STS1_INDEX		(1024)
#define CIR_STS1_SAMPLES	(512)
#define CIR_STS2_INDEX		(1536)
#define CIR_STS2_SAMPLES	(512)

uint8_t cir_buffer[1016*6+1];  /* max # samples * 6 bytes/sample + 1 dummy (first byte) */

char print_buffer[64]; /* should have space for two decimal and hex 32-bit numbers and a few more characters */

/**
 * Application entry point.
 */
int dw_main(void)
{
    stdio_write("DW3000 TEST PDOA\n");

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
    dwt_setcallbacks(NULL, rx_ok_cb, rx_err_cb, rx_err_cb, NULL, NULL);

    /* Enable wanted interrupts (RX good frames and RX errors). */
    dwt_setinterrupt(SYS_ENABLE_LO_RXFCG_ENABLE_BIT_MASK | SYS_STATUS_ALL_RX_ERR, 0, DWT_ENABLE_INT);

    /*Clearing the SPI ready interrupt*/
    dwt_write32bitreg(SYS_STATUS_ID, SYS_STATUS_RCINIT_BIT_MASK | SYS_STATUS_SPIRDY_BIT_MASK);

    /* Install DW IC IRQ handler. */
    port_set_dwic_isr(dwt_isr);

    stdio_write("Waiting for frames\n");

    /* Enable IC diagnostic calculation and logging */
    dwt_configciadiag(DW_CIA_DIAG_LOG_ALL);

    /* Activate reception immediately. */
    dwt_rxenable(DWT_START_RX_IMMEDIATE);

    /* CIR sample values */
	int32_t real;
	int32_t imag;

	uint32_t frame_counter = 0;

    /*loop forever receiving frames*/
    while (1)
    {
    	if (new_frame)
    	{
    		stdio_write("Frame Received (v5)\n");
    		new_frame = 0;
    		frame_counter++;

    		snprintf(print_buffer, sizeof(print_buffer), "count: %lu\n", frame_counter);
    		stdio_write(print_buffer);

    		/* Read diagnostics data. */
    		dwt_rxdiag_t rx_diag = {0};
    		dwt_readdiagnostics(&rx_diag);

    		int16_t sts_quality_index;
    		int sts_quality = dwt_readstsquality(&sts_quality_index);

    		/* The first byte of the CIR data a dummy byte and afterwards the CIR is formatted as
    		 * 3 byte signed integers for real and imaginary parts of each sample (i.e. two 24-bit
    		 * signed integers per sample. The byte order is little-endian.
    		 * To use this the bytes have to be converted into a int32_t with the appropriate sign.
    		 */

    		/* diagnostics from received preamble */
    		snprintf(print_buffer, sizeof(print_buffer), "ip_toa: 0x%02X%02X%02X%02X%02X\n",
    		    	rx_diag.ipatovRxTime[4], rx_diag.ipatovRxTime[3], rx_diag.ipatovRxTime[2],
					rx_diag.ipatovRxTime[1], rx_diag.ipatovRxTime[0]);
    		stdio_write(print_buffer);
    		snprintf(print_buffer, sizeof(print_buffer), "ip_toast: 0x%X\n", rx_diag.ipatovRxStatus);
    		stdio_write(print_buffer);
    		snprintf(print_buffer, sizeof(print_buffer), "ip_poa: %u\n", rx_diag.ipatovPOA);
    		stdio_write(print_buffer);
    		/* preamble first path index from diagnostics, but only integer part */
    		snprintf(print_buffer, sizeof(print_buffer), "ip_fp: %u\n", rx_diag.ipatovFpIndex >> 6);
    		stdio_write(print_buffer);

    		/* diagnostics from antenna 1 sts part */
    		snprintf(print_buffer, sizeof(print_buffer), "sts1_toa: 0x%02X%02X%02X%02X%02X\n",
    				rx_diag.stsRxTime[4], rx_diag.stsRxTime[3], rx_diag.stsRxTime[2],
					rx_diag.stsRxTime[1], rx_diag.stsRxTime[0]);
    		stdio_write(print_buffer);
    		snprintf(print_buffer, sizeof(print_buffer), "sts1_toast: 0x%X\n", rx_diag.stsRxStatus);
    		stdio_write(print_buffer);
    		snprintf(print_buffer, sizeof(print_buffer), "sts1_poa: %u\n", rx_diag.stsPOA);
    		stdio_write(print_buffer);
    		snprintf(print_buffer, sizeof(print_buffer), "sts1_fp: %u\n", rx_diag.stsFpIndex >> 6);
    		stdio_write(print_buffer);

    		snprintf(print_buffer, sizeof(print_buffer), "sts2_toa: 0x%02X%02X%02X%02X%02X\n",
    				rx_diag.sts2RxTime[4], rx_diag.sts2RxTime[3], rx_diag.sts2RxTime[2],
					rx_diag.sts2RxTime[1], rx_diag.sts2RxTime[0]);
    		stdio_write(print_buffer);
    		snprintf(print_buffer, sizeof(print_buffer), "sts2_toast: 0x%X\n", rx_diag.sts2RxStatus);
    		stdio_write(print_buffer);
    		snprintf(print_buffer, sizeof(print_buffer), "sts2_poa: %u\n", rx_diag.sts2POA);
    		stdio_write(print_buffer);
    		snprintf(print_buffer, sizeof(print_buffer), "sts2_fp: %u\n", rx_diag.sts2FpIndex >> 6);
    		stdio_write(print_buffer);

    		snprintf(print_buffer, sizeof(print_buffer), "xtaloffset: %d\n", rx_diag.xtalOffset);
    		stdio_write(print_buffer);

    		// user manual p. 183 - tdoa is a 41-bit value with bit 40 being a sign bit
    		snprintf(print_buffer, sizeof(print_buffer), "tdoa: 0x%02X%02X%02X%02X%02X%02X\n",
    				rx_diag.tdoa[5] & 0x01, rx_diag.tdoa[4], rx_diag.tdoa[3], rx_diag.tdoa[2],
					rx_diag.tdoa[1], rx_diag.tdoa[0]);
    		stdio_write(print_buffer);
    		snprintf(print_buffer, sizeof(print_buffer), "pdoa: %d\n", rx_diag.pdoa);
    		stdio_write(print_buffer);
    		snprintf(print_buffer, sizeof(print_buffer), "fpth: %u\n", (dwt_read16bitoffsetreg(0x0C001E, 0) & 0x4000) >> 14);
    		stdio_write(print_buffer);

    		if (sts_quality > 0)
			{
    			snprintf(print_buffer, sizeof(print_buffer), "sts qual: good (%d)\n", sts_quality_index);
    			stdio_write(print_buffer);
			}
    		else
    		{
    			snprintf(print_buffer, sizeof(print_buffer), "sts qual: bad (%d)\n", sts_quality_index);
    			stdio_write(print_buffer);
    		}

    		stdio_write("CIR IP: ");
    		dwt_readaccdata(cir_buffer, CIR_IP_SAMPLES*6+1, CIR_IP_INDEX);
    		for (uint16_t i = 1; i <= CIR_IP_SAMPLES*6; i += 6) {
    			/* combine three bytes into integer */
    			real = decode_24bit(&cir_buffer[i]);
    			imag = decode_24bit(&cir_buffer[i+3]);
    			// output the CIR sample as signed decimal values for real and imaginary part
    			snprintf(print_buffer, sizeof(print_buffer), "%u r %ld i %ld | ", (i-1)/6, real, imag);
    			stdio_write(print_buffer);
    		}
    		stdio_write("END CIR IP\n");

    		stdio_write("CIR STS1: ");
    		dwt_readaccdata(cir_buffer, CIR_STS1_SAMPLES*6+1, CIR_STS1_INDEX);
    		for (uint16_t i = 1; i <= CIR_STS1_SAMPLES*6; i += 6) {
    			/* combine three bytes into integer */
    			real = decode_24bit(&cir_buffer[i]);
    			imag = decode_24bit(&cir_buffer[i+3]);
    			// output the CIR sample as signed decimal values for real and imaginary part
    			snprintf(print_buffer, sizeof(print_buffer), "%u r %ld i %ld | ", (i-1)/6, real, imag);
    			stdio_write(print_buffer);
    		}
    		stdio_write("END CIR STS1\n");

    		stdio_write("CIR STS2: ");
    		dwt_readaccdata(cir_buffer, CIR_STS2_SAMPLES*6+1, CIR_STS2_INDEX);
    		for (uint16_t i = 1; i <= CIR_STS2_SAMPLES*6; i += 6) {
    			/* combine three bytes into integer */
    			real = decode_24bit(&cir_buffer[i]);
    			imag = decode_24bit(&cir_buffer[i+3]);
    			// output the CIR sample as signed decimal values for real and imaginary part
    			snprintf(print_buffer, sizeof(print_buffer), "%u r %ld i %ld | ", (i-1)/6, real, imag);
    			stdio_write(print_buffer);
    		}
    		stdio_write("END CIR STS2\n");

    		dwt_rxenable(DWT_START_RX_IMMEDIATE);
    	}
    }

    return DWT_SUCCESS;
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
	UNUSED(cb_data);
	new_frame = 1;
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
