/*
 * ep_coms.h
 *
 *  Created on: Jun 20, 2016
 *      Author: leumas64
 */

#ifndef EP_COMS_H_
#define EP_COMS_H_

#include "ep_common.h"
#include "em_gpio.h"

uint8_t* dataPtr;


void UART_Setup();
void sendSamples(uint8_t* dataPtr, uint32_t dataLen);

#endif /* EP_COMS_H_ */
