#include "qpn_port.h"
#include "bsp.h"
#include "chromatic_tumor.h"
#include "xintc.h"
#include "xil_exception.h"
#include "xtmrctr_l.h"
#include "xtmrctr.h"
#include "xspi.h"
#include "xspi_l.h"
#include "lcd.h"
#include "stream_grabber.h"
#include "fft.h"

/*****************************/

/* Define all variables and Gpio objects here  */

#define GPIO_CHANNEL1 1
#define DELAY(X) for(volatile int z=0;z<X;z++);

static XGpio EncoderGpio;
static XIntc intc;
static XTmrCtr tmrctr;

static XGpio BtnGPIO;
static XGpio dc;
static XSpi spi;

/*
static int BufferHigh[240][320];
static int BufferLow[240][320];
*/
/******** For debouncing rotary encoder*/
volatile int dir = -1;
volatile int final = -1;
volatile int finalState = 3;
/***********************/

volatile int mode = -1;
volatile int ct = 0;
volatile int sleep = 0;

extern volatile int modeChange;

#define SAMPLES 4096 // AXI4 Streaming Data FIFO has size 512
#define M 12 //2^m=samples
#define CLOCK 100000000.0 //clock speed

int int_buffer[SAMPLES];
float cosLUT[M+1][SAMPLES/2]; // [k][j]
float sinLUT[M+1][SAMPLES/2]; // [k][j]
XSpi_Config *spiConfig;	/* Pointer to Configuration data */


u32 controlReg;

/* Interrupt handlers for buttons, encoder, and timer*/
/*..........................................................................*/

void timer_handler(){
	Xuint32 ControlStatusReg;
	ControlStatusReg = XTimerCtr_ReadReg(tmrctr.BaseAddress,0,XTC_TCSR_OFFSET);
	//xil_printf("timer handler");
	ct++;
	XTmrCtr_WriteReg(tmrctr.BaseAddress, 0 , XTC_TCSR_OFFSET, ControlStatusReg | XTC_CSR_INT_OCCURED_MASK);
}


void button_handler(void *CallbackRef){
	XGpio *GpioPtr = (XGpio *)CallbackRef;
	unsigned int btn = XGpio_DiscreteRead(&BtnGPIO, 1);//1 up 2 left 4 right 8 down
	//xil_printf("b ");
	ct = 0;
	if(btn==1){
		modeChange = 1;
		QActive_postISR((QActive *)&AO_ChromaticTumor, BUTTON_UP);
	}
	else if(btn==2){
		QActive_postISR((QActive *)&AO_ChromaticTumor, BUTTON_LEFT);
	}
	else if(btn==4){
		QActive_postISR((QActive *)&AO_ChromaticTumor, BUTTON_RIGHT);
	}
	else if(btn==8){
		//modeChange = 1;
		QActive_postISR((QActive *)&AO_ChromaticTumor, BUTTON_DOWN);
	}
	else if(btn==16){
		QActive_postISR((QActive *)&AO_ChromaticTumor, BUTTON_MIDDLE);
	}
	XGpio_InterruptClear(GpioPtr, 1);
}

void encoder_handler(void *CallbackRef){
	XGpio *GpioPtr = (XGpio *)CallbackRef;
	unsigned int encoder = XGpio_DiscreteRead(&EncoderGpio, 1);//if clockwise, 1023, else 2013, push=73
	// 0 is clockwise, 1 is counterclockwise
	//xil_printf("e ");
	//modeChange = 1;
	ct = 0;
	//xil_printf("enters encoder");
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
	    	QActive_postISR((QActive *)&AO_ChromaticTumor, ENCODER_CLICK);
	    	break;
	    default: //case 3
	    	if (dir == 0 && finalState == 2) final = 1;
	    	if (dir == 1 && finalState == 1) final = 0;
	    	dir = -1;
	        break;
	    }
		XGpio_InterruptClear(GpioPtr, 1);
}

void rotaryup()
{/*
	// If in octave selection state chromatic_tuner.c for state behavior
	if (state == 1)
	{
		// Increment selected octave
		if (octave < 9)
			++octave;
		if (octave < 7)
			factor = 8;
		else if (octave < 8)
			factor = 4;
		else if (octave < 9)
			factor = 2;
		else
			factor = 1;
		// set buffer to be displayed to new octave
		itoa(octave,buffer,10);
	}
	// If in tuning state chromatic_tuner.c for state behavior
	else if (state == 3)
	{
		// Increment displayed frequency
		if (a4tune < 460)
			++a4tune;
		// set buffer to be displayed to new frequency
		itoa(a4tune,buffer,10);
	}*/
}

void rotarydown()
{
	// If in octave selection state chromatic_tuner.c for state behavior
	/*if (state == 1)
	{
		// Decrement selected octave
		if (octave > 0)
			--octave;
		if (octave < 7)
			factor = 8;
		else if (octave < 8)
			factor = 4;
		else if (octave < 9)
			factor = 2;
		else
			factor = 1;
		// set buffer to be displayed to new octave
		itoa(octave,buffer,10);
	}
	// If in tuning state chromatic_tuner.c for state behavior
	else if (state == 3)
	{
		// Decrement displayed frequency
		if (a4tune > 420)
			--a4tune;
		// set buffer to be displayed to new frequency
		itoa(a4tune,buffer,10);
	}*/
}

