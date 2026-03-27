#include "so_hpf.h"
#include "log.h"

void so_hpf_calculate_coeffs(float Q, int fc, int fs, filter_data *data)
{
	// PRINTF_BASE("so_hpf_calculate_coeffs: Q=%f, fc=%d, fs=%d\n", Q, fc, fs);
	F_SIZE w = 2.0 * pi * fc / fs;
	F_SIZE d = 1.0 / Q;
	F_SIZE b = 0.5*(1.0 - (d / 2)*sin(w)) / (1.0 + (d / 2.0)*sin(w));
	F_SIZE g = (0.5 + b)*cos(w);
	data->coeffs.a0 = (0.5 + b + g) / 2.0;
	data->coeffs.a1 = -(0.5 + b + g);
	data->coeffs.a2 = data->coeffs.a0;
	data->coeffs.b1 = -2.0 * g;
	data->coeffs.b2 = 2.0 * b;
}

F_SIZE so_hpf_filter(F_SIZE sample, filter_data *data)
{
	F_SIZE xn = sample;
	F_SIZE yn = data->coeffs.a0*xn + data->coeffs.a1*data->xnz1 + data->coeffs.a2*data->xnz2 - data->coeffs.b1*data->ynz1 - data->coeffs.b2*data->ynz2;

	data->xnz2 = data->xnz1;
	data->xnz1 = xn;
	data->ynz2 = data->ynz1;
	data->ynz1 = yn;

	return(yn + data->offset);
}

void so_hpf_set_offset(F_SIZE offset, filter_data *data)
{
	data->offset = offset;
}

F_SIZE so_hpf_get_offset(F_SIZE offset, filter_data *data)
{
	return data->offset;
}

