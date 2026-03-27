/**
* Second order high-pass filter
* 
* fc: corner frequency
* Q: quality factor controlling resonant peaking
*/
#pragma once
#include "filter_common.h"

typedef struct {
	F_SIZE xnz1;
	F_SIZE xnz2;
	F_SIZE ynz1;
	F_SIZE ynz2;
	F_SIZE offset;
	tp_coeffs coeffs;
} filter_data;

void so_hpf_calculate_coeffs(float Q, int fc, int fs, filter_data *data);
F_SIZE so_hpf_filter(F_SIZE sample, filter_data *data);
void so_hpf_set_offset(F_SIZE offset, filter_data *data);
F_SIZE so_hpf_get_offset(F_SIZE offset, filter_data *data);
