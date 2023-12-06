/*****************************************************************************
* ChromaticTumor.c for ChromaticTumor of ECE 153a at UCSB
* Date of the Last Update:  October 23,2014
*****************************************************************************/




#define AO_CHROMATICTUMOR

#include "qpn_port.h"
#include "bsp.h"
#include "chromatic_tumor.h"




typedef struct ChromaticTumorTag  {               //tuner State machine
	QActive super;
}  ChromaticTumor;

/* Setup state machines */
/**********************************************************************/
static QState ChromaticTumor_initial (ChromaticTumor *me);
static QState ChromaticTumor_on      (ChromaticTumor *me);
static QState ChromaticTumor_stateA  (ChromaticTumor *me);
static QState ChromaticTumor_stateB  (ChromaticTumor *me);


/**********************************************************************/


ChromaticTumor AO_ChromaticTumor;


void ChromaticTumor_ctor(void)  {
	ChromaticTumor *me = &AO_ChromaticTumor;
	QActive_ctor(&me->super, (QStateHandler)&ChromaticTumor_initial);
}


QState ChromaticTumor_initial(ChromaticTumor *me) {
	xil_printf("\n\rInitialization");
    return Q_TRAN(&ChromaticTumor_on);
}

QState ChromaticTumor_on(ChromaticTumor *me) {
	switch (Q_SIG(me)) {
		case Q_ENTRY_SIG: {
			xil_printf("\n\rOn");
			}

		case Q_INIT_SIG: {
			return Q_TRAN(&ChromaticTumor_stateA);
			}
	}

	return Q_SUPER(&QHsm_top);
}


/* Create ChromaticTumor_on state and do any initialization code if needed */
/******************************************************************/

QState ChromaticTumor_stateA(ChromaticTumor *me) {
	switch (Q_SIG(me)) {
		case Q_ENTRY_SIG: {
			xil_printf("Startup State A");
			return Q_HANDLED();
		}

		case ENCODER_UP: {
			xil_printf("Encoder Up from State A");
			return Q_HANDLED();
		}

		case ENCODER_DOWN: {
			xil_printf("Encoder Down from State A");
			return Q_HANDLED();
		}

		case ENCODER_CLICK:  {
			xil_printf("Changing State");
			return Q_TRAN(&ChromaticTumor_stateB);
		}

	}

	return Q_SUPER(&ChromaticTumor_on);

}

QState ChromaticTumor_stateB(ChromaticTumor *me) {
	switch (Q_SIG(me)) {
		case Q_ENTRY_SIG: {
			xil_printf("Startup State B");
			return Q_HANDLED();
		}

		case BUTTON_UP: {
			xil_printf("MODE 1\r");
			return Q_HANDLED();
		}

		case BUTTON_DOWN: {
			xil_printf("MODE 2\r");
			return Q_HANDLED();
		}
		case BUTTON_LEFT: {
			xil_printf("MODE 3\r");
			return Q_HANDLED();
		}

		case BUTTON_RIGHT: {
			xil_printf("MODE 4\r");
			return Q_HANDLED();
		}

		case BUTTON_MIDDLE: {
			xil_printf("MODE 5\r");
			return Q_HANDLED();
		}
		case ENCODER_CLICK:  {
			xil_printf("Changing State");
			return Q_TRAN(&ChromaticTumor_stateA);
		}

	}

	return Q_SUPER(&ChromaticTumor_on);

}
