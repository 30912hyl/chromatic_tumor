#ifndef chromatic_tumor_h
#define chromatic_tumor_h



enum ChromaticTunerSignals {
	ENCODER_UP = Q_USER_SIG,
	ENCODER_DOWN,
	ENCODER_CLICK,
	CLOCK_TICK,
	BUTTON_UP,
	BUTTON_DOWN,
	BUTTON_LEFT,
	BUTTON_RIGHT,
	BUTTON_MIDDLE
};



extern struct ChromaticTumorTag AO_ChromaticTumor;


void ChromaticTumor_ctor(void);

#endif
