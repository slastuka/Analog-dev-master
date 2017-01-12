/*
 * proto Epsilometer_coms.c
 *
 *  Created on: May 24, 2016
 *      Author: leumas64
 */

#include "ep_coms.h"
#include "em_usart.h"
#include "em_cmu.h"
#include "efm32wg_uart.h"



/***************************************************************************//**
 * @brief Set up the USART1 communication port
 * @Detail
 * @return
 * @Note  written by leumas64
*******************************************************************************/

void UART_Setup() {
	/*UART1 shit*/
	CMU_ClockEnable(cmuClock_USART1, true); // Enable clock for USART1 module
	GPIO_PinModeSet(gpioPortD, 7, gpioModePushPull, 1); // TX
	GPIO_PinModeSet(gpioPortD, 6, gpioModeInput, 0); // RX

	USART_InitAsync_TypeDef usartInitUSART1 = {
		.enable = usartDisable, 					// Initially disabled
		.refFreq = 0,								// configured reference frequency
		.baudrate = boardSetup_ptr->usartBaudrate, 				    // Baud rate defined in common.h
		.oversampling = usartOVS16, 				// overSampling rate x16
		.databits = USART_FRAME_DATABITS_EIGHT, 	// 8-bit frames
		.parity = USART_FRAME_PARITY_NONE,			// parity - none
		.stopbits = USART_FRAME_STOPBITS_ONE,		// 1 stop bit
	};

	/*Initialize UART registers*/
	USART_InitAsync(USART1, &usartInitUSART1);

	USART1 -> ROUTE = USART_ROUTE_RXPEN | USART_ROUTE_TXPEN
			| USART_ROUTE_LOCATION_LOC2;

	/* Inform NVIC of IRQ changes*/
	NVIC_ClearPendingIRQ(USART1_TX_IRQn);
	NVIC_EnableIRQ(USART1_TX_IRQn);

	USART_Enable(USART1, usartEnable);
}

/***************************************************************************//**
 * @brief send sample through TX
 * @Detail send dataLen times 8 bit through TX
 * @return
 * @Note likely written by leumas64
*******************************************************************************/

//TODO all of this should be made into a function not an interrupt routine
void sendSamples(uint8_t* dataPtr, uint32_t dataLen) {
	/* Check TX buffer level status */
		for (int i=0; i<dataLen; i++){
			/* Transmit pending character */
			// USART_TX check if TX buffer is empty
			// condition on i because we only want to sent 3 bytes
			USART_Tx(USART1, dataPtr);
			dataPtr++;
		}
}



