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

#ifndef __PTC_UPDATE_HELPER_H_
#define __PTC_UPDATE_HELPER_H_

#include "core/cuda_control.h"
#include "core/math.hpp"
#include "core/particle_structs.h"
#include "utils/vec.hpp"
#include <cstdint>

namespace Aperture {

template <typename value_t>
struct EB_t {
  value_t E1, E2, E3, B1, B2, B3;
};

template <int Dim, typename IntType, typename UIntType, typename FloatType>
struct ptc_context {
  UIntType cell;
  vec_t<FloatType, 3> x;
  vec_t<FloatType, 3> new_x;
  // index_t<Dim> dc;
  vec_t<IntType, Dim> dc;
  vec_t<FloatType, 3> p;
  FloatType gamma;

  UIntType flag;
  FloatType weight;
  UIntType sp;

  vec_t<FloatType, 3> E;
  vec_t<FloatType, 3> B;
};

template <int Dim, typename value_t>
struct ph_context {
  uint32_t cell;
  vec_t<value_t, 3> x;
  vec_t<value_t, 3> new_x;
  index_t<Dim> dc;
  vec_t<value_t, 3> p;
  value_t gamma;

  uint32_t flag;
};

template <typename value_t>
HD_INLINE value_t
center2d(value_t sx0, value_t sx1, value_t sy0, value_t sy1) {
  return (2.0f * sx1 * sy1 + sx0 * sy1 + sx1 * sy0 + 2.0f * sx0 * sy0) *
         0.166666666667f;
}

template <typename value_t>
HD_INLINE value_t
movement3d(value_t sx0, value_t sx1, value_t sy0, value_t sy1, value_t sz0,
           value_t sz1) {
  return (sz1 - sz0) * center2d(sx0, sx1, sy0, sy1);
}

template <typename value_t>
HD_INLINE value_t
movement2d(value_t sx0, value_t sx1, value_t sy0, value_t sy1) {
  return (sy1 - sy0) * 0.5f * (sx0 + sx1);
}

template <typename Pusher>
struct pusher_impl_t {
  Pusher pusher;

