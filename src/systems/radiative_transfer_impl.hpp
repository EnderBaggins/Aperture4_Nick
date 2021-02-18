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

#ifndef _RADIATIVE_TRANSFER_IMPL_H_
#define _RADIATIVE_TRANSFER_IMPL_H_

#include "radiative_transfer.h"

namespace Aperture {

template <class Conf, template <class> class ExecPolicy,
          template <class> class CoordPolicy,
          template <class> class RadiationPolicy>
radiative_transfer<Conf, ExecPolicy, CoordPolicy, RadiationPolicy>::
    radiative_transfer(const grid_t<Conf>& grid, const domain_comm<Conf>* comm)
    : m_grid(grid), m_comm(comm) {
  if (comm != nullptr) {
    m_track_rank = comm->rank();
    m_track_rank <<= 32;
  }

  m_coord_policy = std::make_unique<CoordPolicy<Conf>>(grid);
  m_rad_policy = std::make_unique<RadiationPolicy<Conf>>(grid);
}

template <class Conf, template <class> class ExecPolicy,
          template <class> class CoordPolicy,
          template <class> class RadiationPolicy>
radiative_transfer<Conf, ExecPolicy, CoordPolicy,
                   RadiationPolicy>::~radiative_transfer() = default;

template <class Conf, template <class> class ExecPolicy,
          template <class> class CoordPolicy,
          template <class> class RadiationPolicy>
void
radiative_transfer<Conf, ExecPolicy, CoordPolicy, RadiationPolicy>::init() {
  sim_env().params().get_value("fld_output_interval", m_data_interval);
  sim_env().params().get_value("sort_interval", m_sort_interval);
  sim_env().params().get_value("ph_per_scatter", m_ph_per_scatter);
  sim_env().params().get_value("tracked_fraction", m_tracked_fraction);

  sim_env().get_data("particles", ptc);
  sim_env().get_data("rng_states", rng_states);

  m_rad_policy->init();
}

template <class Conf, template <class> class ExecPolicy,
          template <class> class CoordPolicy,
          template <class> class RadiationPolicy>
void
radiative_transfer<Conf, ExecPolicy, CoordPolicy,
                   RadiationPolicy>::register_data_components() {
  size_t max_ph_num = 10000;
  sim_env().params().get_value("max_ph_num", max_ph_num);

  ph = sim_env().template register_data<photon_data_t>(
      "photons", max_ph_num, ExecPolicy<Conf>::data_mem_type());
  rho_ph = sim_env().template register_data<scalar_field<Conf>>(
      "Rho_ph", this->m_grid, field_type::vert_centered,
      ExecPolicy<Conf>::data_mem_type());
  photon_produced = sim_env().template register_data<scalar_field<Conf>>(
      "photon_produced", this->m_grid, field_type::vert_centered,
      ExecPolicy<Conf>::data_mem_type());
  pair_produced = sim_env().template register_data<scalar_field<Conf>>(
      "pair_produced", this->m_grid, field_type::vert_centered,
      ExecPolicy<Conf>::data_mem_type());
  photon_produced->reset_after_output(true);
  pair_produced->reset_after_output(true);
}

template <class Conf, template <class> class ExecPolicy,
          template <class> class CoordPolicy,
          template <class> class RadiationPolicy>
void
radiative_transfer<Conf, ExecPolicy, CoordPolicy, RadiationPolicy>::update(
    double dt, uint32_t step) {
  emit_photons(dt);
  create_pairs(dt);
}

template <class Conf, template <class> class ExecPolicy,
          template <class> class CoordPolicy,
          template <class> class RadiationPolicy>
void
radiative_transfer<Conf, ExecPolicy, CoordPolicy,
                   RadiationPolicy>::emit_photons(double dt) {
  auto ptc_num = ptc->number();
  if (ptc_num == 0) return;
  // Have to define these variables in this scope in order to capture them in a
  // lambda function
  auto ph_num = ph->number();
  int ph_per_scatter = m_ph_per_scatter;
  auto tracked_fraction = m_tracked_fraction;
  auto track_rank = m_track_rank;

  // Define a variable to hold the moving position in the photon array where we
  // insert new photons
  buffer<unsigned long long int> pos(1);
  pos[0] = 0;
  pos.copy_to_device();

  // Loop over the particle array to test photon emission and produce photons
  ExecPolicy<Conf>::launch(
      [ptc_num, ph_num, ph_per_scatter, tracked_fraction, track_rank] LAMBDA(
          auto ptc, auto ph, auto ph_pos, auto ph_id, auto ph_produced,
          auto states, auto rad_policy) {
        auto& grid = ExecPolicy<Conf>::grid();
        auto ext = grid.extent();
        rng_t rng(states);

        ExecPolicy<Conf>::loop(
            0, ptc_num,
            [&grid, &ext, &rng, ph_num, ph_per_scatter, tracked_fraction,
             track_rank] LAMBDA(auto n, auto& ptc, auto& ph, auto& ph_pos,
                                auto& ph_id, auto& ph_produced,
                                auto& rad_policy) {
              auto cell = ptc.cell[n];
              if (cell == empty_cell) return;

              auto idx = typename Conf::idx_t(cell, ext);
              auto pos = get_pos(idx, ext);

              if (!grid.is_in_bound(pos)) return;
              auto flag = ptc.flag[n];
              int sp = get_ptc_type(flag);
              if (sp == (int)PtcType::ion) return;

              size_t ph_offset =
                  rad_policy.emit_photon(ptc, n, ph, ph_num, ph_pos, rng);

              if (ph_offset != 0) {
                auto w = ptc.weight[n];

                atomic_add(&ph_produced[idx], w * ph_per_scatter);

                for (int i = 0; i < ph_per_scatter; i++) {
                  // Set the photon to be tracked according to the given ratio
                  float u = rng.uniform<float>();
                  if (u < tracked_fraction) {
                    ph.flag[ph_offset + i] = flag_or(PhFlag::tracked);
                    ph.id[ph_offset + i] = track_rank + atomic_add(ph_id, 1);
                  }
                }
              }
            },
            ptc, ph, ph_pos, ph_id, ph_produced, rad_policy);
      },
      ptc, ph, pos, ph->ptc_id(), photon_produced, rng_states, *m_rad_policy);
  ExecPolicy<Conf>::sync();

  pos.copy_to_host();
  ph->add_num(pos[0]);

  Logger::print_info("{} photons are produced!", pos[0]);
}

template <class Conf, template <class> class ExecPolicy,
          template <class> class CoordPolicy,
          template <class> class RadiationPolicy>
void
radiative_transfer<Conf, ExecPolicy, CoordPolicy,
                   RadiationPolicy>::create_pairs(double dt) {
  auto ph_num = ph->number();
  if (ph_num == 0) return;

  // Have to define these variables in this scope in order to capture them in a
  // lambda function
  auto ptc_num = ptc->number();
  auto tracked_fraction = m_tracked_fraction;
  auto track_rank = m_track_rank;

  // Define a variable to hold the moving position in the photon array where we
  // insert new photons
  buffer<unsigned long long int> pos(1);
  pos[0] = 0;
  pos.copy_to_device();
  // Loop over the photons array to test pair production and create the pairs in
  // the particle array
  ExecPolicy<Conf>::launch(
      [ptc_num, ph_num, tracked_fraction, track_rank] LAMBDA(
          auto ph, auto ptc, auto ptc_pos, auto ptc_id, auto pair_produced,
          auto states, auto rad_policy) {
        auto& grid = ExecPolicy<Conf>::grid();
        auto ext = grid.extent();
        rng_t rng(states);

        ExecPolicy<Conf>::loop(
            0, ph_num,
            [&grid, &ext, &rng, ptc_num, tracked_fraction, track_rank] LAMBDA(
                auto n, auto& ph, auto& ptc, auto& ptc_pos, auto& ptc_id,
                auto& pair_produced, auto& rad_policy) {
              auto cell = ph.cell[n];
              if (cell == empty_cell) return;

              auto idx = typename Conf::idx_t(cell, ext);
              auto pos = get_pos(idx, ext);

              if (!grid.is_in_bound(pos)) return;

              size_t ptc_offset =
                  rad_policy.produce_pair(ph, n, ptc, ptc_num, ptc_pos, rng);

              // if (rad_policy.check_produce_pair(ph, n, rng)) {
              if (ptc_offset != 0) {
                auto w = ph.weight[n];

                atomic_add(&pair_produced[idx], 2.0f * w);

                // Set the photon cell to empty (delete the photon)
                ph.cell[n] = empty_cell;

                // Set the photon to be tracked according to the given ratio
                float u = rng.uniform<float>();
                if (u < tracked_fraction) {
                  set_flag(ptc.flag[ptc_offset], PtcFlag::tracked);
                  set_flag(ptc.flag[ptc_offset + 1], PtcFlag::tracked);
                  ptc.id[ptc_offset] = track_rank + atomic_add(ptc_id, 1);
                  ptc.id[ptc_offset + 1] = track_rank + atomic_add(ptc_id, 1);
                }
              }
            },
            ph, ptc, ptc_pos, ptc_id, pair_produced, rad_policy);
      },
      ph, ptc, pos, ptc->ptc_id(), pair_produced, rng_states, *m_rad_policy);
  ExecPolicy<Conf>::sync();

  pos.copy_to_host();
  ptc->add_num(pos[0]);

  Logger::print_info("{} particles are created!", pos[0]);
}

}  // namespace Aperture

#endif  // _RADIATIVE_TRANSFER_IMPL_H_
