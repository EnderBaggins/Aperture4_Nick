/*
 * Copyright (c) 2021 Alex Chen.
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

#ifndef __RANDOM_H_
#define __RANDOM_H_

#include "core/math.hpp"
#include "core/typedefs_and_constants.h"
#include "framework/environment.h"

#ifdef CUDA_ENABLED
#include <curand_kernel.h>
#endif

namespace Aperture {

#ifdef CUDA_ENABLED

typedef curandState rand_state;

struct rng_t {
  __device__ rng_t(rand_state* state) : m_state(state) {
    id = threadIdx.x + blockIdx.x * blockDim.x;
    m_local_state = state[id];
  }
  __device__ ~rng_t() { m_state[id] = m_local_state; }

  // Generates a device random number between 0.0 and 1.0
  template <typename Float>
  __device__ __forceinline__ Float uniform();

  template <typename Float>
  __device__ __forceinline__ Float gaussian(Float sigma);

  int id;
  rand_state* m_state;
  rand_state m_local_state;
};

template <>
__device__ __forceinline__ float
rng_t::uniform() {
  return curand_uniform(&m_local_state);
}

template <>
__device__ __forceinline__ double
rng_t::uniform() {
  return curand_uniform_double(&m_local_state);
}

template <>
__device__ __forceinline__ float
rng_t::gaussian(float sigma) {
  return curand_normal(&m_local_state) * sigma;
}

template <>
__device__ __forceinline__ double
rng_t::gaussian(double sigma) {
  return curand_normal(&m_local_state) * sigma;
}

#else

namespace detail {

constexpr inline uint64_t
rotl(const uint64_t x, int k) {
  return (x << k) | (x >> (64 - k));
}

uint64_t
split_mix_64(uint64_t x) {
  uint64_t z = (x += 0x9e3779b97f4a7c15);
  z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9;
  z = (z ^ (z >> 27)) * 0x94d049bb133111eb;
  return z ^ (z >> 31);
}

}  // namespace detail

struct rand_state {
  uint64_t s[4] = {};

  rand_state(uint64_t seed = default_random_seed) {
    s[0] = detail::split_mix_64(seed);
    s[1] = detail::split_mix_64(s[0]);
    s[2] = detail::split_mix_64(s[1]);
    s[3] = detail::split_mix_64(s[2]);
  }

  rand_state(uint64_t seed[4]) {
    for (int i = 0; i < 4; i++) s[i] = seed[i];
  }
};

struct rng_t {
  rand_state& m_state;

  rng_t(rand_state* state) : m_state(*state) {}

  /*  Written in 2018 by David Blackman and Sebastiano Vigna (vigna@acm.org)

  To the extent possible under law, the author has dedicated all copyright
  and related and neighboring rights to this software to the public domain
  worldwide. This software is distributed without any warranty.

  See <http://creativecommons.org/publicdomain/zero/1.0/>. */
  uint64_t xoshiro256plus() {
    const uint64_t result = m_state.s[0] + m_state.s[3];

    const uint64_t t = m_state.s[1] << 17;

    m_state.s[2] ^= m_state.s[0];
    m_state.s[3] ^= m_state.s[1];
    m_state.s[1] ^= m_state.s[2];
    m_state.s[0] ^= m_state.s[3];

    m_state.s[2] ^= t;

    m_state.s[3] = detail::rotl(m_state.s[3], 45);

    return result;
  }

  template <typename Float>
  inline Float uniform() {
    uint64_t n = xoshiro256plus();
    return n / 18446744073709551615.0;
  }

  template <typename Float>
  inline Float gaussian(double sigma) {
    auto u1 = uniform<Float>();
    auto u2 = uniform<Float>();
    return math::sqrt(-2.0f * math::log(u1)) * math::cos(2.0f * M_PI * u2) *
           sigma;
  }
};

#endif

}  // namespace Aperture

#endif  // __RANDOM_H_
