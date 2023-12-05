/*****************************************************************************
* bsp.c for Lab2A of ECE 153a at UCSB
* Date of the Last Update:  October 27,2019
*****************************************************************************/

/**/
#include "qpn_port.h"
#include "bsp.h"
#include "xintc.h"
#include "xil_exception.h"
#include "xtmrctr_l.h"
#include "xtmrctr.h"
#include "xspi.h"
#include "xspi_l.h"
#include "lcd.h"


/*****************************/

/* Define all variables and Gpio objects here  */

#define GPIO_CHANNEL1 1
#define DELAY(X) for(volatile int z=0;z<X;z++);

static XGpio Gpio;
static XGpio EncoderGpio;
static XIntc intc;
static XTmrCtr tmrctr;

static XGpio BtnGPIO;
static XGpio dc;
static XSpi spi;
static int BufferHigh[240][320];
static int BufferLow[240][320];

/******** For debouncing rotary encoder*/
volatile int dir = -1;
volatile int final = -1;
volatile int btnpress = 0;
volatile int finalState = 3;
/***********************/

volatile int mode = -1;
volatile int ct = 0;
volatile int sleep = 0;

XSpi_Config *spiConfig;	/* Pointer to Configuration data */
XGpio EncoderGpio;

u32 controlReg;

static int volume = 50;
/*
void timer_handler(){
	Xuint32 ControlStatusReg;
	ControlStatusReg = XTimerCtr_ReadReg(tmrctr.BaseAddress,0,XTC_TCSR_OFFSET);
	ct++;
	XTmrCtr_WriteReg(tmrctr.BaseAddress, 0 , XTC_TCSR_OFFSET, ControlStatusReg | XTC_CSR_INT_OCCURED_MASK);
}*/


