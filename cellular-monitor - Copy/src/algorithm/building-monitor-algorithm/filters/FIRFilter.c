#include "FIRFilter.h"
#include "log.h"

static int filter_taps[FIRFILTER_TAP_NUM] = {
  -1468,
  3650,
  -638,
  -8365,
  13829,
  -8365,
  -638,
  3650,
  -1468
};

void FIRFilter_init(FIRFilter* f) {
  int i;
  for(i = 0; i < FIRFILTER_TAP_NUM; ++i)
    f->history[i] = 0;
  f->last_index = 0;
}

void FIRFilter_put(FIRFilter* f, int input) {
  f->history[f->last_index++] = input;
  if(f->last_index == FIRFILTER_TAP_NUM)
    f->last_index = 0;
}

int FIRFilter_get(FIRFilter* f) {
  long long acc = 0;
  int index = f->last_index, i;
  for(i = 0; i < FIRFILTER_TAP_NUM; ++i) {
    index = index != 0 ? index-1 : FIRFILTER_TAP_NUM-1;
    acc += (long long)f->history[index] * filter_taps[i];
    // printf("acc %lld\n", acc);
  };
  int ret = acc >> 16;
  // printf("ret %d\n", ret);
  return ret;
}
