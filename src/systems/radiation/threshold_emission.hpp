/*
 * Copyright (c) 2020 Alex Chen.
 * This file is part of Aperture (https://github.com/fizban007/Aperture4.git).
 *
 * Aperture is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 *
 * Aperture is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __THRESHOLD_EMISSION_H_
#define __THRESHOLD_EMISSION_H_

#include "core/cuda_control.h"
#include "core/particle_structs.h"
#include "data/curand_states.h"

namespace Aperture {

struct threshold_emission_t {
  float gamma_thr = 30.0f;

  HOST_DEVICE bool check_emit_photon(ptc_ptrs& ptc, uint32_t tid,
                                     cuda_rng_t& rng) const {
    return ptc.E[tid] > gamma_thr;
  }
};

}  // namespace Aperture

#endif  // __THRESHOLD_EMISSION_H_