/*..........................................................................*/
void BSP_init(void) {
	/* Setup LED's, etc */
	/* Setup interrupts and reference to interrupt handler function(s)  */

	/*
	 * Initialize the interrupt controller driver so that it's ready to use.
	 * specify the device ID that was generated in xparameters.h
	 *
	 * Initialize GPIO and connect the interrupt controller to the GPIO.
	 *
	 */
	XTmrCtr_Initialize(&tmrctr, XPAR_MICROBLAZE_0_AXI_INTC_DEVICE_ID);
	XTmrCtr_SetOptions(&tmrctr, 0, XTC_INT_MODE_OPTION | XTC_AUTO_RELOAD_OPTION);
	XTmrCtr_SetResetValue(&tmrctr, 0, 0xFFFFFFFF - 200000);
	XTmrCtr_Start(&tmrctr, 0);

	XGpio_Initialize(&EncoderGpio, XPAR_AXI_GPIO_ENCODER_DEVICE_ID);
	XIntc_Initialize(&intc, XPAR_INTC_0_DEVICE_ID);
	XGpio_Initialize(&dc, XPAR_SPI_DC_DEVICE_ID);
	XGpio_Initialize(&BtnGPIO, XPAR_AXI_GPIO_BTN_DEVICE_ID);
	XGpio_SetDataDirection(&dc, 1, 0x0);
	XIntc_Connect(&intc, XPAR_MICROBLAZE_0_AXI_INTC_AXI_GPIO_ENCODER_IP2INTC_IRPT_INTR,
			(XInterruptHandler) encoder_handler, &EncoderGpio);
	XIntc_Connect(&intc, XPAR_MICROBLAZE_0_AXI_INTC_AXI_GPIO_BTN_IP2INTC_IRPT_INTR,
		(XInterruptHandler) button_handler, &BtnGPIO);
	//XIntc_Connect(&intc, XPAR_MICROBLAZE_0_AXI_INTC_AXI_TIMER_0_INTERRUPT_INTR,
	//	(XInterruptHandler) timer_handler, &tmrctr);
	spiConfig = XSpi_LookupConfig(XPAR_SPI_DEVICE_ID);
	XSpi_CfgInitialize(&spi, spiConfig, spiConfig->BaseAddress);

	XSpi_Reset(&spi);

	controlReg = XSpi_GetControlReg(&spi);
	XSpi_SetControlReg(&spi,
			(controlReg | XSP_CR_ENABLE_MASK | XSP_CR_MASTER_MODE_MASK) &
			(~XSP_CR_TRANS_INHIBIT_MASK));

	XSpi_SetSlaveSelectReg(&spi, ~0x01);
}
/*..........................................................................*/
void QF_onStartup(void) {                 /* entered with interrupts locked */
/* Enable interrupts */
	//xil_printf("\n\rQF_onStartup\n"); // Comment out once you are in your complete program
	// Press Knob
	// Enable interrupt controller
	XIntc_Enable(&intc, XPAR_MICROBLAZE_0_AXI_INTC_AXI_GPIO_ENCODER_IP2INTC_IRPT_INTR);
	XIntc_Enable(&intc, XPAR_MICROBLAZE_0_AXI_INTC_AXI_GPIO_BTN_IP2INTC_IRPT_INTR);
	XIntc_Enable(&intc, XPAR_MICROBLAZE_0_AXI_INTC_AXI_TIMER_0_INTERRUPT_INTR);
	microblaze_enable_interrupts();
	// Start interrupt controller
	XIntc_Start(&intc, XIN_REAL_MODE);
	// register handler with Microblaze
	// Global enable of interrupt
	// Enable interrupt on the GPIO
	XGpio_InterruptEnable(&EncoderGpio, 1);
	XGpio_InterruptGlobalEnable(&EncoderGpio);
	XGpio_InterruptEnable(&BtnGPIO, 1);
	XGpio_InterruptGlobalEnable(&BtnGPIO);
	
	// set up Buffer array for the background
	int x, y = 0;
	for(int i = 0; i < 240; i++) {
		x = i % 40;
		for (int j = 0; j < 320; j++) {
			y = j % 40;
			if((y>(2*x-40) && y<(-2*x+40+80*(x/40))) || (y<(2*x-40) && y>(-2*x+40+80*(x/40)))){
				BufferHigh[i][j] = (0 & 0x0F8) | 0 >> 5;
				BufferLow[i][j] = (0 & 0x1C) << 3 | 255 >> 3;
			}
			else{
				BufferHigh[i][j] = (255 & 0x0F8) | 255 >> 5;
				BufferLow[i][j] = (255 & 0x1C) << 3 | 255 >> 3;
			}
		}
	}

	// initialize LCD with buffer array
	initLCD();
	clrScr();
	for(int j=0; j<320; j++) {
		for(int i=0; i<240; i++) {
			setXY(i, j, i, j);
			LCD_Write_DATA(BufferHigh[i][j]);
			LCD_Write_DATA(BufferLow[i][j]);
			clrXY();
		}
	}
	int pastX = 40+(8*volume)/5;
	setColor(255, 0, 0);
	fillRect(40, 100, pastX, 120);
	setFont(SmallFont);
	ct = 0;
	/*
	// grand loop from lab 2b
	while(1){
		if(ct==1000){
			for(int j=100; j<151; j++) {
				for(int i=40; i<=200; i++) {
					setXY(i, j, i, j);
					LCD_Write_DATA(BufferHigh[i][j]);
					LCD_Write_DATA(BufferLow[i][j]);
					clrXY();
				}
			}
			sleep = 1;
		}
		if(mode==1){
			setColor(255, 255, 255);
			fillRect(80, 130, 160, 150);
			lcdPrint("Mode: I", 91, 135);
		}
		else if(mode==2){
			setColor(255, 255, 255);
			fillRect(80, 130, 160, 150);
			lcdPrint("Mode: II", 88, 135);
		}
		else if(mode==4){
			setColor(255, 255, 255);
			fillRect(80, 130, 160, 150);
			lcdPrint("Mode: III", 85, 135);
		}
		else if(mode==8){
			setColor(255, 255, 255);
			fillRect(80, 130, 160, 150);
			lcdPrint("Mode: IV", 88, 135);
		}
		setColor(255, 0, 0);
		if(btnpress==1){
			for(int j=100; j<121; j++) {
				for(int i=40; i<=pastX; i++) {
					setXY(i, j, i, j);
					LCD_Write_DATA(BufferHigh[i][j]);
					LCD_Write_DATA(BufferLow[i][j]);
					clrXY();
				}
			}
			volume = 0;
			pastX = 40;
		}
		if(final==1){
			volume++;
			if(volume>100) volume = 100;
			if(sleep){
				pastX = 40;
				sleep = 0;
			}
			fillRect(pastX, 100, 40+(8*volume)/5, 120);
			pastX = 40+(8*volume)/5;
		}
		else if(final==0){
			volume--;
			if(volume<0) volume = 0;
			if(sleep){
				fillRect(40, 100, 40+(8*volume)/5, 120);
				pastX = 40+(8*volume)/5;
				sleep = 0;
			}
			for(int j=100; j<121; j++) {
				for(int i=pastX; i>=40+(8*volume)/5; i--) {
					setXY(i, j, i, j);
					LCD_Write_DATA(BufferHigh[i][j]);
					LCD_Write_DATA(BufferLow[i][j]);
					clrXY();
				}
			}
			pastX = 40+(8*volume)/5;
		}
		mode = -1;
		btnpress = 0;
		final = -1;
		DELAY(100);
	}
	*/
	// Variables for reading Microblaze registers to debug your interrupts.
//	{
//		u32 axi_ISR =  Xil_In32(intcPress.BaseAddress + XIN_ISR_OFFSET);
//		u32 axi_IPR =  Xil_In32(intcPress.BaseAddress + XIN_IPR_OFFSET);
//		u32 axi_IER =  Xil_In32(intcPress.BaseAddress + XIN_IER_OFFSET);
//		u32 axi_IAR =  Xil_In32(intcPress.BaseAddress + XIN_IAR_OFFSET);
//		u32 axi_SIE =  Xil_In32(intcPress.BaseAddress + XIN_SIE_OFFSET);
//		u32 axi_CIE =  Xil_In32(intcPress.BaseAddress + XIN_CIE_OFFSET);
//		u32 axi_IVR =  Xil_In32(intcPress.BaseAddress + XIN_IVR_OFFSET);
//		u32 axi_MER =  Xil_In32(intcPress.BaseAddress + XIN_MER_OFFSET);
//		u32 axi_IMR =  Xil_In32(intcPress.BaseAddress + XIN_IMR_OFFSET);
//		u32 axi_ILR =  Xil_In32(intcPress.BaseAddress + XIN_ILR_OFFSET) ;
//		u32 axi_IVAR = Xil_In32(intcPress.BaseAddress + XIN_IVAR_OFFSET);
//		u32 gpioTestIER  = Xil_In32(sw_Gpio.BaseAddress + XGPIO_IER_OFFSET);
//		u32 gpioTestISR  = Xil_In32(sw_Gpio.BaseAddress  + XGPIO_ISR_OFFSET ) & 0x00000003; // & 0xMASK
//		u32 gpioTestGIER = Xil_In32(sw_Gpio.BaseAddress  + XGPIO_GIE_OFFSET ) & 0x80000000; // & 0xMASK
//	}
}


