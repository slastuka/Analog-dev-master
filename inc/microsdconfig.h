/***************************************************************************//**
 * @file
 * @brief Provide MicroSD SPI configuration parameters.
 * @version 3.20.12
 *******************************************************************************
 * @section License
 * <b>(C) Copyright 2014 Silicon Labs, http://www.silabs.com</b>
 *******************************************************************************
 *
 * This file is licensed under the Silabs License Agreement. See the file
 * "Silabs_License_Agreement.txt" for details. Before using this software for
 * any purpose, you must agree to the terms of that agreement.
 *
 ******************************************************************************/

#ifndef __SDCONFIG_H
#define __SDCONFIG_H

/* Don't increase MICROSD_HI_SPI_FREQ beyond 24MHz. */
#define MICROSD_HI_SPI_FREQ     24000000

#define MICROSD_LO_SPI_FREQ     100000
#define MICROSD_USART           USART2
#define MICROSD_LOC             USART_ROUTE_LOCATION_LOC1
#define MICROSD_CMUCLOCK        cmuClock_USART2
#define MICROSD_GPIOPORT        gpioPortB
#define MICROSD_MOSIPIN         3
#define MICROSD_MISOPIN         4
#define MICROSD_CSPIN           6
#define MICROSD_CLKPIN          5



#endif /* __SDCONFIG_H */
