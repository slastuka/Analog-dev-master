/**************************************************************************//**
 * @file
 * @brief Epsilometer Project
 * @author Energy Micro AS
 * @version 3.20.2
 ******************************************************************************
 * @section License
 ******************************************************************************/

#include "em_device.h"
#include "em_cmu.h"
#include "em_gpio.h"
#include "stdint.h"

#include "ep_common.h"
#include "ep_analog.h"
#include "ep_coms.h"
#include "stdio.h"
#include "em_usart.h"

#include "em_chip.h"

#define STREAMMODE
//#define STORAGEMODE

/******************************************************************************/
/********************** List of Possible Channels / Sensor ********************/
/******************************************************************************/

epsiSetup boardSetup = EPSI_SETUP_DEFAULT;
epsiSetupPtr boardSetup_ptr = &boardSetup;

SensorSpec fp07_1 = {"T1", AD7124_RESET_DEFAULT, {gpioPortA, 0}};  //PA0
SensorSpec fp07_2 = {"T2", AD7124_RESET_DEFAULT, {gpioPortC, 5}};  //PC5
SensorSpec shr_1  = {"S1", AD7124_RESET_DEFAULT, {gpioPortC, 6}};  //PC6
SensorSpec shr_2  = {"S2", AD7124_RESET_DEFAULT, {gpioPortC, 7}};  //PC7
SensorSpec con_1  = {"C1", AD7124_RESET_DEFAULT, {gpioPortC, 12}}; //PC12
SensorSpec ax     = {"AX", AD7124_RESET_DEFAULT, {gpioPortC, 14}}; //PC14
SensorSpec ay     = {"AY", AD7124_RESET_DEFAULT, {gpioPortC, 15}}; //PC15
SensorSpec az     = {"AZ", AD7124_RESET_DEFAULT, {gpioPortA, 1}};  //PA1

volatile SensorSpec_ptr sensors[8]={&fp07_1, &fp07_2, &shr_1, &shr_2, &con_1, &ax, &ay, &az};

volatile uint32_t pendingSamples = 0; // counter in the background IRQ
volatile uint32_t samplesSent    = 0;    // counter in the foreground IRQ
volatile uint32_t numSync        = 0;    // number of sync signal sent
volatile uint32_t flagSync       = 0;    // flag to reset pending sample in the interrupt.

// Waiting Variable for Clock Setting
volatile uint32_t gulclockset=0;

static const uint32_t bitFieldLookup[8] = {
		0x00000001,
		0x00000003,
		0x00000007,
		0x0000000F,
		0x0000001F,
		0x0000003F,
		0x0000007F,
		0x000000FF};

uint32_t Create_Mask(uint32_t nbits, uint32_t start_pos) {
	return bitFieldLookup[nbits] << start_pos;
}

uint32_t Data_Bit_Shift(uint32_t data, uint32_t start_pos) {
	return data << start_pos;
}

// Register Bit Setting Routine
// data needs to be left shifted to start position
uint32_t Merge_Bits(uint32_t reg, uint32_t data, uint32_t mask) {
	return reg ^ ((reg ^ data) & mask);
}

// Register Bit Setting Routine
void Set_Value_16Bit_Register(uint16_t* reg, uint16_t value, uint16_t nbits, uint16_t start_pos)
{
  int i, j;
  uint16_t temp_value;
  temp_value = value;  						// use 16 bit temp register to shift into
  for (i = 0, j = start_pos; i < nbits; i++, j++){
	*reg &= ~(1 << j); 						//clear each bit in the target field's mask
    *reg |= (((temp_value >> i) & 1) << j); //find byte position then set bit
  }
}

// CMU Interrupt Handler
void CMU_IRQHandler(void)
{
  // Read Interrupt flags
  uint32_t intFlags = CMU_IntGet();

  // Clear interrupt flags register
  CMU_IntClear(CMU_IF_HFXORDY | CMU_IF_HFRCORDY);

    // Check if the Clock Source for External Crystal
  // is Ready or Not?
  if (intFlags & CMU_IF_HFXORDY)
  {
    CMU_ClockSelectSet(cmuClock_HF, cmuSelect_HFXO);
    CMU_OscillatorEnable(cmuOsc_HFRCO, false, false);
        //Inform that We are Done
        gulclockset = 1;
  }
}

