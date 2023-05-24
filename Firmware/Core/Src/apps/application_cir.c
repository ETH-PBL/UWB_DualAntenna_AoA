/*
 * application_cir.c
 *
 *  Created on: June 23, 2022
 *      Author: Tobias Margiani
 */

#include "applications.h"

// Reading the CIR
#ifdef APPLICATION_CIR

#include <stdio.h>
#include <string.h>

#include "deca_regs.h"
#include "deca_spi.h"
#include "port.h"
#include "uart_stdio.h"

#include "application_config.h"

static void rx_ok_cb(const dwt_cb_data_t *cb_data);
static void rx_err_cb(const dwt_cb_data_t *cb_data);

uint8_t new_frame = 0;  /* Flag to indicate a new frame was received from the interrupt */

#define CIR_SAMPLES		(1016)  			/* number of samples in the cir buffer */
#define CIR_BUFFER_LEN 	(CIR_SAMPLES*6+1)	/* # samples * 6 bytes/sample + 1 dummy (first byte) */
uint8_t cir_buffer[CIR_BUFFER_LEN];

char print_buffer[64]; /* should have space for two decimal and hex 32-bit numbers and a few more characters */

/**
 * Application entry point.
 */
int dw_main(void)
{
    stdio_write("DW3000 TEST CIR\n");

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
	uint32_t real;
	uint32_t imag;
	/* number of CIR samples to send over UART */
	const int output_samples = CIR_SAMPLES;
	/* 24th bit a 1 to compute sign extension of the three byte CIR sample values */
	const int32_t m = 1u << 23; // 24th bit is 1

    /*loop forever receiving frames*/
    while (1)
    {
    	if (new_frame)
    	{
    		stdio_write("Frame Received\n");
    		new_frame = 0;

    		dwt_readaccdata(cir_buffer, CIR_BUFFER_LEN, 0);

    		/* Read diagnostics data. */
    		dwt_rxdiag_t rx_diag = {0};
    		dwt_readdiagnostics(&rx_diag);

    		/* The first byte of the CIR data a dummy byte and afterwards the CIR is formatted as
    		 * 3 byte signed integers for real and imaginary parts of each sample (i.e. two 24-bit
    		 * signed integers per sample. The byte order is little-endian.
    		 * To use this the bytes have to be converted into a int32_t with the appropriate sign.
    		 */

    		stdio_write("CIR v3:\n");

    		/* print preamble first path index from diagnostics (only integer part) */
    		uint16_t ip_fp = rx_diag.ipatovFpIndex >> 6;
    		snprintf(print_buffer, sizeof(print_buffer), "ip_fp: %i\n", ip_fp);
    		stdio_write(print_buffer);

    		for (uint16_t i = 1; i <= output_samples*6; i += 6) {
    			/* combine three bytes into integer */
    			real = ((uint32_t)cir_buffer[i]) \
    					+ ((uint32_t)cir_buffer[i+1] << 8) \
						+ ((uint32_t)cir_buffer[i+2] << 16);
    			/* Sign extension:
    			 * a positive number will have a 0 as 24th bit => this does nothing,
    			 * a negative number will have a 1 as 24th bit => xor makes a 0, subtraction
    			 * adds 1s until the end of the number effectively doing sign extension
    			 */
    			real = (real ^ m) - m;

    			/* combine the three imaginary value bytes into a signed integer */
    			imag = ((uint32_t)cir_buffer[i+3]) \
    					+ ((uint32_t)cir_buffer[i+4] << 8) \
						+ ((uint32_t)cir_buffer[i+5] << 16);
    			imag = (imag ^ m) - m;

    			// output the CIR sample as signed decimal values for real and imaginary part
    			snprintf(print_buffer, sizeof(print_buffer), "%d r %li i %li\n", (i-1)/6, real, imag);
    			stdio_write(print_buffer);
    		}

    		stdio_write("cir done");

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
