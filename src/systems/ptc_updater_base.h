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

#ifndef __PTC_UPDATER_BASE_H_
#define __PTC_UPDATER_BASE_H_

#include "data/data_array.hpp"
#include "data/fields.h"
#include "data/particle_data.h"
#include "framework/system.h"
#include "systems/domain_comm.h"
#include "systems/grid.h"
#include "utils/nonown_ptr.hpp"

namespace Aperture {

class rng_states_t;

template <typename Conf,
          template <class> class ExecPolicy,
          template <class> class CoordPolicy,
          template <class> class PhysicsPolicy>
class ptc_updater : public system_t {
 public:
  typedef typename Conf::value_t value_t;
  static std::string name() { return "ptc_updater"; }

  ptc_updater(const grid_t<Conf>& grid);
  ptc_updater(const grid_t<Conf>& grid, const domain_comm<Conf>& comm);

  void init() override;
  void update(double dt, uint32_t step) override;
  void register_data_components() override;

  void update_particles(value_t dt, uint32_t step);
  void update_photons(value_t dt, uint32_t step);
  void clear_guard_cells();
  void sort_particles();
  void fill_multiplicity(int mult, value_t weight = 1.0);

 private:
  // Grid and communicator which are essential for particle update
  const grid_t<Conf>& m_grid;
  const domain_comm<Conf>* m_comm = nullptr;

  // These are data components that are relevant for particle update
  nonown_ptr<particle_data_t> ptc;
  nonown_ptr<photon_data_t> ph;
  nonown_ptr<vector_field<Conf>> E, B, J;
  data_array<scalar_field<Conf>> Rho;
  nonown_ptr<scalar_field<Conf>> rho_ph;
  nonown_ptr<rng_states_t> rng_states;

  // buffer<ndptr<value_t, Conf::dim>> rho_ptrs;

  // Parameters for this module
  uint32_t m_num_species = 2;
  uint32_t m_data_interval = 1;
  uint32_t m_rho_interval = 1;
  uint32_t m_sort_interval = 20;
  uint32_t m_filter_times = 1;

  // By default the maximum number of species is 8
  vec_t<float, max_ptc_types> m_charges;
  vec_t<float, max_ptc_types> m_masses;
  vec_t<float, max_ptc_types> m_q_over_m;

  void init_charge_mass();
};

}  // namespace Aperture

#endif  // __PTC_UPDATER_BASE_H_