void QF_onIdle(void) {        /* entered with interrupts locked */

    QF_INT_UNLOCK();                       /* unlock interrupts */

    {
    	// Write code to increment your interrupt counter here.
    	// QActive_postISR((QActive *)&AO_Lab2A, ENCODER_DOWN); is used to post an event to your FSM



// 			Useful for Debugging, and understanding your Microblaze registers.
//    		u32 axi_ISR =  Xil_In32(intcPress.BaseAddress + XIN_ISR_OFFSET);
//    	    u32 axi_IPR =  Xil_In32(intcPress.BaseAddress + XIN_IPR_OFFSET);
//    	    u32 axi_IER =  Xil_In32(intcPress.BaseAddress + XIN_IER_OFFSET);
//
//    	    u32 axi_IAR =  Xil_In32(intcPress.BaseAddress + XIN_IAR_OFFSET);
//    	    u32 axi_SIE =  Xil_In32(intcPress.BaseAddress + XIN_SIE_OFFSET);
//    	    u32 axi_CIE =  Xil_In32(intcPress.BaseAddress + XIN_CIE_OFFSET);
//    	    u32 axi_IVR =  Xil_In32(intcPress.BaseAddress + XIN_IVR_OFFSET);
//    	    u32 axi_MER =  Xil_In32(intcPress.BaseAddress + XIN_MER_OFFSET);
//    	    u32 axi_IMR =  Xil_In32(intcPress.BaseAddress + XIN_IMR_OFFSET);
//    	    u32 axi_ILR =  Xil_In32(intcPress.BaseAddress + XIN_ILR_OFFSET) ;
//    	    u32 axi_IVAR = Xil_In32(intcPress.BaseAddress + XIN_IVAR_OFFSET);
//
//    	    // Expect to see 0x00000001
//    	    u32 gpioTestIER  = Xil_In32(sw_Gpio.BaseAddress + XGPIO_IER_OFFSET);
//    	    // Expect to see 0x00000001
//    	    u32 gpioTestISR  = Xil_In32(sw_Gpio.BaseAddress  + XGPIO_ISR_OFFSET ) & 0x00000003;
//
//    	    // Expect to see 0x80000000 in GIER
//    		u32 gpioTestGIER = Xil_In32(sw_Gpio.BaseAddress  + XGPIO_GIE_OFFSET ) & 0x80000000;


    }
}