/******************************************************************************
 * @brief
 *   Main function.
 * @details
 *   Configures the DVK for SPI
 *****************************************************************************/
int main(void) {

    /* Initialize chip - handle erratas */
    CHIP_Init();

	/* Use 14 MHZ HFRCO as core clock frequency*/
	CMU_ClockSelectSet(cmuClock_HF, cmuSelect_HFRCO);
	CMU_ClockEnable(cmuClock_GPIO, true);
	CMU_ClockEnable(cmuClock_TIMER0, true);
	CMU_ClockEnable(cmuClock_TIMER1, true);
	//TODO Enable WDOG with WDOG_CTRL.CLKSEL and ULFRCO

	// define GPIO pin mode
	GPIO_PinModeSet(gpioPortA, 12,	gpioModePushPull, 1); // PA12 - Enable DC/DC SYNC pin for high current PWM mode
	GPIO_PinModeSet(gpioPortE, 13, gpioModePushPull, 1); // Enable Analog power
	GPIO_PinModeSet(gpioPortF, 3, gpioModePushPull, 1); // Enable 485 Driver
	GPIO_PinModeSet(gpioPortF, 4, gpioModePushPull, 0); // Enable 485 Receiver power
	GPIO_PinModeSet(gpioPortF, 5, gpioModePushPull, 1); // Enable 485 Transmitter
	GPIO_PinModeSet(gpioPortA, 2,	gpioModePushPull, 1); // PA2 in output mode to send the MCLOCK  to ADC
	GPIO_PinModeSet(gpioPortA, 12,	gpioModePushPull, 1); // PA12 to turn on DC/DC high current SYNC mode
	GPIO_PinModeSet(gpioPortB, 7, gpioModePushPull, 1);   // PB7 in output mode to send the SYNC away
	//TODO need to move SYNC pin to alternate location PE10 if the ULFRXO is used for the watchdog

    // Enable the External Oscillator , true for enabling the O and false to not wait
    CMU_OscillatorEnable(cmuOsc_HFXO,true,false);
    //TODO CMU_HFCORECLKDIV=2 to halve the HF core frequency,
    //TODO CMU_HFPERCLKDIV=2 to halve the peripheral clk
    //TODO CMU_CMU_HFCORECLKEN0 to LE interface clock for the watchdog
    //TODO Make sure core clk freq is greater than or equal to the peripheral clk

    // Enable interrupts for HFXORDY
    CMU_IntEnable(CMU_IF_HFXORDY);
    // Enable CMU interrupt vector in NVIC
    NVIC_EnableIRQ(CMU_IRQn);
    // Wait for the HFXO Clock to be Stable - Or infinite in case of error
    while(gulclockset != 1);
	/* set up the 20 MHz clock */
	CMU->CTRL =(CMU->CTRL &~_CMU_CTRL_HFXOMODE_MASK)| CMU_CTRL_HFXOMODE_DIGEXTCLK;
    CMU->CTRL =(CMU->CTRL &~_CMU_CTRL_CLKOUTSEL0_MASK)| CMU_CTRL_CLKOUTSEL0_HFXO;
    CMU->ROUTE =(CMU->ROUTE &~_CMU_ROUTE_CLKOUT0PEN_MASK)| CMU_ROUTE_CLKOUT0PEN;

    /* Watchdog Setup */
    /* WDOGtimeout = (2^(3+PERSEL) + 1)/f */
    //TODO WDOG_CRTL EN=1
    //TODO if(WDOG_SYNCBUSY_CTRL)  // do not write watchdog registers until it is enabled (SYNCBUSY low)
    //TODO 		WDOG_CRTL PERSEL= 15 // 256000 clk cycles before watchdog timeout
    //TODO 		WDOG_CTRL DEBUGRUN=0
    //TODO 		WDOG_CMD CLEAR=1

    for (int i=0;i<8;i++){
    	AD7124_ChipSelect(sensors[i], LHI); // bring them all high
    }

    GPIO_PinModeSet(gpioPortC, 13, gpioModePushPull, 1); // and the ADA2200

	#ifdef STREAMMODE
		UART_Setup();
	#endif

	SPI_Init();     // Initialize the SPI interface for the Analog front end
	AD7124_Reset(); // Place devices into a known state

	setupGpioInt(); // Setup GPIO Interrupt but leave disabled

	AD7124 COMMON_SETUP = AD7124_RESET_DEFAULT;
	COMMON_SETUP.ERROR_EN = AD7124_ERREN_REG_REF_DET_ERR_EN | AD7124_ERREN_REG_SPI_IGNORE_ERR_EN;

	Set_Value_16Bit_Register(&COMMON_SETUP.CONFIG_0, AD7124_CONFIG_BIPOLAR_ENABLE, AD7124_CONFIG_BIPOLAR_NUM_BITS, AD7124_CONFIG_BIPOLAR_START_POSITION);
	Set_Value_16Bit_Register(&COMMON_SETUP.CONFIG_0, AD7124_CONFIG_BURNOUT_OFF, AD7124_CONFIG_BURNOUT_NUM_BITS, AD7124_CONFIG_BURNOUT_START_POSITION);
	Set_Value_16Bit_Register(&COMMON_SETUP.CONFIG_0, AD7124_CONFIG_INT_REF, AD7124_CONFIG_REF_SEL_NUM_BITS, AD7124_CONFIG_REF_SEL_START_POSITION);
	Set_Value_16Bit_Register(&COMMON_SETUP.CONFIG_0, AD7124_CONFIG_PGA_GAIN_1, AD7124_CONFIG_PGA_NUM_BITS, AD7124_CONFIG_PGA_START_POSITION);

	Set_Value_16Bit_Register(&COMMON_SETUP.ADC_CONTROL, AD7124_CTRL_CS_EN_ENABLE, AD7124_CTRL_CS_EN_NUM_BITS, AD7124_CTRL_CS_EN_START_POSITION);
	Set_Value_16Bit_Register(&COMMON_SETUP.ADC_CONTROL, AD7124_CTRL_POWER_MODE_HIGH, AD7124_CTRL_POWER_MODE_NUM_BITS, AD7124_CTRL_POWER_MODE_START_POSITION);
	Set_Value_16Bit_Register(&COMMON_SETUP.ADC_CONTROL, AD7124_CTRL_MODE_CONTINUOUS, AD7124_CTRL_MODE_NUM_BITS, AD7124_CTRL_MODE_START_POSITION);
	Set_Value_16Bit_Register(&COMMON_SETUP.ADC_CONTROL, AD7124_CTRL_REF_EN_ENABLE, AD7124_CTRL_REF_EN_NUM_BITS, AD7124_CTRL_REF_EN_START_POSITION);
	Set_Value_16Bit_Register(&COMMON_SETUP.ADC_CONTROL, AD7124_CTRL_CLKSEL_EXT, AD7124_CTRL_CLKSEL_NUM_BITS, AD7124_CTRL_CLKSEL_START_POSITION);

	COMMON_SETUP.FILTER_0 = 0x06001E; // 0x1E for 640hz from sinc4 filter, with LP @ 147.2Hz
//	COMMON_SETUP.FILTER_0 = 0x06003C; // 0x3C for 320hz from sinc4 filter, with LP @ 73.6Hz
//	COMMON_SETUP.FILTER_0 = 0x060078; // 0x78 for 160hz from sinc4 filter, with LP @ 36.8Hz
//	COMMON_SETUP.FILTER_0 = 0x800001; // 0x01 for 505hz from sinc4+sinc1 filter, with LP @ 222Hz
//	COMMON_SETUP.FILTER_0 = 0x01001E; // 0x1E for 160hz from sinc4 filter + Fast Settling, with LP @ 147Hz
//	COMMON_SETUP.FILTER_0 = 0x01000E; // 0x0E for 320hz from sinc4 filter + Fast Settling, with LP @ 294Hz

	AD7124 TEMP_SETUP = COMMON_SETUP;
	Set_Value_16Bit_Register(&TEMP_SETUP.CHANNEL_0, AD7124_CH_ENABLE, AD7124_CH_EN_NUM_BITS, AD7124_CH_EN_START_POSITION);
	Set_Value_16Bit_Register(&TEMP_SETUP.CHANNEL_0, AD7124_CH_AINP_AIN0, AD7124_CH_AINP_NUM_BITS, AD7124_CH_AINP_START_POSITION);
	Set_Value_16Bit_Register(&TEMP_SETUP.CHANNEL_0, AD7124_CH_AINM_AIN1, AD7124_CH_AINM_NUM_BITS, AD7124_CH_AINM_START_POSITION);

	AD7124 SHR_SETUP = COMMON_SETUP;
	Set_Value_16Bit_Register(&SHR_SETUP.CHANNEL_0, AD7124_CH_ENABLE, AD7124_CH_EN_NUM_BITS, AD7124_CH_EN_START_POSITION);
	Set_Value_16Bit_Register(&SHR_SETUP.CHANNEL_0, AD7124_CH_AINP_AIN0, AD7124_CH_AINP_NUM_BITS, AD7124_CH_AINP_START_POSITION);
	Set_Value_16Bit_Register(&SHR_SETUP.CHANNEL_0, AD7124_CH_AINM_AIN1, AD7124_CH_AINM_NUM_BITS, AD7124_CH_AINM_START_POSITION);
	Set_Value_16Bit_Register(&SHR_SETUP.CONFIG_0, AD7124_CONFIG_BIPOLAR_ENABLE, AD7124_CONFIG_BIPOLAR_NUM_BITS, AD7124_CONFIG_BIPOLAR_START_POSITION);
//	Set_Value_16Bit_Register(&SHR_SETUP.CONFIG_0, AD7124_CONFIG_AIN_BUFP_ON, AD7124_CONFIG_AIN_BUFP_NUM_BITS, AD7124_CONFIG_AIN_BUFP_START_POSITION);
//	Set_Value_16Bit_Register(&SHR_SETUP.CONFIG_0, AD7124_CONFIG_AIN_BUFM_ON, AD7124_CONFIG_AIN_BUFM_NUM_BITS, AD7124_CONFIG_AIN_BUFM_START_POSITION);
	Set_Value_16Bit_Register(&SHR_SETUP.CONFIG_0, AD7124_CONFIG_PGA_GAIN_2, AD7124_CONFIG_PGA_NUM_BITS, AD7124_CONFIG_PGA_START_POSITION);

	/*	SHR_SETUP.CONFIG_0 = Merge_Bits(SHR_SETUP.CONFIG_0, \
				Data_Bit_Shift(AD7124_CONFIG_BIPOLAR_DISABLE, AD7124_CONFIG_BIPOLAR_START_POSITION), \
				Create_Mask(AD7124_CONFIG_BIPOLAR_NUM_BITS, AD7124_CONFIG_BIPOLAR_START_POSITION)); This is sample code */

	AD7124 ACCL_SETUP = COMMON_SETUP;
	Set_Value_16Bit_Register(&ACCL_SETUP.CHANNEL_0, AD7124_CH_ENABLE, AD7124_CH_EN_NUM_BITS, AD7124_CH_EN_START_POSITION);
	Set_Value_16Bit_Register(&ACCL_SETUP.CHANNEL_0, AD7124_CH_AINP_AIN0, AD7124_CH_AINP_NUM_BITS, AD7124_CH_AINP_START_POSITION);
	Set_Value_16Bit_Register(&ACCL_SETUP.CHANNEL_0, AD7124_CH_AINM_AIN1, AD7124_CH_AINM_NUM_BITS, AD7124_CH_AINM_START_POSITION);
	//TODO: GND reference to refin1-
	Set_Value_16Bit_Register(&ACCL_SETUP.CONFIG_0, AD7124_CONFIG_BIPOLAR_ENABLE, AD7124_CONFIG_BIPOLAR_NUM_BITS, AD7124_CONFIG_BIPOLAR_START_POSITION);

	AD7124 COND_SETUP = COMMON_SETUP;
	Set_Value_16Bit_Register(&COND_SETUP.CHANNEL_0, AD7124_CH_ENABLE, AD7124_CH_EN_NUM_BITS, AD7124_CH_EN_START_POSITION);
	Set_Value_16Bit_Register(&COND_SETUP.CHANNEL_0, AD7124_CH_AINP_AIN0, AD7124_CH_AINP_NUM_BITS, AD7124_CH_AINP_START_POSITION);
	Set_Value_16Bit_Register(&COND_SETUP.CHANNEL_0, AD7124_CH_AINM_AIN1, AD7124_CH_AINM_NUM_BITS, AD7124_CH_AINM_START_POSITION);

	AD7124 OFF_SETUP = AD7124_RESET_DEFAULT;
	Set_Value_16Bit_Register(&OFF_SETUP.ADC_CONTROL, AD7124_CTRL_MODE_POWER_DOWN, AD7124_CTRL_POWER_MODE_NUM_BITS, AD7124_CTRL_POWER_MODE_START_POSITION);


	//set up timer MCLOCK and SYNC
	//configure TIMER0 create and output a
	/* Configure TIMER */
	//TIMER_Init(TIMER0, &timerInit);
	TIMER0->CTRL =(TIMER0->CTRL &~_TIMER_CTRL_CLKSEL_MASK)| TIMER_CTRL_CLKSEL_PRESCHFPERCLK;
	// DIV 1 core clock divided by 1
	TIMER0->CTRL =(TIMER0->CTRL &~_TIMER_CTRL_PRESC_MASK)| TIMER_CTRL_PRESC_DIV1;
	// TIMER Up Count
	TIMER0->CTRL =(TIMER0->CTRL &~_TIMER_CTRL_MODE_MASK)| TIMER_CTRL_MODE_UP;
	// Define the top value
	TIMER0->TOP =(TIMER0->TOP &~_TIMER_TOP_MASK)| TIMER_TOP_TOP_DEFAULT;
    // 2 * 16 cycles 20MHz = 625 kHz -> ok for ADC
	TIMER0->TOP =(TIMER0->TOP &~_TIMER_TOP_MASK)| 0xf;
    // Setup the CC as output compare to generate a sync signal
	TIMER0->CC[2].CTRL =(TIMER0->CC[2].CTRL &~_TIMER_CC_CTRL_MODE_MASK)| TIMER_CC_CTRL_MODE_OUTPUTCOMPARE;
	// Toggle the output signal on compare
	TIMER0->CC[2].CTRL =(TIMER0->CC[2].CTRL &~_TIMER_CC_CTRL_CMOA_MASK)| TIMER_CC_CTRL_CMOA_TOGGLE;
	//
	TIMER0->CC[2].CCV =(TIMER0->CC[2].CCV &~_TIMER_CC_CCV_CCV_MASK)| TIMER_CC_CCV_CCV_DEFAULT;
	// Invert the output for the SYNC signal
	TIMER0->CC[2].CTRL =(TIMER0->CC[2].CTRL &~_TIMER_CC_CTRL_OUTINV_MASK)| TIMER_CC_CTRL_OUTINV;
	// setup CCV to trigger TIMER1 with 90 deg phase shift -> 90 deg shift = half of a period = 0xf/2 =0x8
	TIMER0->CC[2].CCV =(TIMER0->CC[2].CCV &~_TIMER_CC_CCV_CCV_MASK)| 0x6;  // control the phase of TIMER0
	// Timer0 CC2 is in location0 which is at Port A Pin 2 (Pin3 /MCLK)
	TIMER0->ROUTE =(TIMER0->ROUTE &~_TIMER_ROUTE_LOCATION_MASK)| TIMER_ROUTE_LOCATION_LOC0;
	TIMER0->ROUTE =(TIMER0->ROUTE &~_TIMER_ROUTE_MASK)| TIMER_ROUTE_CC2PEN;

	//configure TIMER1. TIMER1 is SYNC for the ADC
	//Count up like in Timer0 to set SYNC freq
	TIMER1->CTRL =(TIMER1->CTRL &~_TIMER_CTRL_MODE_MASK)| TIMER_CTRL_MODE_UP;
	//One shot pulse for /SYNC
	TIMER1->CTRL =(TIMER1->CTRL &~_TIMER_CTRL_OSMEN_MASK)| TIMER_CTRL_OSMEN;
	//Timer starts on trigger from another timer
    TIMER1->CTRL =(TIMER1->CTRL &~_TIMER_CTRL_SYNC_MASK)| TIMER_CTRL_SYNC_DEFAULT;
	//Timer clocked by over or underflow in the lower numbered neighbor timer
    TIMER1->CTRL =(TIMER1->CTRL &~_TIMER_CTRL_CLKSEL_MASK)| TIMER_CTRL_CLKSEL_TIMEROUF;
	TIMER1->CTRL =(TIMER1->CTRL &~_TIMER_CTRL_OSMEN_MASK)| TIMER_CTRL_OSMEN;
	//Set output compare CC[0] register to define the duration of the one-shot pulse
	TIMER1->CC[0].CTRL =(TIMER1->CC[0].CTRL &~_TIMER_CC_CTRL_MODE_MASK)| TIMER_CC_CTRL_MODE_OUTPUTCOMPARE;
	TIMER1->CC[0].CTRL =(TIMER1->CC[0].CTRL &~_TIMER_CC_CTRL_OUTINV_MASK)| TIMER_CC_CTRL_OUTINV;
	//Compare Match output compare action set to toggle the Timer1.CC0 output signal
	TIMER1->CC[0].CTRL =(TIMER1->CC[0].CTRL &~_TIMER_CC_CTRL_CMOA_MASK)| TIMER_CC_CTRL_CMOA_TOGGLE;
	//Select Pin (as opposed to PRS input for Output Compare
	TIMER1->CC[0].CTRL =(TIMER1->CC[0].CTRL &~_TIMER_CC_CTRL_INSEL_MASK)| TIMER_CC_CTRL_INSEL_PIN;
	// set up top as 13 cycle of TIMER0
	TIMER1->TOP =(TIMER1->TOP &~_TIMER_TOP_MASK)| 0x13;
	TIMER1->ROUTE =(TIMER1->ROUTE &~_TIMER_ROUTE_MASK) | TIMER_ROUTE_CC0PEN;
	TIMER1->ROUTE =(TIMER1->ROUTE &~_TIMER_ROUTE_LOCATION_MASK)| TIMER_ROUTE_LOCATION_LOC3;


	uint32_t delay = 0xFFFF;
	while (delay--); // Delay after power on to ensure registers are ready to write

	AD7124_ConfigureDevice(&fp07_1, TEMP_SETUP);
	AD7124_ConfigureDevice(&fp07_2, TEMP_SETUP);
	AD7124_ConfigureDevice(&shr_1, SHR_SETUP);
	AD7124_ConfigureDevice(&shr_2, SHR_SETUP);
	AD7124_ConfigureDevice(&con_1, COND_SETUP);
	AD7124_ConfigureDevice(&ax, ACCL_SETUP);
	AD7124_ConfigureDevice(&ay, ACCL_SETUP);
	AD7124_ConfigureDevice(&az, ACCL_SETUP);

	/****************************************************************
	 * Primitive Sampling routine
	 * ***************************************************************/

	//int pendingTimeout = 0;
	AD7124_StartConversion(sensors[boardSetup_ptr->numSensor]);

	/****************************************************************
	 * Primitive Sampling routine
	 ****************************************************************/
	while (1) {
	}// end while loop

}// end main