  template <typename value_t>
  HD_INLINE void operator()(ptc_ptrs& ptc, uint32_t n, EB_t<value_t>& EB,
                            value_t qdt_over_2m, value_t dt) {
    pusher(ptc.p1[n], ptc.p2[n], ptc.p3[n], ptc.E[n], EB.E1, EB.E2, EB.E3,
           EB.B1, EB.B2, EB.B3, qdt_over_2m, dt);
  }
};

template <typename value_t>
HD_INLINE void
deposit_add(value_t* addr, value_t value) {
#ifdef __CUDACC__
  atomicAdd(addr, value);
#else
  *addr += value;
#endif
}

template <int Dim, typename spline_t>
struct deposit_t {
  template <typename value_t, typename ContextType, typename JFieldType,
            typename RhoFieldType, typename idx_t>
  HOST_DEVICE void operator()(ContextType& context, JFieldType& J,
                              RhoFieldType& Rho, idx_t idx, value_t dt,
                              bool deposit_rho = false);
};

template <typename spline_t>
struct deposit_t<1, spline_t> {
  template <typename value_t, typename ContextType, typename JFieldType,
            typename RhoFieldType, typename idx_t>
  HOST_DEVICE void operator()(ContextType& context, JFieldType& J,
                              RhoFieldType& Rho, idx_t idx, value_t dt,
                              bool deposit_rho) {
    spline_t interp;
    int i_0 = (context.dc[0] == -1 ? -spline_t::radius : 1 - spline_t::radius);
    int i_1 = (context.dc[0] == 1 ? spline_t::radius + 1 : spline_t::radius);
    value_t djx = 0.0f;
    for (int i = i_0; i <= i_1; i++) {
      value_t sx0 = interp(-context.x[0] + i);
      value_t sx1 = interp(-context.new_x[0] - context.dc[0] + i);

      // j1 is movement in x1
      auto offset = idx.inc_x(i);
      djx += sx1 - sx0;
      // atomicAdd(&J[0][offset], -weight * djx);
      deposit_add(&J[0][offset], -context.weight * djx / dt);

      // j2 is simply v2 times rho at center
      value_t val1 = 0.5f * (sx0 + sx1);
      // atomicAdd(&J[1][offset], weight * v[1] * val1);
      deposit_add(
          &J[1][offset],
          context.weight * (context.new_x[1] - context.x[1]) / dt * val1);

      // j3 is simply v3 times rho at center
      // atomicAdd(&J[2][offset], weight * v[2] * val1);
      deposit_add(
          &J[2][offset],
          context.weight * (context.new_x[2] - context.x[2]) / dt * val1);

      // rho is deposited at the final position
      if (deposit_rho) {
        // atomicAdd(&Rho[sp][offset], weight * sx1);
        deposit_add(&Rho[context.sp][offset], context.weight * sx1);
      }
    }
  }
};

template <typename spline_t>
struct deposit_t<2, spline_t> {
  template <typename value_t, typename ContextType, typename JFieldType,
            typename RhoFieldType, typename idx_t>
  HOST_DEVICE void operator()(ContextType& context, JFieldType& J,
                              RhoFieldType& Rho, idx_t idx, value_t dt,
                              bool deposit_rho) {
    spline_t interp;

    int j_0 = (context.dc[1] == -1 ? -spline_t::radius : 1 - spline_t::radius);
    int j_1 = (context.dc[1] == 1 ? spline_t::radius + 1 : spline_t::radius);
    int i_0 = (context.dc[0] == -1 ? -spline_t::radius : 1 - spline_t::radius);
    int i_1 = (context.dc[0] == 1 ? spline_t::radius + 1 : spline_t::radius);

    value_t djy[2 * spline_t::radius + 1] = {};
    for (int j = j_0; j <= j_1; j++) {
      value_t sy0 = interp(-context.x[1] + j);
      value_t sy1 = interp(-context.new_x[1] - context.dc[1] + j);

      value_t djx = 0.0f;
      for (int i = i_0; i <= i_1; i++) {
        value_t sx0 = interp(-context.x[0] + i);
        value_t sx1 = interp(-context.new_x[0] - context.dc[0] + i);

        // j1 is movement in x1
        auto offset = idx.inc_x(i).inc_y(j);
        djx += movement2d(sy0, sy1, sx0, sx1);
        if (math::abs(djx) > TINY) {
          deposit_add(&J[0][offset], -context.weight * djx / dt);
        }

        // j2 is movement in x2
        djy[i - i_0] += movement2d(sx0, sx1, sy0, sy1);
        if (math::abs(djy[i - i_0]) > TINY) {
          deposit_add(&J[1][offset], -context.weight * djy[i - i_0] / dt);
        }

        // j3 is simply v3 times rho at center
        deposit_add(&J[2][offset], context.weight *
                                       (context.new_x[2] - context.x[2]) / dt *
                                       center2d(sx0, sx1, sy0, sy1));
        // printf("---- v3 is %f, J3 is %f\n", v3, J[2][offset]);

        // rho is deposited at the final position
        if (deposit_rho) {
          if (math::abs(sx1 * sy1) > TINY) {
            deposit_add(&Rho[context.sp][offset], context.weight * sx1 * sy1);
            // printf("---- rho deposit is %f\n", weight * sx1 * sy1);
          }
        }
      }
    }
  }
};

template <typename spline_t>
struct deposit_t<3, spline_t> {
  template <typename value_t, typename ContextType, typename JFieldType,
            typename RhoFieldType, typename idx_t>
  HOST_DEVICE void operator()(ContextType& context, JFieldType& J,
                              RhoFieldType& Rho, idx_t idx, value_t dt,
                              bool deposit_rho) {
    spline_t interp;

    int k_0 = (context.dc[2] == -1 ? -spline_t::radius : 1 - spline_t::radius);
    int k_1 = (context.dc[2] == 1 ? spline_t::radius + 1 : spline_t::radius);
    int j_0 = (context.dc[1] == -1 ? -spline_t::radius : 1 - spline_t::radius);
    int j_1 = (context.dc[1] == 1 ? spline_t::radius + 1 : spline_t::radius);
    int i_0 = (context.dc[0] == -1 ? -spline_t::radius : 1 - spline_t::radius);
    int i_1 = (context.dc[0] == 1 ? spline_t::radius + 1 : spline_t::radius);

    value_t djz[2 * spline_t::radius + 1][2 * spline_t::radius + 1] = {};
    for (int k = k_0; k <= k_1; k++) {
      value_t sz0 = interp(-context.x[2] + k);
      value_t sz1 = interp(-context.new_x[2] - context.dc[2] + k);

      value_t djy[2 * spline_t::radius + 1] = {};
      for (int j = j_0; j <= j_1; j++) {
        value_t sy0 = interp(-context.x[1] + j);
        value_t sy1 = interp(-context.new_x[1] - context.dc[1] + j);

        value_t djx = 0.0f;
        for (int i = i_0; i <= i_1; i++) {
          value_t sx0 = interp(-context.x[0] + i);
          value_t sx1 = interp(-context.new_x[0] - context.dc[0] + i);

          // j1 is movement in x1
          auto offset = idx.inc_x(i).inc_y(j).inc_z(k);
          djx += movement3d(sy0, sy1, sz0, sz1, sx0, sx1);
          if (math::abs(djx) > TINY) {
            deposit_add(&J[0][offset], -context.weight * djx / dt);
          }
          // Logger::print_debug("J0 is {}", (*J)[0][offset]);

          // j2 is movement in x2
          djy[i - i_0] += movement3d(sz0, sz1, sx0, sx1, sy0, sy1);
          if (math::abs(djy[i - i_0]) > TINY) {
            deposit_add(&J[1][offset], -context.weight * djy[i - i_0] / dt);
          }

          // j3 is movement in x3
          djz[j - j_0][i - i_0] += movement3d(sx0, sx1, sy0, sy1, sz0, sz1);
          if (math::abs(djz[j - j_0][i - i_0]) > TINY) {
            deposit_add(&J[2][offset],
                        -context.weight * djz[j - j_0][i - i_0] / dt);
          }

          // rho is deposited at the final position
          if (deposit_rho) {
            deposit_add(&Rho[context.sp][offset],
                        context.weight * sx1 * sy1 * sz1);
          }
        }
      }
    }
  }
};

#ifdef USE_SIMD

namespace simd {

template <int Dim, typename spline_t>
struct deposit_t {
  template <typename value_t, typename ContextType, typename JFieldType,
            typename RhoFieldType, typename idx_t>
  HOST_DEVICE void operator()(int n, ContextType& context, JFieldType& J,
                              RhoFieldType& Rho, idx_t idx, value_t dt,
                              bool deposit_rho = false);
};

template <typename spline_t>
struct deposit_t<1, spline_t> {
  template <typename value_t, typename ContextType, typename JFieldType,
            typename RhoFieldType, typename idx_t>
  HOST_DEVICE void operator()(int n, ContextType& context, JFieldType& J,
                              RhoFieldType& Rho, idx_t idx, value_t dt,
                              bool deposit_rho) {
    spline_t interp;
    int i_0 = (context.dc[0][n] == -1 ? -spline_t::radius : 1 - spline_t::radius);
    int i_1 = (context.dc[0][n] == 1 ? spline_t::radius + 1 : spline_t::radius);
    value_t djx = 0.0f;
    for (int i = i_0; i <= i_1; i++) {
      value_t sx0 = interp(-context.x[0][n] + i);
      value_t sx1 = interp(-context.new_x[0][n] - context.dc[0][n] + i);

      // j1 is movement in x1
      auto offset = idx.inc_x(i);
      djx += sx1 - sx0;
      deposit_add(&J[0][offset], -context.weight[n] * djx / dt);

      // j2 is simply v2 times rho at center
      value_t val1 = 0.5f * (sx0 + sx1);
      deposit_add(
          &J[1][offset],
          context.weight[n] * (context.new_x[1][n] - context.x[1][n]) / dt * val1);

      // j3 is simply v3 times rho at center
      deposit_add(
          &J[2][offset],
          context.weight[n] * (context.new_x[2][n] - context.x[2][n]) / dt * val1);

      // rho is deposited at the final position
      if (deposit_rho) {
        deposit_add(&Rho[context.sp[n]][offset], context.weight[n] * sx1);
      }
    }
  }
};

template <typename spline_t>
struct deposit_t<2, spline_t> {
  template <typename value_t, typename ContextType, typename JFieldType,
            typename RhoFieldType, typename idx_t>
  HOST_DEVICE void operator()(int n, ContextType& context, JFieldType& J,
                              RhoFieldType& Rho, idx_t idx, value_t dt,
                              bool deposit_rho) {
    spline_t interp;

    int j_0 = (context.dc[1][n] == -1 ? -spline_t::radius : 1 - spline_t::radius);
    int j_1 = (context.dc[1][n] == 1 ? spline_t::radius + 1 : spline_t::radius);
    int i_0 = (context.dc[0][n] == -1 ? -spline_t::radius : 1 - spline_t::radius);
    int i_1 = (context.dc[0][n] == 1 ? spline_t::radius + 1 : spline_t::radius);

    value_t djy[2 * spline_t::radius + 1] = {};
    for (int j = j_0; j <= j_1; j++) {
      value_t sy0 = interp(-context.x[1][n] + j);
      value_t sy1 = interp(-context.new_x[1][n] - context.dc[1][n] + j);

      value_t djx = 0.0f;
      for (int i = i_0; i <= i_1; i++) {
        value_t sx0 = interp(-context.x[0][n] + i);
        value_t sx1 = interp(-context.new_x[0][n] - context.dc[0][n] + i);

        // j1 is movement in x1
        auto offset = idx.inc_x(i).inc_y(j);
        djx += movement2d(sy0, sy1, sx0, sx1);
        if (math::abs(djx) > TINY) {
          deposit_add(&J[0][offset], -context.weight[n] * djx / dt);
        }

        // j2 is movement in x2
        djy[i - i_0] += movement2d(sx0, sx1, sy0, sy1);
        if (math::abs(djy[i - i_0]) > TINY) {
          deposit_add(&J[1][offset], -context.weight[n] * djy[i - i_0] / dt);
        }

        // j3 is simply v3 times rho at center
        deposit_add(&J[2][offset], context.weight[n] *
                                       (context.new_x[2][n] - context.x[2][n]) / dt *
                                       center2d(sx0, sx1, sy0, sy1));
        // printf("---- v3 is %f, J3 is %f\n", v3, J[2][offset]);

        // rho is deposited at the final position
        if (deposit_rho) {
          if (math::abs(sx1 * sy1) > TINY) {
            deposit_add(&Rho[context.sp[n]][offset], context.weight[n] * sx1 * sy1);
            // printf("---- rho deposit is %f\n", weight * sx1 * sy1);
          }
        }
      }
    }
  }
};

template <typename spline_t>
struct deposit_t<3, spline_t> {
  template <typename value_t, typename ContextType, typename JFieldType,
            typename RhoFieldType, typename idx_t>
  HOST_DEVICE void operator()(int n, ContextType& context, JFieldType& J,
                              RhoFieldType& Rho, idx_t idx, value_t dt,
                              bool deposit_rho) {
    spline_t interp;

    int k_0 = (context.dc[2][n] == -1 ? -spline_t::radius : 1 - spline_t::radius);
    int k_1 = (context.dc[2][n] == 1 ? spline_t::radius + 1 : spline_t::radius);
    int j_0 = (context.dc[1][n] == -1 ? -spline_t::radius : 1 - spline_t::radius);
    int j_1 = (context.dc[1][n] == 1 ? spline_t::radius + 1 : spline_t::radius);
    int i_0 = (context.dc[0][n] == -1 ? -spline_t::radius : 1 - spline_t::radius);
    int i_1 = (context.dc[0][n] == 1 ? spline_t::radius + 1 : spline_t::radius);

    value_t djz[2 * spline_t::radius + 1][2 * spline_t::radius + 1] = {};
    for (int k = k_0; k <= k_1; k++) {
      value_t sz0 = interp(-context.x[2][n] + k);
      value_t sz1 = interp(-context.new_x[2][n] - context.dc[2][n] + k);

      value_t djy[2 * spline_t::radius + 1] = {};
      for (int j = j_0; j <= j_1; j++) {
        value_t sy0 = interp(-context.x[1][n] + j);
        value_t sy1 = interp(-context.new_x[1][n] - context.dc[1][n] + j);

        value_t djx = 0.0f;
        for (int i = i_0; i <= i_1; i++) {
          value_t sx0 = interp(-context.x[0][n] + i);
          value_t sx1 = interp(-context.new_x[0][n] - context.dc[0][n] + i);

          // j1 is movement in x1
          auto offset = idx.inc_x(i).inc_y(j).inc_z(k);
          djx += movement3d(sy0, sy1, sz0, sz1, sx0, sx1);
          if (math::abs(djx) > TINY) {
            deposit_add(&J[0][offset], -context.weight[n] * djx / dt);
          }
          // Logger::print_debug("J0 is {}", (*J)[0][offset]);

          // j2 is movement in x2
          djy[i - i_0] += movement3d(sz0, sz1, sx0, sx1, sy0, sy1);
          if (math::abs(djy[i - i_0]) > TINY) {
            deposit_add(&J[1][offset], -context.weight[n] * djy[i - i_0] / dt);
          }

          // j3 is movement in x3
          djz[j - j_0][i - i_0] += movement3d(sx0, sx1, sy0, sy1, sz0, sz1);
          if (math::abs(djz[j - j_0][i - i_0]) > TINY) {
            deposit_add(&J[2][offset],
                        -context.weight[n] * djz[j - j_0][i - i_0] / dt);
          }

          // rho is deposited at the final position
          if (deposit_rho) {
            deposit_add(&Rho[context.sp[n]][offset],
                        context.weight[n] * sx1 * sy1 * sz1);
          }
        }
      }
    }
  }
};

}  // namespace simd

#endif

template <typename spline_t, typename value_t, typename JFieldType,
          typename RhoFieldType, typename idx_t>
HOST_DEVICE void
deposit_1d(const vec_t<value_t, 3>& x, const vec_t<value_t, 3>& new_x, int dc,
           const vec_t<value_t, 3>& v, JFieldType& J, RhoFieldType& Rho,
           idx_t idx, value_t weight, int sp, bool deposit_rho = false) {
  spline_t interp;
  int i_0 = (dc == -1 ? -spline_t::radius : 1 - spline_t::radius);
  int i_1 = (dc == 1 ? spline_t::radius + 1 : spline_t::radius);
  value_t djx = 0.0f;
  for (int i = i_0; i <= i_1; i++) {
    value_t sx0 = interp(-x[0] + i);
    value_t sx1 = interp(-new_x[0] + i);

    // j1 is movement in x1
    auto offset = idx.inc_x(i);
    djx += sx1 - sx0;
    // atomicAdd(&J[0][offset], -weight * djx);
    deposit_add(&J[0][offset], -weight * djx);

    // j2 is simply v2 times rho at center
    value_t val1 = 0.5f * (sx0 + sx1);
    // atomicAdd(&J[1][offset], weight * v[1] * val1);
    deposit_add(&J[1][offset], weight * v[1] * val1);

    // j3 is simply v3 times rho at center
    // atomicAdd(&J[2][offset], weight * v[2] * val1);
    deposit_add(&J[2][offset], weight * v[2] * val1);

    // rho is deposited at the final position
    if (deposit_rho) {
      // atomicAdd(&Rho[sp][offset], weight * sx1);
      deposit_add(&Rho[sp][offset], weight * sx1);
    }
  }
}

template <typename spline_t, typename value_t, typename JFieldType,
          typename RhoFieldType, typename idx_t>
HOST_DEVICE void
deposit_2d(const vec_t<value_t, 3>& x, const vec_t<value_t, 3>& new_x,
           const vec_t<int, 2>& dc, value_t v3, JFieldType& J,
           RhoFieldType& Rho, idx_t idx, value_t weight, int sp,
           bool deposit_rho = false) {
  spline_t interp;

  int j_0 = (dc[1] == -1 ? -spline_t::radius : 1 - spline_t::radius);
  int j_1 = (dc[1] == 1 ? spline_t::radius + 1 : spline_t::radius);
  int i_0 = (dc[0] == -1 ? -spline_t::radius : 1 - spline_t::radius);
  int i_1 = (dc[0] == 1 ? spline_t::radius + 1 : spline_t::radius);

  value_t djy[2 * spline_t::radius + 1] = {};
  for (int j = j_0; j <= j_1; j++) {
    value_t sy0 = interp(-x[1] + j);
    value_t sy1 = interp(-new_x[1] + j);

    value_t djx = 0.0f;
    for (int i = i_0; i <= i_1; i++) {
      value_t sx0 = interp(-x[0] + i);
      value_t sx1 = interp(-new_x[0] + i);

      // j1 is movement in x1
      auto offset = idx.inc_x(i).inc_y(j);
      djx += movement2d(sy0, sy1, sx0, sx1);
      if (math::abs(djx) > TINY) {
        deposit_add(&J[0][offset], -weight * djx);
      }

      // j2 is movement in x2
      djy[i - i_0] += movement2d(sx0, sx1, sy0, sy1);
      if (math::abs(djy[i - i_0]) > TINY) {
        deposit_add(&J[1][offset], -weight * djy[i - i_0]);
      }

      // j3 is simply v3 times rho at center
      deposit_add(&J[2][offset], weight * v3 * center2d(sx0, sx1, sy0, sy1));
      // printf("---- v3 is %f, J3 is %f\n", v3, J[2][offset]);

      // rho is deposited at the final position
      if (deposit_rho) {
        if (math::abs(sx1 * sy1) > TINY) {
          deposit_add(&Rho[sp][offset], weight * sx1 * sy1);
          // printf("---- rho deposit is %f\n", weight * sx1 * sy1);
        }
      }
    }
  }
}

template <typename spline_t, typename value_t, typename JFieldType,
          typename RhoFieldType, typename idx_t>
HOST_DEVICE void
deposit_3d(const vec_t<value_t, 3>& x, const vec_t<value_t, 3>& new_x,
           const vec_t<int, 3>& dc, const vec_t<value_t, 3>& v, JFieldType& J,
           RhoFieldType& Rho, idx_t idx, value_t weight, int sp,
           bool deposit_rho = false) {
  spline_t interp;

  int k_0 = (dc[2] == -1 ? -spline_t::radius : 1 - spline_t::radius);
  int k_1 = (dc[2] == 1 ? spline_t::radius + 1 : spline_t::radius);
  int j_0 = (dc[1] == -1 ? -spline_t::radius : 1 - spline_t::radius);
  int j_1 = (dc[1] == 1 ? spline_t::radius + 1 : spline_t::radius);
  int i_0 = (dc[0] == -1 ? -spline_t::radius : 1 - spline_t::radius);
  int i_1 = (dc[0] == 1 ? spline_t::radius + 1 : spline_t::radius);

  value_t djz[2 * spline_t::radius + 1][2 * spline_t::radius + 1] = {};
  for (int k = k_0; k <= k_1; k++) {
    value_t sz0 = interp(-x[2] + k);
    value_t sz1 = interp(-new_x[2] + k);

    value_t djy[2 * spline_t::radius + 1] = {};
    for (int j = j_0; j <= j_1; j++) {
      value_t sy0 = interp(-x[1] + j);
      value_t sy1 = interp(-new_x[1] + j);

      value_t djx = 0.0f;
      for (int i = i_0; i <= i_1; i++) {
        value_t sx0 = interp(-x[0] + i);
        value_t sx1 = interp(-new_x[0] + i);

        // j1 is movement in x1
        auto offset = idx.inc_x(i).inc_y(j).inc_z(k);
        djx += movement3d(sy0, sy1, sz0, sz1, sx0, sx1);
        if (math::abs(djx) > TINY) {
          deposit_add(&J[0][offset], -weight * djx);
        }
        // Logger::print_debug("J0 is {}", (*J)[0][offset]);

        // j2 is movement in x2
        djy[i - i_0] += movement3d(sz0, sz1, sx0, sx1, sy0, sy1);
        if (math::abs(djy[i - i_0]) > TINY) {
          deposit_add(&J[1][offset], -weight * djy[i - i_0]);
        }

        // j3 is movement in x3
        djz[j - j_0][i - i_0] += movement3d(sx0, sx1, sy0, sy1, sz0, sz1);
        if (math::abs(djz[j - j_0][i - i_0]) > TINY) {
          deposit_add(&J[2][offset], -weight * djz[j - j_0][i - i_0]);
        }

        // rho is deposited at the final position
        if (deposit_rho) {
          deposit_add(&Rho[sp][offset], weight * sx1 * sy1 * sz1);
        }
      }
    }
  }
}

}  // namespace Aperture

#endif  // __PTC_UPDATE_HELPER_H_