void init_LUT() {
	// so b = 1, 2, 4, 6, ..., 512
	// which means j = 0, 1, 2, ..., 9
	// k = max 255.
	int k;
	int j;
	float temp;
	uint32_t yi;
	uint32_t exponent;
	for(j = 0; j < M+1; j++) {
		for (k = 0; k < (SAMPLES >> 1); k++) {
			temp = -1 * PI * k;
			//xil_printf("k is %d, b is %f\r\n", k, pow(2.0,j));
			yi = *(uint32_t *)&temp;        // get float value as bits
			exponent = yi & 0x7f800000;   // extract exponent bits 30..23
			exponent -= (j << 23);                 // subtract n from exponent
			yi = yi & ~0x7f800000 | exponent;      // insert modified exponent back into bits 30..23
			//xil_printf("here2");

			temp = temp/(pow(2,j));
			//temp = *(float *)&yi;
			cosLUT[j][k] = cos(temp);
			sinLUT[j][k] = sin(temp);
			if(j==1 && k == 0) cosLUT[j][k] = 1.0;
			//xil_printf(cosLUT[j][k]);


		}
	}
}
void read_fsl_values(float* q, int n) {
   int i;
   unsigned int x;
   int ct = 0;
   stream_grabber_wait_enough_samples(1);
   float temp;
   float ave;
   uint32_t yi;
   uint32_t exponent;
   for(i = 0; i < n; i++) {
      int_buffer[i] = stream_grabber_read_sample(i);
      // xil_printf("%d\n",int_buffer[i]);
      x = int_buffer[i];
      temp = 3.3*x;
      yi = *(uint32_t *)&temp;        // get float value as bits
      exponent = yi & 0x7f800000;   // extract exponent bits 30..23
      exponent -= (26 << 23);                 // subtract n from exponent
      yi = yi & ~0x7f800000 | exponent;      // insert modified exponent back into bits 30..23
      //average[ct] = *(float *)&yi;
      ct++;
      //if(ct==4){
    	  //ave = (average[0]+average[1]+average[2]+average[3]+average[4]+average[5]+average[6]+average[7])/8;
          //q[i] = ave;
    	  //printf("average is %f \r\n", average[0]);
    //	  ct = 0;
      //}
    	  q[i] = *(float *)&yi;
    	  //q[i] = 1.5 * cosf(i *(2.0*PI*80/(2*CLOCK/n))) + 1.5;
    	//  ct = 0;
      //}
      //printf("ct %d is %f\r\n",ct,q[i]);
      //q[i] = 3.3*x/67108864.0; // 3.3V and 2^26 bit precision.
   }
   stream_grabber_start();
}
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
	XIntc_Connect(&intc, XPAR_MICROBLAZE_0_AXI_INTC_AXI_TIMER_0_INTERRUPT_INTR,
		(XInterruptHandler) timer_handler, &tmrctr);
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
	XIntc_Enable(&intc, XPAR_MICROBLAZE_0_AXI_INTC_AXI_GPIO_ENCODER_IP2INTC_IRPT_INTR);
	XIntc_Enable(&intc, XPAR_MICROBLAZE_0_AXI_INTC_AXI_GPIO_BTN_IP2INTC_IRPT_INTR);
	XIntc_Enable(&intc, XPAR_MICROBLAZE_0_AXI_INTC_AXI_TIMER_0_INTERRUPT_INTR);
	microblaze_enable_interrupts();
	XIntc_Start(&intc, XIN_REAL_MODE);
	XGpio_InterruptEnable(&EncoderGpio, 1);
	XGpio_InterruptGlobalEnable(&EncoderGpio);
	XGpio_InterruptEnable(&BtnGPIO, 1);
	XGpio_InterruptGlobalEnable(&BtnGPIO);
/*
	// set up Buffer array for the background
	int x, y = 0;
	for(int i = 0; i < 240; i++) {
		x = i % 40;
		for (int j = 0; j < 320; j++) {
			y = j % 40;
			if((y>(2*x-40) && y<(-2*x+40+80*(x/40))) || (y<(2*x-40) && y>(-2*x+40+80*(x/40)))){
				BufferHigh[i][j] = (0 & 0x0F8) | 0 >> 5;
				BufferLow[i][j] = (0 & 0x1C) << 3 | 200 >> 3;
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
	}*/
	//setFont(SmallFont);
	//ct = 0;
	/*
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
		//final = -1;
		DELAY(100);
	}
	*/
}


void QF_onIdle(void) {        /* entered with interrupts locked */
    QF_INT_UNLOCK();                       /* unlock interrupts */
    {
    	// Write code to increment your interrupt counter here.
    	if (final == 1)
		{
			rotaryup();
			QActive_postISR((QActive *)&AO_ChromaticTumor, ENCODER_UP);
			final = -1;
		}
		else if (final==0)
		{
			rotarydown();
			QActive_postISR((QActive *)&AO_ChromaticTumor, ENCODER_DOWN);
			final = -1;
		}
    }
}

/* Q_onAssert is called only when the program encounters an error*/
/*..........................................................................*/
void Q_onAssert(char const Q_ROM * const Q_ROM_VAR file, int line) {
    (void)file;                                   /* name of the file that throws the error */
    (void)line;                                   /* line of the code that throws the error */
    QF_INT_LOCK();
    //printDebugLog();
    for (;;) {
    }
}


