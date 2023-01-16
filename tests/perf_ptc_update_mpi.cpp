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

#include "framework/config.h"
#include "framework/environment.h"
#include "systems/domain_comm.h"
#include "systems/policies.h"
// #include "systems/ptc_updater.h"
#include "systems/ptc_updater_simd.h"
#include "utils/timer.h"
#include <fstream>
#include <iomanip>

using namespace Aperture;

namespace Aperture {
template class ptc_updater<Config<2>, exec_policy_openmp,
                               coord_policy_cartesian>;
template class ptc_updater<Config<3>, exec_policy_openmp,
                               coord_policy_cartesian>;
}  // namespace Aperture

int
main(int argc, char* argv[]) {
  typedef Config<3> Conf3D;

  auto& env = sim_env(&argc, &argv);

  // env.params().add("N", std::vector<int64_t>({64, 64, 64}));
  // env.params().add("guard", std::vector<int64_t>({2, 2, 2}));
  // env.params().add("size", std::vector<double>({2.0, 3.14, 1.0}));
  // env.params().add("lower", std::vector<double>({0.0, 0.0, 0.0}));
  // env.params().add("ranks", std::vector<int64_t>({2, 2, 1}));
  // env.params().add("max_ptc_num", 60000000l);

  auto comm = env.register_system<domain_comm<Conf3D>>();
  auto grid3d = env.register_system<grid_t<Conf3D>>(*comm);
  auto pusher3d = env.register_system<
      // ptc_updater<Conf3D, exec_policy_openmp, coord_policy_cartesian>>(
      ptc_updater_simd<Conf3D, coord_policy_cartesian>>(
      *grid3d, *comm);

  env.init();
  Logger::print_info("3D Case:");


  particle_data_t* ptc;
  env.get_data("particles", &ptc);
  pusher3d->fill_multiplicity(10);
  Logger::print_info("There are {} particles in the array", ptc->number());

  int N = 50;
  double t = 0.0;
  for (int i = 0; i < N; i++) {
    timer::stamp();
    pusher3d->update(0.01, 2);
    double dt = 0.001 * timer::get_duration_since_stamp("us");
    t += dt;
    if (i % 10 == 0) Logger::print_info("Particle update cycle took {}ms", dt);
  }
  t /= N;
  Logger::print_info("Ran particle update {} times, average time {}ms", N, t);
  Logger::print_info("Time per particle: {}ns", t / ptc->number() * 1.0e6);

  return 0;
}
