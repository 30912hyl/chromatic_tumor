#include "note.h"
//#include "lcd.h"
#include <math.h>

//array to store note names for findNote
static char notes[12][3]={"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};

//finds and prints note of frequency and deviation from note
void findNote(float f, int a) {
	float c=a*pow(2,(-3.0/4));
	float r;
	int oct=4;
	int note=0;
	//determine which octave frequency is in
	if(f >= c) {
		while(f > c*2) {
			c=c*2;
			oct++;
		}
	}
	else { //f < C4
		while(f < c) {
			c=c/2;
			oct--;
		}

	}

	//find note below frequency
	//c=middle C
	r=c*root2;
	while(f > r) {
		c=c*root2;
		r=r*root2;
		note++;
	}


	//determine which note frequency is closest to
	if((f-c) <= (r-f)) { //closer to left note
		xil_printf("N:");
		xil_printf(notes[note]);
		xil_printf("%d", oct);
		//xil_printf("\n\n D:+");
		//xil_printf((int)(f-c+.5));
		//xil_printf("Hz");
	}
	else { //f closer to right note
		note++;
		if(note >=12) note=0;
		xil_printf("N:");
		xil_printf(notes[note]);
		xil_printf("%d", oct);
		//xil_printf(" D:-");
		//xil_printf((int)(r-f+.5));
		//xil_printf("Hz");
	}

}
