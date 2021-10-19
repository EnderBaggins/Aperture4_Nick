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

#include "boundary_condition.h"
#include "framework/config.h"
#include "systems/grid_sph.hpp"
#include "utils/kernel_helper.hpp"

namespace Aperture {

template <typename Conf>
void
boundary_condition<Conf>::init() {
  sim_env().get_data("Edelta", &E);
  sim_env().get_data("E0", &E0);
  sim_env().get_data("Bdelta", &B);
  sim_env().get_data("B0", &B0);

  sim_env().params().get_value("E0", m_E0);
  sim_env().params().get_value("omega_t", m_omega_t);
  sim_env().params().get_value("Bp", m_Bp);
}

template <typename Conf>
void
boundary_condition<Conf>::update(double dt, uint32_t step) {
  auto ext = m_grid.extent();
  typedef typename Conf::idx_t idx_t;
  typedef typename Conf::value_t value_t;

  value_t time = sim_env().get_time();
  value_t Bp = m_Bp;
  value_t omega;
  // if (m_omega_t * time < 5000.0)
  value_t phase = time * m_omega_t;
  if (phase < 8.0)
    omega = m_E0 * sin(2.0 * M_PI * phase);
  else
    omega = 0.0;
  Logger::print_debug("time is {}, Omega is {}", time, omega);

  kernel_launch([ext, time, omega, Bp] __device__ (auto e, auto b, auto e0, auto b0) {
      auto& grid = dev_grid<Conf::dim, typename Conf::value_t>();
      for (auto n1 : grid_stride_range(0, grid.dims[1])) {
        value_t theta = grid_sph_t<Conf>::theta(grid.template pos<1>(n1, false));
        value_t theta_s = grid_sph_t<Conf>::theta(grid.template pos<1>(n1, true));

        // For quantities that are not continuous across the surface
        for (int n0 = 0; n0 < grid.guard[0] + 1; n0++) {
          auto idx = idx_t(index_t<2>(n0, n1), ext);
          e[0][idx] = 0.0;
          b[1][idx] = 0.0;
          b[2][idx] = 0.0;
        }
        // For quantities that are continuous across the surface
        for (int n0 = 0; n0 < grid.guard[0] + 1; n0++) {
          auto idx = idx_t(index_t<2>(n0, n1), ext);
          value_t r = grid_sph_t<Conf>::radius(grid.template pos<0>(n0, false));
          value_t r_s = grid_sph_t<Conf>::radius(grid.template pos<0>(n0, true));
          b[0][idx] = 0.0;
          e[1][idx] = 0.0;
          // if (theta_s > 0.7 && theta_s < 1.2)
          // e[2][idx] = -omega * sin(theta_s) * r_s * b0[0][idx];
          e[2][idx] = -omega * sin(theta_s) * r_s * Bp;
          // else
          //   e[2][idx] = 0.0;
          // e[2][idx] = 0.0;
        }
      }
    }, E->get_ptrs(), B->get_ptrs(), E0->get_ptrs(), B0->get_ptrs());
  CudaSafeCall(cudaDeviceSynchronize());
}


template class boundary_condition<Config<2>>;

}
