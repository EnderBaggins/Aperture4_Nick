/*
 * Copyright (c) 2022 Alex Chen.
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

#include "data/fields.h"
#include "data/particle_data.h"
#include "data/rng_states.h"
#include "framework/config.h"
#include "framework/environment.h"
#include "systems/data_exporter.h"
#include "systems/field_solver_cartesian.h"
#include "systems/policies/coord_policy_cartesian.hpp"
#include "systems/ptc_updater_base.h"

using namespace Aperture;

#ifdef GPU_ENABLED
#include "systems/policies/exec_policy_cuda.hpp"
template <typename Conf>
using exec_policy = exec_policy_cuda<Conf>;
#else
#include "systems/policies/exec_policy_host.hpp"
template <typename Conf>
using exec_policy = exec_policy_host<Conf>;
#endif

int
main(int argc, char *argv[]) {
  typedef Config<3> Conf;
  auto &env = sim_environment::instance(&argc, &argv, true);
  typedef typename Conf::value_t value_t;

  // env.params().add("dt", 3.5e-3);
  // env.params().add("N", std::vector<int64_t>({128, 128, 128}));
  // env.params().add("guard", std::vector<int64_t>({2, 2, 2}));
  // env.params().add("size", std::vector<double>({1.0, 1.0, 1.0}));
  // env.params().add("lower", std::vector<double>({0.0, 0.0, 0.0}));
  // env.params().add("periodic_boundary",
  //                  std::vector<bool>({false, false, false}));
  // env.params().add("damping_boundary",
  //                  std::vector<bool>({true, true, true, true, true, true}));
  // env.params().add("use_implicit", false);
  // env.params().add("pml_length", 8l);
  // env.params().add("fld_output_interval", 10l);

  domain_comm<Conf, exec_policy> comm;
  // auto &grid = *(env.register_system<grid_t<Conf>>(comm));
  grid_t<Conf> grid(comm);
  auto pusher = env.register_system<
      ptc_updater<Conf, exec_policy, coord_policy_cartesian>>(grid, &comm);
  auto solver = env.register_system<
      field_solver<Conf, exec_policy, coord_policy_cartesian>>(grid, &comm);
  auto exporter =
      env.register_system<data_exporter<Conf, exec_policy>>(grid, &comm);

  env.init();

  particle_data_t *ptc;
  env.get_data("particles", &ptc);

  for (int i = 0; i < 1; i++) {
    ptc_append_global(exec_tags::host{}, *ptc, grid, {0.35f, 0.35f, 0.35f},
                      {0.0f, 10.0f, 0.0f}, 1.0f,
                      set_ptc_type_flag(0, PtcType::electron));
  }
  Logger::print_info("finished initializing a single particle");

  env.run();

  return 0;
}
