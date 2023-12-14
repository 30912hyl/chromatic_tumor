/*****************************************************************************
* main.c for Chromatic Tuner Project of ECE 153a at UCSB
*****************************************************************************/

#include "qpn_port.h"                                       /* QP-nano port */
#include "bsp.h"                             /* Board Support Package (BSP) */
#include "chromatic_tumor.h"                               /* application interface */
#include "xil_cache.h"		                /* Cache Drivers */

static QEvent l_chromatictumorQueue[30];

QActiveCB const Q_ROM Q_ROM_VAR QF_active[] = {
	{ (QActive *)0,            (QEvent *)0,          0                    },
	{ (QActive *)&AO_ChromaticTumor,    l_chromatictumorQueue,         Q_DIM(l_chromatictumorQueue)  }
};

Q_ASSERT_COMPILE(QF_MAX_ACTIVE == Q_DIM(QF_active) - 1);

// Do not edit main, unless you have a really good reason

int main(void) {

	Xil_ICacheInvalidate();
	Xil_ICacheEnable();
	Xil_DCacheInvalidate();
	Xil_DCacheEnable();

	ChromaticTumor_ctor(); // inside of chromatic_tumor.c
	BSP_init(); // inside of bsp.c, starts out empty!
	QF_run(); // inside of qfn.c
	return 0;
}


/*
#include <stdio.h>
#include "xil_cache.h"
#include <mb_interface.h>

#include "xbasic_types.h"
#include "xparameters.h"
#include <xil_types.h>
#include <xil_assert.h>

#include "xtmrctr_l.h"
#include "xtmrctr.h"
#include <xio.h>
#include "xtmrctr.h"
#include "xintc.h"
#include "fft.h"
#include "note.h"
#include "stream_grabber.h"
#include <math.h>

*/
