#include "fft.h"
#include "complex.h"
#include <xil_types.h>

#define SAMPLES 512
#define M 9
static float new_[SAMPLES];
static float new_im[SAMPLES];

extern float cosLUT[M+1][SAMPLES/2];
extern float sinLUT[M+1][SAMPLES/2];

extern inline unsigned int reverseBits(unsigned int bigN)
{
    unsigned int rev = 0;
    int n = bigN;
    // traversing bits of 'n' from the right
    for(int ct = 0; ct<9; ct++) {
        // bitwise left shift 'rev' by 1
        rev <<= 1;

        // if current bit is '1'
        if (n & 1 == 1)
            rev ^= 1;

        // bitwise right shift 'n' by 1
        n >>= 1;
    }
    // required number
    return rev;
}

//__attribute__((section(".text.fft_code")))
float fft(float* q, float* w, int n, int m, float sample_f) { //n=2^m samples
	int a,b,r,d,e,c;
	int k,place;
	a = n >> 1;
	b=1;
	int i,j;
	float real=0,imagine=0;
	float max,frequency;
	int temp, invertedIndex = 0;

	/*for(i=0;i<n;i++){
		//xil_printf("myBool[%d] is: %d \r\n", i, myBool[i]);
			//xil_printf("ETNERES");
		invertedIndex = reverseBits(i);
		if(invertedIndex>i){
			temp = q[i];
			q[i] = q[invertedIndex];
			q[invertedIndex] = temp;
			temp = w[i];
			w[i] = w[invertedIndex];
			w[invertedIndex] = temp;
		}

	}*/
	// ORdering algorithm

	for(i=0; i<(m-1); i++){
		d=0;
		for (j=0; j<b; j++){
			for (c=0; c<a; c++){	
				e=c+d;
				c=c<<1;
				new_[e]=q[(c)+d];
				new_im[e]=w[(c)+d];
				new_[e+a]=q[c+1+d];
				new_im[e+a]=w[c+1+d];
				c=c>>1;
			}
			d+=(n >> i);
		}		
		for (r=0; r<n;r++){
			q[r]=new_[r];
			w[r]=new_im[r];
		}
		b = b << 1;
		a=n >> (i+2);              // b=2 i=0 -> n/2^2    b=4 i=1 -> n/2^3    b=8 i=2 -> n/2^4
	}

	//end ordering algorithm

	b=1;
	k=0;
	for (j=0; j<m; j++){	
	//MATH
		for(i=0; i<n; i+=2){
			if (i!=0 && i%(n/b)==0)
				k++;
			//cosine(-PI*k/b)
			//sine(-PI*k/b)
			/*if(cosLUT[j][k]-cosine(PI*k/b)<0.1){
				//printf("RIGHT %f, should be: %f, %d, %d\r\n", cosLUT[j][k], cosine(-PI*k/b), k, b);
			}
			else{
				printf("WRONG %f, should be: %f, %d, %d\r\n", cosLUT[j][k], cosine(-PI*k/b), k, b);
			}*/
			real=mult_real(q[i+1], w[i+1], cosLUT[j][k], sinLUT[j][k]);
			imagine=mult_im(q[i+1], w[i+1], cosLUT[j][k], sinLUT[j][k]);
			new_[i]=q[i]+real;
			new_im[i]=w[i]+imagine;
			new_[i+1]=q[i]-real;
			new_im[i+1]=w[i]-imagine;

		}
		for (i=0; i<n; i++){
			q[i]=new_[i];
			w[i]=new_im[i];
		}
	//END MATH
		q[0]=q[0];
	//REORDER
		for (i=0; i<n>>1; i++){
			new_[i]=q[i<<1];
			new_[i+(n>>1)]=q[(i<<1)+1];
			new_im[i]=w[i<<1];
			new_im[i+(n>>1)]=w[(i<<1)+1];
		}
		for (i=0; i<n; i++){
			q[i]=new_[i];
			w[i]=new_im[i];
		}
	//END REORDER	
		b=b<<1;
		//xil_printf("k is %d",k);
		k=0;		
	}

	//find magnitudes
	max=0;
	place=1;
	for(i=1;i<(n>>1);i++) {
		new_[i]=q[i]*q[i]+w[i]*w[i];
		if(max < new_[i]) {
			max=new_[i];
			place=i;
		}
	}
	
	//float s=sample_f/n; //spacing of bins

	float s = sample_f;
	uint32_t yi = *(uint32_t *)&s;        // get float value as bits
	uint32_t exponent = yi & 0x7f800000;   // extract exponent bits 30..23
	exponent -= (9 << 23);                 // subtract n from exponent
	yi = yi & ~0x7f800000 | exponent;      // insert modified exponent back into bits 30..23
	s = *(float *)&yi;
	
	frequency = s*place;

	//curve fitting for more accuarcy
	//assumes parabolic shape and uses three point to find the shift in the parabola
	//using the equation y=A(x-x0)^2+C
	float y1=new_[place-1],y2=new_[place],y3=new_[place+1];
	float x0=s+(2*s*(y2-y1))/(2*y2-y1-y3);
	x0=x0/s-1;
	
	if(x0 <0 || x0 > 2) { //error
		return 0;
	}
	if(x0 <= 1)  {
		frequency=frequency-(1-x0)*s;
	}
	else {
		frequency=frequency+(x0-1)*s;
	}
	
	return frequency;
}
