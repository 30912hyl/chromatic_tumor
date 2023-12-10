/*****************************************************************************
* ChromaticTumor.c for ChromaticTumor of ECE 153a at UCSB
* Date of the Last Update:  October 23,2014
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

static char notes[12][3]={"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
extern int note;
extern int oct;

static float q[SAMPLES];
static float w[SAMPLES];
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


/**********************************************************************/


ChromaticTumor AO_ChromaticTumor;


void ChromaticTumor_ctor(void)  {
	xil_printf("entering ctor \r\n");
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
	xil_printf("finishing ctor \r\n");
}


QState ChromaticTumor_initial(ChromaticTumor *me) {
	xil_printf("\n\rInitialization");
	initLCD();
	clrScr();
	for(int j=0; j<320; j++) {
		for(int i=0; i<240; i++) {
			setXY(i, j, i, j);
			LCD_Write_DATA(AO_ChromaticTumor.BufferHigh[i][j]);
			LCD_Write_DATA(AO_ChromaticTumor.BufferLow[i][j]);
			clrXY();
		}
	}
    return Q_TRAN(&ChromaticTumor_on);
}

QState ChromaticTumor_on(ChromaticTumor *me) {
	switch (Q_SIG(me)) {
		case Q_ENTRY_SIG: {
			xil_printf("\n\rOn");
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
			xil_printf("Press Encoder To Begin");
			return Q_HANDLED();
		}

		case BUTTON_UP: {
			return Q_HANDLED();
		}

		case BUTTON_DOWN: {
			return Q_HANDLED();
		}
		case BUTTON_LEFT: {
			return Q_HANDLED();
		}

		case BUTTON_RIGHT: {
			return Q_HANDLED();
		}

		case BUTTON_MIDDLE: {
			return Q_HANDLED();
		}

		case ENCODER_UP: {
			return Q_HANDLED();
		}

		case ENCODER_DOWN: {
			return Q_HANDLED();
		}

		case ENCODER_CLICK:  {
			xil_printf("DEFAULT MODE");
			return Q_TRAN(&ChromaticTumor_default);
		}

	}

	return Q_SUPER(&ChromaticTumor_on);

}

QState ChromaticTumor_default(ChromaticTumor *me) {
	switch (Q_SIG(me)) {
		case Q_ENTRY_SIG: {
			xil_printf("Startup Default Mode\n");
		    setColor(176, 224, 230);
			setFont(BigFont);
			lcdPrint("Note:  ", 65, 150);
			modeChange = 0;
			int l=0;
			float sample_f;
			float frequency;

			while(modeChange==0){
				read_fsl_values(q, SAMPLES);
				sample_f = (100000000) >> 14;
				for(l=0;l<SAMPLES;l++)
					w[l]=0;

				frequency=fft(q,w,SAMPLES,M,sample_f);
				findNote(frequency, AO_ChromaticTumor.base);
				if(oct>-1 && oct<11){
					lcdPrint(notes[note], 145, 150);
					itoa(oct,buffer,10);
					if(strchr(notes[note], '#') != NULL){
						lcdPrint(buffer, 175, 150);
					}
					else{
						for(int j=150; j<170; j++) {
							for(int i=175; i<=190; i++) {
								setXY(i, j, i, j);
								LCD_Write_DATA(AO_ChromaticTumor.BufferHigh[i][j]);
								LCD_Write_DATA(AO_ChromaticTumor.BufferLow[i][j]);
								clrXY();
							}
						}
						lcdPrint(buffer, 160, 150);
					}

					xil_printf("frequency: %d Hz\r\n", (int)(frequency));
				}
			}
			return Q_HANDLED();
		}

		case ENCODER_UP: {
			return Q_HANDLED();
		}

		case ENCODER_DOWN: {
			return Q_HANDLED();
		}

		case ENCODER_CLICK:  {
			return Q_HANDLED();
		}

		case BUTTON_UP: {
			xil_printf("ALT TUNING MODE\n");
			return Q_TRAN(&ChromaticTumor_altTune);
		}

		case BUTTON_LEFT:  {
			return Q_HANDLED();
		}
		case BUTTON_RIGHT:  {
			return Q_HANDLED();
		}
		case BUTTON_MIDDLE:  {
			return Q_HANDLED();
		}
		case BUTTON_DOWN:  {
			//xil_printf("TEST MODE\n");
			return Q_HANDLED();
		}

	}

	return Q_SUPER(&ChromaticTumor_on);

}

QState ChromaticTumor_altTune(ChromaticTumor *me) {
	switch (Q_SIG(me)) {
		case Q_ENTRY_SIG: {
			xil_printf("Startup Alternative Tuning Mode\n");
			return Q_HANDLED();
		}

		case BUTTON_UP: {
			return Q_HANDLED();
		}

		case BUTTON_DOWN: {
			//xil_printf("TEST MODE\n");
			return Q_HANDLED();
		}
		case BUTTON_LEFT: {
			return Q_HANDLED();
		}

		case BUTTON_RIGHT: {
			return Q_HANDLED();
		}

		case BUTTON_MIDDLE: {
			return Q_HANDLED();
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
			xil_printf("DEFAULT MODE\n");
			return Q_TRAN(&ChromaticTumor_default);
		}

	}

	return Q_SUPER(&ChromaticTumor_on);

}

