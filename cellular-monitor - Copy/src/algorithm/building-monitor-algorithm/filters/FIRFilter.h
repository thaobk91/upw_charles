#ifndef FIRFILTER_H_
#define FIRFILTER_H_

/*

FIR filter designed with
 http://t-filter.appspot.com

sampling frequency: 150 Hz

fixed point precision: 16 bits

* 0 Hz - 25 Hz
  gain = 0
  desired attenuation = -40 dB
  actual attenuation = n/a

* 50 Hz - 60 Hz
  gain = 1
  desired ripple = 1 dB
  actual ripple = n/a

*/

#define FIRFILTER_TAP_NUM 9

typedef struct {
  int history[FIRFILTER_TAP_NUM];
  unsigned int last_index;
} FIRFilter;

void FIRFilter_init(FIRFilter* f);
void FIRFilter_put(FIRFilter* f, int input);
int FIRFilter_get(FIRFilter* f);

#endif