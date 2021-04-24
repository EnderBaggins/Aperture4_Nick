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

#ifndef _PTC_INJECTOR_CUDA_H_
#define _PTC_INJECTOR_CUDA_H_

#include "core/cached_allocator.hpp"
#include "core/multi_array.hpp"
#include "data/fields.h"
#include "data/particle_data.h"
#include "data/rng_states.h"
#include "framework/system.h"
#include "systems/grid.h"
#include "systems/policies/exec_policy_cuda.hpp"
#include "systems/ptc_injector_new.h"
#include "utils/range.hpp"
#include <thrust/device_ptr.h>
#include <thrust/scan.h>

namespace Aperture {

template <typename Conf>
class ptc_injector<Conf, exec_policy_cuda> : public system_t {
 public:
  using value_t = typename Conf::value_t;
  static std::string name() { return "ptc_injector"; }

  ptc_injector(const Grid<Conf::dim, value_t>& grid) : m_grid(grid) {
    auto ext = grid.extent();
    m_num_per_cell.resize(ext);
    m_cum_num_per_cell.resize(ext);
  }

  ~ptc_injector() {}

  void init() override {
    sim_env().get_data("particles", ptc);
    sim_env().get_data("rng_states", rng_states);
  }

  template <typename FCriteria, typename FDist, typename FNumPerCell,
            typename FWeight>
  void inject(const FCriteria& fc, const FNumPerCell& fn, const FDist& fd,
              const FWeight& fw, uint32_t flag = 0) {
    using policy = exec_policy_cuda<Conf>;
    m_num_per_cell.assign_dev(0);
    m_cum_num_per_cell.assign_dev(0);

    Logger::print_debug_all("Before calculating num_per_cell");
    // First compute the number of particles per cell
    policy::launch(
        [] __device__(auto num_per_cell, auto fc, auto fn) {
          auto& grid = policy::grid();
          auto ext = grid.extent();

          for (auto idx : grid_stride_range(Conf::begin(ext), Conf::end(ext))) {
            auto pos = get_pos(idx, ext);
            if (grid.is_in_bound(pos)) {
              if (fc(pos, grid, ext)) {
                num_per_cell[idx] = fn(pos, grid, ext);
              }
            }
          }
        },
        m_num_per_cell, fc, fn);
    policy::sync();
    // Logger::print_debug("Finished calculating num_per_cell");

    // Compute cumulative number per cell
    thrust::device_ptr<int> p_num_per_cell(m_num_per_cell.dev_ptr());
    thrust::device_ptr<int> p_cum_num_per_cell(m_cum_num_per_cell.dev_ptr());

    thrust::exclusive_scan(p_num_per_cell, p_num_per_cell + m_grid.size(),
                           p_cum_num_per_cell);
    CudaCheckError();
    m_num_per_cell.copy_to_host();
    m_cum_num_per_cell.copy_to_host();
    int new_particles = (m_cum_num_per_cell[m_grid.size() - 1] +
                         m_num_per_cell[m_grid.size() - 1]);
    auto num = ptc->number();
    // Logger::print_debug("Current num is {}, injecting {}", num,
    // new_particles);
    Logger::print_info_all("Injecting {}", new_particles);

    // Actually create the particles
    policy::launch(
        [flag, num] __device__(auto ptc, auto states, auto num_per_cell,
                               auto cum_num_per_cell, auto fd, auto fw) {
          auto& grid = policy::grid();
          auto ext = grid.extent();
          rng_t rng(states);

          for (auto idx : grid_stride_range(Conf::begin(ext), Conf::end(ext))) {
            auto pos = get_pos(idx, ext);
            if (grid.is_in_bound(pos)) {
              for (int i = 0; i < num_per_cell[idx]; i += 2) {
                uint32_t offset_e = num + cum_num_per_cell[idx] + i;
                uint32_t offset_p = offset_e + 1;

                ptc.x1[offset_e] = ptc.x1[offset_p] = rng.uniform<value_t>();
                ptc.x2[offset_e] = ptc.x2[offset_p] = rng.uniform<value_t>();
                ptc.x3[offset_e] = ptc.x3[offset_p] = rng.uniform<value_t>();

                auto p = fd(pos, grid, ext, rng, PtcType::electron);
                ptc.p1[offset_e] = p[0];
                ptc.p2[offset_e] = p[1];
                ptc.p3[offset_e] = p[2];
                ptc.E[offset_e] = math::sqrt(1.0f + p.dot(p));

                p = fd(pos, grid, ext, rng, PtcType::positron);
                ptc.p1[offset_p] = p[0];
                ptc.p2[offset_p] = p[1];
                ptc.p3[offset_p] = p[2];
                ptc.E[offset_p] = math::sqrt(1.0f + p.dot(p));

                ptc.weight[offset_e] = ptc.weight[offset_p] =
                    fw(pos, grid, ext);
                ptc.cell[offset_e] = idx.linear;
                ptc.cell[offset_p] = idx.linear;
                ptc.flag[offset_e] = set_ptc_type_flag(flag, PtcType::electron);
                ptc.flag[offset_p] = set_ptc_type_flag(flag, PtcType::positron);
              }
            }
          }
        },
        ptc, rng_states, m_num_per_cell, m_cum_num_per_cell, fd, fw);
    policy::sync();
    Logger::print_debug_all("Finished creating particles");
    ptc->add_num(new_particles);
  }

 private:
  const Grid<Conf::dim, value_t>& m_grid;
  nonown_ptr<particle_data_t> ptc;
  nonown_ptr<rng_states_t> rng_states;

  multi_array<int, Conf::dim> m_num_per_cell, m_cum_num_per_cell;
};

}  // namespace Aperture

#endif  // _PTC_INJECTOR_CUDA_H_
