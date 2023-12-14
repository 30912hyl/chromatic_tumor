/*****************************************************************************
* ChromaticTumor.c for ChromaticTumor of ECE 153a at UCSB
*****************************************************************************/
#define AO_CHROMATICTUMOR

#include "qpn_port.h"
#include "bsp.h"
#include "chromatic_tumor.h"
#include "lcd.h"
#include "note.h"
#include "fft.h"
#include <string.h>

#define SAMPLES 4096
#define M 12
#define BIN 60

static char notes[12][3]={"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
extern int note;
extern int oct;
extern int cents;
int prevC = 0;
extern float new_[SAMPLES];
static float q[SAMPLES];
static float w[SAMPLES];
int histogram[BIN];
float freqVals[4096];
char buffer[6];
volatile int modeChange;

typedef struct ChromaticTumorTag  {               //tuner State machine
	QActive super;
	volatile int transition;

	int BufferHigh[240][320];
	int BufferLow[240][320];
	int base;
	char octave;
}  ChromaticTumor;

/* Setup state machines */
/**********************************************************************/
static QState ChromaticTumor_initial (ChromaticTumor *me);
static QState ChromaticTumor_on      (ChromaticTumor *me);
static QState ChromaticTumor_title   (ChromaticTumor *me);
static QState ChromaticTumor_default (ChromaticTumor *me);
static QState ChromaticTumor_altTune (ChromaticTumor *me);
static QState ChromaticTumor_testMode(ChromaticTumor *me);

/**********************************************************************/

ChromaticTumor AO_ChromaticTumor;

void erase(int x1, int x2, int y1, int y2) {
	if(x2 < x1) {
		for(int j=y1; j<y2; j++) {
			for(int i=x2; i<=x1; i++) {
				setXY(i, j, i, j);
				LCD_Write_DATA(AO_ChromaticTumor.BufferHigh[i][j]);
				LCD_Write_DATA(AO_ChromaticTumor.BufferLow[i][j]);
				clrXY();
			}
		}
	} else {
		for(int j=y1; j<y2; j++) {
			for(int i=x1; i<=x2; i++) {
				setXY(i, j, i, j);
				LCD_Write_DATA(AO_ChromaticTumor.BufferHigh[i][j]);
				LCD_Write_DATA(AO_ChromaticTumor.BufferLow[i][j]);
				clrXY();
			}
		}
	}


}

void draw_hist(int *hist){
	int barSize = 240/BIN;
	setColor(255, 0, 0);
	xil_printf("Histogram caps out at %d.\r\n", 200);
	for(int i=0;i<BIN;i++){
		if(hist[i]>200){
			xil_printf("bin %d actual value: %d\r\n", i, hist[i]);
			hist[i]=200;
		}
		fillRect(i*barSize,320,(i+1)*barSize,320-hist[i]);
	}
    setColor(176, 224, 230);
}

void ChromaticTumor_ctor(void)  {
	xil_printf("running");
	setColor(0,0,0);
	fillRect(0,0,240,320);
	setColor(176, 224, 230);
	setFont(BigFont);
	lcdPrint("Program", 60, 140);
	lcdPrint("Loading...", 60, 160);
	ChromaticTumor *me = &AO_ChromaticTumor;
	AO_ChromaticTumor.transition = 0;
	AO_ChromaticTumor.base = 440;
	QActive_ctor(&me->super, (QStateHandler)&ChromaticTumor_initial);
	init_LUT();
	int x, y = 0;
	for(int i = 0; i < 240; i++) {
		x = i % 40;
		for (int j = 0; j < 320; j++) {
			y = j % 40;
			if((y>(2*x-40) && y<(-2*x+40+80*(x/40))) || (y<(2*x-40) && y>(-2*x+40+80*(x/40)))){
				AO_ChromaticTumor.BufferHigh[i][j] = (0 & 0x0F8) | 0 >> 5;
				AO_ChromaticTumor.BufferLow[i][j] = (0 & 0x1C) << 3 | 200 >> 3;
			}
			else{
				AO_ChromaticTumor.BufferHigh[i][j] = (255 & 0x0F8) | 255 >> 5;
				AO_ChromaticTumor.BufferLow[i][j] = (255 & 0x1C) << 3 | 255 >> 3;
			}
		}
	}
}


QState ChromaticTumor_initial(ChromaticTumor *me) {
	xil_printf("\r\nInitialization State.\r\n");
	initLCD();
	clrScr();
	// initialize screen
	erase(0, 240, 0, 320);
    return Q_TRAN(&ChromaticTumor_on);
}

QState ChromaticTumor_on(ChromaticTumor *me) {
	switch (Q_SIG(me)) {
		case Q_ENTRY_SIG: {
			xil_printf("\r\nOn State.\r\n");
			}

		case Q_INIT_SIG: {
			return Q_TRAN(&ChromaticTumor_title);
			}
	}

	return Q_SUPER(&QHsm_top);
}


/* Create ChromaticTumor_on state and do any initialization code if needed */
/******************************************************************/

QState ChromaticTumor_title(ChromaticTumor *me) {
	switch (Q_SIG(me)) {
		case Q_ENTRY_SIG: {
			setColor(0,0,0);
			fillRect(20,140,220,220);
			setColor(176, 224, 230);
			setFont(BigFont);
			lcdPrint("Chromatic", 50, 150);
			lcdPrint("Tumor", 80, 170);
			setFont(SmallFont);
			lcdPrint("Click Encoder To Begin", 35, 200);
			modeChange=1;
			return Q_HANDLED();
		}

		case Q_EXIT_SIG: {
			erase(20, 220, 140, 222);
			return Q_HANDLED();
		}
		case BUTTON_UP: {
			return Q_HANDLED();
		}

		case BUTTON_DOWN: {
			return Q_HANDLED();
		}

		case ENCODER_UP: {
			return Q_HANDLED();
		}

		case ENCODER_DOWN: {
			return Q_HANDLED();
		}

		case ENCODER_CLICK:  {
			return Q_TRAN(&ChromaticTumor_default);
		}

	}

	return Q_SUPER(&ChromaticTumor_on);

}

QState ChromaticTumor_default(ChromaticTumor *me) {
	switch (Q_SIG(me)) {
		case Q_ENTRY_SIG: {
			//xil_printf("Startup Default Mode\r");
		    setColor(176, 224, 230);
			setFont(BigFont);
			lcdPrint("Cts:", 70, 170);
			modeChange = 0;
			int l=0;
			float sample_f;
			float frequency;
			itoa(AO_ChromaticTumor.base,buffer,10);
			lcdPrint(buffer, 5, 30);
			lcdPrint("Tuning", 140, 30);
			setFont(SmallFont);
			lcdPrint("Button Up     =  Adjust", 30, 280);
			lcdPrint("Button Down   = Testing", 30, 290);
			lcdPrint("Encoder Click =  Tuning", 30, 300);
			setFont(BigFont);

			while(modeChange==0){
				read_fsl_values(q, SAMPLES);
				sample_f = (100000000) >> 14; //16;
				for(l=0;l<SAMPLES;l++)
					w[l]=0;
				frequency=fft(q,w,SAMPLES,M,sample_f);
				findNote(frequency, AO_ChromaticTumor.base);

				if(oct>-1 && oct<11){
					lcdPrint(notes[note], 105, 80);
					itoa(oct,buffer,10);
					if(strchr(notes[note], '#') != NULL){
						lcdPrint(buffer, 135, 80);         //print number
					}
					else{
						erase(135, 150, 80, 100);
						lcdPrint(buffer, 120, 80);         //print number
					}

					if(cents>0 && prevC>0 && cents<prevC){ //erase bar
						erase(220, 120+2*cents, 120, 145);
					}
					else if(cents>0 && prevC<0){
						erase(20, 120, 120, 145);
					}
					else if(cents<0 && prevC>0){
						erase(220, 120, 120, 145);
					}
					else if(cents<0 && prevC<0 && cents>prevC){
						erase(20, 120+2*cents, 120, 145);
					}
					else if(cents>0 && prevC>0 && cents>prevC){
						erase(20, 120, 120, 145);
					}
					else{
						erase(120, 220, 120, 145);
					}

					if((cents<0 && cents>-20) || (cents>0 && cents <20)){//draw bar
						setColor(0,255,0);
					}
					else if((cents<0 && cents>-40) || (cents>0 && cents <40)){
						setColor(255,255,0);
					}
					else{
						setColor(255,0,0);
					}
					fillRect(120,120,120+2*cents,140);

					setColor(176, 224, 230);
					itoa(cents,buffer,10);
					if(cents>=0 && cents < 10){//single digit positive
						//erase(145, 180, 170, 210);
						buffer[2]=buffer[0];
						buffer[1]='0';
						buffer[0]='+';
					}
					else if(cents>=10){//double digit positive
						//erase(165, 180, 170, 190);
						buffer[2]=buffer[1];
						buffer[1]=buffer[0];
						buffer[0]='+';
					}
					else if(cents<0 && cents>-10){//single digit negative
						//erase(165, 180, 170, 190);
						buffer[2]=buffer[1];
						buffer[1]='0';
						buffer[0]='-';
					}
					lcdPrint(buffer, 130, 170);

					prevC=cents;
					//xil_printf("frequency: %d Hz\r\n", (int)(frequency));
					xil_printf("cents: %d \r\n", cents);
					xil_printf(buffer[0]+buffer[1]+buffer[2]);
				}
			}
			return Q_HANDLED();
		}

		case Q_EXIT_SIG: {
			erase(20,220,120,145);
			erase(70,180,170,190);
			return Q_HANDLED();
		}

		case ENCODER_UP: {
			return Q_HANDLED();
		}

		case ENCODER_DOWN: {
			return Q_HANDLED();
		}

		case ENCODER_CLICK:  {
			//xil_printf("here");
			return Q_HANDLED();
		}

		case BUTTON_UP: {
			return Q_TRAN(&ChromaticTumor_altTune);
		}

		case BUTTON_DOWN:  {
			return Q_TRAN(&ChromaticTumor_testMode);
		}

	}

	return Q_SUPER(&ChromaticTumor_on);

}

QState ChromaticTumor_altTune(ChromaticTumor *me) {
	switch (Q_SIG(me)) {
		case Q_ENTRY_SIG: {
			//xil_printf("Startup Alternative Tuning Mode\r");
			erase(105, 150, 80, 100);
			lcdPrint("Adjust", 140, 30);
			setFont(SmallFont);
			lcdPrint("Shift Encoder To Adjust", 30, 160);
			lcdPrint("Button Up     =  Adjust", 30, 280);
			lcdPrint("Button Down   = Testing", 30, 290);
			lcdPrint("Encoder Click =  Tuning", 30, 300);
			setFont(BigFont);
			return Q_HANDLED();
		}

		case Q_EXIT_SIG: {
			erase(30, 215, 160, 175);
			return Q_HANDLED();
		}
		case BUTTON_UP: {
			return Q_HANDLED();
		}

		case BUTTON_DOWN: {
			return Q_TRAN(&ChromaticTumor_testMode);
		}

		case ENCODER_UP: {
			if(AO_ChromaticTumor.base < 460){
				AO_ChromaticTumor.base++;
			}

			itoa(AO_ChromaticTumor.base,buffer,10);
			lcdPrint(buffer, 5, 30);
			return Q_HANDLED();
		}

		case ENCODER_DOWN: {
			if(AO_ChromaticTumor.base > 420){
				AO_ChromaticTumor.base--;
			}
			itoa(AO_ChromaticTumor.base,buffer,10);
			lcdPrint(buffer, 5, 30);
			return Q_HANDLED();
		}

		case ENCODER_CLICK:  {
			return Q_TRAN(&ChromaticTumor_default);
		}

	}

	return Q_SUPER(&ChromaticTumor_on);

}

QState ChromaticTumor_testMode(ChromaticTumor *me) {
	switch (Q_SIG(me)) {
		case Q_ENTRY_SIG: {
			erase(30,215,280,315);
			lcdPrint(notes[note], 105, 80);
			itoa(oct,buffer,10);
			if(strchr(notes[note], '#') != NULL){
				lcdPrint(buffer, 135, 80);         //print number
			}
			else{
				erase(135, 150, 80, 100);
				lcdPrint(buffer, 120, 80);         //print number
			}

			//xil_printf("Startup Testing Mode\r");
			lcdPrint("Testing", 125, 30);
			float minVal = 0;
			float maxVal = 0;

			for(int i=0;i<4096;i++){
				if(new_[i]>0 && new_[i]<100) freqVals[i]=new_[i];
				else freqVals[i]=0;
				if(new_[i]>maxVal) maxVal = freqVals[i];
			}

			float binsize = (maxVal-minVal)/BIN;
			for(int i=0;i<4096;i++){
				for(int j=1;j<=BIN;j++){
					if(new_[i]<minVal+j*binsize){
						histogram[j-1]++;
						break;
					}
				}
			}
			draw_hist(histogram);
			return Q_HANDLED();
		}

		case Q_EXIT_SIG: {
			int barSize = 240/BIN;
			for(int i=0;i<BIN;i++){
				erase(i*barSize,(i+1)*barSize, 320-histogram[i], 320);
			}
			erase(0,240,315,320);
			for(int i=0;i<BIN;i++){
				histogram[i]=0;
			}
			erase(125, 140, 30, 50);
			erase(140, 180, 80, 105);
			return Q_HANDLED();
		}

		case BUTTON_UP: {
			return Q_TRAN(&ChromaticTumor_altTune);
		}

		case BUTTON_DOWN: {
			return Q_HANDLED();
		}

		case ENCODER_UP: {
			return Q_HANDLED();
		}

		case ENCODER_DOWN: {
			return Q_HANDLED();
		}

		case ENCODER_CLICK:  {
			return Q_TRAN(&ChromaticTumor_default);
		}

	}

	return Q_SUPER(&ChromaticTumor_on);

}

