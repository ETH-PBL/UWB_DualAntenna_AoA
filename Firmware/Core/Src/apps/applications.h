/*
 * applications.h
 *
 *  Created on: 22.06.2022
 *      Author: tobias
 */

#ifndef SRC_APPS_APPLICATIONS_H_
#define SRC_APPS_APPLICATIONS_H_

/* --- uncomment exactly one, each will define dw_main() --- */
//#define APPLICATION_TX            // Demo application transmitter
//#define APPLICATION_RX            // Demo application receiver
//#define APPLICATION_CIR           // Basic CIR readout
//#define APPLICATION_PDOA          // Simple measurement readout (fully text based transmission)
//#define APPLICATION_TWR_TAG       // TWR tag test (double antenna module)
#define APPLICATION_TWR_PDOA_TAG  // TWR tag with full data collection (double antenna module) => used for final measurements
//#define APPLICATION_TWR_ANCHOR    // TWR anchor (single antenna module) => used for final measurements
//#define APPLICATION_TX_TEST       // Transmission debug application

int dw_main(void);

/* Microsecond to device time unit (40-bit timestamps, around 15.65 ps) conversion factor.
 * 1 µs = 499.2 * 128 dtu. */
#define US_TO_DWT_TIME (63898)

#endif /* SRC_APPS_APPLICATIONS_H_ */
