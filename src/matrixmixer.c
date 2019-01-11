/* Matrix Mixer LV2
 *
 * Copyright (C) 2019 Robin Gareus <robin@gareus.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv2/lv2plug.in/ns/lv2core/lv2.h"

#include "matrixmixer.h"

#define MAX_NPROC 32

typedef struct {
	float const* a_in[N_INPUTS];
	float*       a_out[N_OUTPUTS];
	float*       p_gain[N_INPUTS * N_OUTPUTS];

	float gain[N_INPUTS * N_OUTPUTS];
	float lpf;
} MixMat;

static LV2_Handle
instantiate (
    const LV2_Descriptor*     descriptor,
    double                    rate,
    const char*               bundle_path,
    const LV2_Feature* const* features)
{
	MixMat* self = (MixMat*)calloc (1, sizeof (MixMat));
	if (!self) {
		return NULL;
	}

	self->lpf = 920.f / rate;
	memset (self->gain, 0, sizeof (float) * N_INPUTS * N_OUTPUTS);

	return (LV2_Handle)self;
}

static void
connect_port (
    LV2_Handle handle,
    uint32_t   port,
    void*      data)
{
	MixMat* self = (MixMat*)handle;
	if (port < N_INPUTS) {
		self->a_in[port] = (float const*)data;
	} else if (port < N_INPUTS + N_OUTPUTS) {
		self->a_out[port - N_INPUTS] = (float*)data;
	} else if (port < N_INPUTS + N_OUTPUTS + N_INPUTS * N_OUTPUTS) {
		self->p_gain[port - N_INPUTS - N_OUTPUTS] = (float*)data;
	}
}

static void
process_inplace (float const* const* ins, float* const* outs, float const* const gain, uint32_t n_samples)
{
	/* LV2 allows in-place processing  ins == outs.
	 * we need to buffer outputs to avoid overwriting data
	 */
	float buf[N_OUTPUTS][MAX_NPROC];

	/* 1st row, set outputs */
	float const* in = ins[0];
	for (uint32_t c = 0; c < N_OUTPUTS; ++c) {
		for (uint32_t i = 0; i < n_samples; ++i) {
			buf[c][i] = in[i] * gain[c];
		}
	}

	/* remaining rows, add to output */
	for (uint32_t r = 1; r < N_INPUTS; ++r) {
		in = ins[r];
		for (uint32_t c = 0; c < N_OUTPUTS; ++c) {
			for (uint32_t i = 0; i < n_samples; ++i) {
				buf[c][i] += in[i] * gain[c + r * N_OUTPUTS];
			}
		}
	}

	/* copy back buffer */
	for (uint32_t c = 0; c < N_OUTPUTS; ++c) {
		memcpy (outs[c], buf[c], n_samples * sizeof (float));
	}
}

static void
run (LV2_Handle handle, uint32_t n_samples)
{
	MixMat* self = (MixMat*)handle;

	/* cache on stack */
	float  const* const* const ins  = self->a_in;
	float* const* const        outs = self->a_out;

	float gain_target[N_INPUTS * N_OUTPUTS];
	float gain[N_INPUTS * N_OUTPUTS];

	const float lpf = self->lpf;

	for (int i = 0; i < N_INPUTS * N_OUTPUTS; ++i) {
		gain_target[i] = *self->p_gain[i];
		gain[i]        = self->gain[i];
	}

	/* process in chunks of MAX_NPROC samples */
	uint32_t offset = 0;
	uint32_t remain = n_samples;
	while (remain > 0) {
		uint32_t n_proc = remain > MAX_NPROC ? MAX_NPROC : remain;

		float const* inp[N_INPUTS];
		float*       outp[N_OUTPUTS];

		/* prepare buffer-pointers for this sub-cycle */
		for (uint32_t r = 0; r < N_INPUTS; ++r) {
			inp[r] = &ins[r][offset];
		}
		for (uint32_t c = 0; c < N_OUTPUTS; ++c) {
			outp[c] = &outs[c][offset];
		}

		process_inplace (inp, outp, gain, n_proc);

		/* low-pass filter gain coefficient */
		for (int i = 0; i < N_INPUTS * N_OUTPUTS; ++i) {
			if (fabsf (gain[i] - gain_target[i]) < 1e-6) {
				gain[i] = gain_target[i];
			} else {
				gain[i] += lpf * (gain_target[i] - gain[i]) + 1e-10;
			}
		}

		remain -= n_proc;
		offset += n_proc;
	}

	memcpy (self->gain, gain, N_INPUTS * N_OUTPUTS * sizeof (float));
}

static void
cleanup (LV2_Handle handle)
{
	free (handle);
}

static const void*
extension_data (const char* uri)
{
	return NULL;
}

static const LV2_Descriptor descriptor = {
	MATMIX_URI,
	instantiate,
	connect_port,
	NULL,
	run,
	NULL,
	cleanup,
	extension_data
};

#undef LV2_SYMBOL_EXPORT
#ifdef _WIN32
#    define LV2_SYMBOL_EXPORT __declspec(dllexport)
#else
#    define LV2_SYMBOL_EXPORT  __attribute__ ((visibility ("default")))
#endif
LV2_SYMBOL_EXPORT
const LV2_Descriptor*
lv2_descriptor (uint32_t index)
{
	switch (index) {
		case 0:
			return &descriptor;
		default:
			return NULL;
	}
}
