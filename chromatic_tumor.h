#ifndef chromatictumor_h
#define chromatictumor_h

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


enum ChromaticTunerSignals {
	ENCODER_UP = Q_USER_SIG,
	ENCODER_DOWN,
	ENCODER_CLICK,
	CLOCK_TICK,
};


extern struct ChromaticTumorTag AO_ChromaticTumor;


void ChromaticTumor_ctor(void);

#endif  