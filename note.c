#include "note.h"
//#include "lcd.h"
#include <math.h>

//array to store note names for findNote
static char notes[12][3]={"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
int oct=4;
int note=0;
int cents=0;
//finds and prints note of frequency and deviation from note
void findNote(float f, int a) {
	oct = 4;
	note = 0;
	float c=a*pow(2,(oct-4+(-9.0+note)/12));
	float highfreq;
	float lowfreq;
	float r;

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

    //f = 1042, C5
	//determine which note frequency is closest to
	if((f-c) <= (r-f)) { //closer to left note
		if(note==12){
			xil_printf("ERROR LEFT");
			highfreq = a*pow(2,((oct+1)-4+(-9.0+0)/12));
		}
		else highfreq = a*pow(2,(oct-4+(-9.0+note+1)/12));

		lowfreq = a*pow(2,(oct-4+(-9.0+note)/12));
		cents = (int)((f-c)/((highfreq-lowfreq)/100));
		if(cents>50) cents = 50;
	}
	else { //f closer to right note
		note++;
		if(note >=12){
			note=0;
			oct++;
		}
		highfreq = a*pow(2,(oct-4+(-9.0+note)/12));
		if(note==0){
			lowfreq = a*pow(2,((oct-1)-4+(-9.0+11)/12));
		}
		else lowfreq = a*pow(2,(oct-4+(-9.0+note-1)/12));
		cents = (int)((f-r)/((highfreq-lowfreq)/100));
		if(cents<-50) cents = -50;
	}

}