/* Q_onAssert is called only when the program encounters an error*/
/*..........................................................................*/
void Q_onAssert(char const Q_ROM * const Q_ROM_VAR file, int line) {
    (void)file;                                   /* name of the file that throws the error */
    (void)line;                                   /* line of the code that throws the error */
    QF_INT_LOCK();
    printDebugLog();
    for (;;) {
    }
}

/* Interrupt handlers for buttons, encoder, and timer*/
/*..........................................................................*/
void timer_handler(){
	Xuint32 ControlStatusReg;
	ControlStatusReg = XTimerCtr_ReadReg(tmrctr.BaseAddress,0,XTC_TCSR_OFFSET);
	ct++;
	XTmrCtr_WriteReg(tmrctr.BaseAddress, 0 , XTC_TCSR_OFFSET, ControlStatusReg | XTC_CSR_INT_OCCURED_MASK);
}

void button_handler(void *CallbackRef){
	XGpio *GpioPtr = (XGpio *)CallbackRef;
	unsigned int btn = XGpio_DiscreteRead(&BtnGPIO, 1);//1 up 2 left 4 right 8 down
	ct = 0;
	if(btn==1){
		mode = 1;
	}
	else if(btn==2){
		mode = 2;
	}
	else if(btn==4){
		mode = 4;
	}
	else if(btn==8){
		mode = 8;
	}
	XGpio_InterruptClear(GpioPtr, 1);
}

void encoder_handler(void *CallbackRef){
	XGpio *GpioPtr = (XGpio *)CallbackRef;
	unsigned int encoder = XGpio_DiscreteRead(&EncoderGpio, 1);//if clockwise, 1023, else 2013, push=73
	// 0 is clockwise, 1 is counterclockwise
	ct = 0;
	switch (encoder) {
	    case 0:
	    	finalState = 0;
	        break;
	    case 1:
	    	if (dir == -1) dir = 0;
	    	finalState = 1;
	        break;
	    case 2:
	    	if (dir == -1) dir = 1;
	    	finalState = 2;
	        break;
	    case 7:
	    	btnpress = 1;
	    	break;
	    default: //case 3
	    	if (dir == 0 && finalState == 2) final = 1;
	    	if (dir == 1 && finalState == 1) final = 0;
	    	dir = -1;
	        break;
	    }
		XGpio_InterruptClear(GpioPtr, 1);
}
