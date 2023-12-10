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

/*  debug function

void printDebugLog() {
	int size = Q_DIM(QF_active);
	xil_printf("Number of HSMs: %i\r\n", size);
	for (int i = 0; i < size; i++) {
		xil_printf("\r\n");
		if (QF_active[i].act == 0 || QF_active[i].act->prio != i) {
			xil_printf("HSM %i: not initialized.\r\n", i);
			continue;
		}
		const QActiveCB *block = &(QF_active[i]);
		QActive *act = block->act;
		xil_printf("HSM %i: initialized\r\n", i);
		xil_printf("Queue: %i/%i\r\n", act->nUsed, block->end);
		int ind = act->tail;
		for (int j = 0; j < act->nUsed; j++) {
			QSignal sig = block->queue[ind].sig;
			xil_printf("\tEvent %i: %i\r\n", j, sig);
			ind++;
			if (ind >= block->end)
				ind -= block->end;
		}
	}
}
*/

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

#define SAMPLES 4096 // AXI4 Streaming Data FIFO has size 512
#define M 12 //2^m=samples
#define CLOCK 100000000.0 //clock speed


extern uintptr_t FFT_CODE_START;
extern uintptr_t FFT_CODE_END;

XTmrCtr timer;

int int_buffer[SAMPLES];
static float q[SAMPLES];
static float w[SAMPLES];

float cosLUT[M+1][SAMPLES/2]; // [k][j]
float sinLUT[M+1][SAMPLES/2]; // [k][j]

static float average[8];
//void print(char *str);
int timerCount[6] = {0};
int total_count;
uint32_t addrFFTBeg = -2147405252;
uint32_t addrFFTMath = -2147404632;
uint32_t addrFFTReorder = -2147403932;
uint32_t addrFFTRest = -2147403564;
uint32_t addrFFTEnd = -2147402864;
uint32_t addrReadBeg = -2147401512;
uint32_t addrReadEnd = -2147401180;

void timer_handler(){
	Xuint32 ControlStatusReg;
	ControlStatusReg = XTimerCtr_ReadReg(timer.BaseAddress,0,XTC_TCSR_OFFSET);
	//ct++;
	XTmrCtr_WriteReg(timer.BaseAddress, 0 , XTC_TCSR_OFFSET, ControlStatusReg | XTC_CSR_INT_OCCURED_MASK);
	uint32_t a;
	asm("add %0, r0, r14" : " = r"(a));
	if(a>=addrFFTBeg && a<addrFFTMath){
		timerCount[0]++;//ordering time
		//xil_printf("timerCount[0] is %d", timerCount[0]);
	}
	else if(a>=addrFFTMath && a<addrFFTReorder){
		timerCount[1]++;//math time
	}
	else if(a>=addrFFTReorder && a<addrFFTRest){
		timerCount[2]++;//reordering time
	}
	else if(a>=addrFFTRest && a<addrFFTEnd){
		timerCount[3]++;//rest of fft time
	}
	else if(a>=addrReadBeg && a<addrReadEnd){
		timerCount[4]++;//read_fst time
	}
	else{
		timerCount[5]++;//rest of program time
	}
	//printf("\n");

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



int main() {
   float sample_f;
   int l;
   int ticks; //used for timer
   uint32_t Control;
   float frequency; 
   float tot_time; //time to run program
   int aveIndex=0;
   float ave;
   Xil_ICacheInvalidate();
   Xil_ICacheEnable();
   Xil_DCacheInvalidate();
   Xil_DCacheEnable();

   //set up timer
   XTmrCtr_Initialize(&timer, XPAR_AXI_TIMER_0_DEVICE_ID);
   //Control = XTmrCtr_GetOptions(&timer, 0) | XTC_CAPTURE_MODE_OPTION | XTC_INT_MODE_OPTION;
   //XTmrCtr_SetOptions(&timer, 0, Control);
	XTmrCtr_SetOptions(&timer, 0, XTC_INT_MODE_OPTION | XTC_AUTO_RELOAD_OPTION);

   //XTmrCtr_SetResetValue(&timer, 0, 0xFFFFFFFF - 100000);

   XIntc intc;
   XIntc_Initialize(&intc, XPAR_INTC_0_DEVICE_ID);
   XIntc_Connect(&intc, XPAR_MICROBLAZE_0_AXI_INTC_AXI_TIMER_0_INTERRUPT_INTR,
   		(XInterruptHandler) timer_handler, &timer);
   XIntc_Enable(&intc, XPAR_MICROBLAZE_0_AXI_INTC_AXI_TIMER_0_INTERRUPT_INTR);

   	microblaze_enable_interrupts();
    XIntc_Start(&intc, XIN_REAL_MODE);

   init_LUT();

   print("Hello World\n\r");
   //uint32_t a;
   stream_grabber_start();
   XTmrCtr_Start(&timer, 0);

   while(1) { 
      //XTmrCtr_Start(&timer, 0);

      //Read Values from Microblaze buffer, which is continuously populated by AXI4 Streaming Data FIFO.
      read_fsl_values(q, SAMPLES);


      sample_f = (100000000) >> 14;
      //xil_printf("sample frequency: %d \r\n",(int)sample_f);

      //zero w array
      for(l=0;l<SAMPLES;l++)
         w[l]=0; 

      frequency=fft(q,w,SAMPLES,M,sample_f);
      //ignore noise below set frequency
      //if(frequency > 200.0) {
     /* average[aveIndex] = frequency;
      aveIndex++;
      ave = (average[0]+average[1]+average[2]+average[3]+average[4]+average[5]+average[6]+average[7])/8;
      if(aveIndex==7) aveIndex = 0;*/


         //findNote(frequency, 440);


      //fftbool = 0;
         //get time to run program
         //ticks=XTmrCtr_GetValue(&timer, 0);
        //XTmrCtr_Stop(&timer, 0);
         //tot_time=ticks/CLOCK;
         //xil_printf("frequency: %d Hz\r\n", (int)(frequency));
        //xil_printf("program time: %dms \r\n",(int)(1000*tot_time));

      //total_count = timerCount[0]+timerCount[1]+timerCount[2]+timerCount[3]+timerCount[4]+timerCount[5];
      //if(total_count>10000) break;
      //xil_printf("total_count is %d \r\n", total_count);
   /*}
   	 printf("fraction of time in fft ordering: %f ", timerCount[0]/(total_count+0.0));
	 printf("fraction of time in fft math: %f ", timerCount[1]/(total_count+0.0));
	 printf("fraction of time in fft reorder: %f ", timerCount[2]/(total_count+0.0));
	 printf("fraction of time in fft rest: %f ", timerCount[3]/(total_count+0.0));
	 printf("fraction of time in read_fsl_values: %f ", timerCount[4]/(total_count+0.0));
	 printf("fraction of time in other: %f ", timerCount[5]/(total_count+0.0));


   return 0;
}

*/
