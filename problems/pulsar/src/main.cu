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

#include "framework/config.h"
#include "framework/environment.h"
#include "systems/boundary_condition.h"
#include "systems/compute_lorentz_factor.h"
#include "systems/data_exporter.h"
#include "systems/field_solver_sph.h"
#include "systems/ph_freepath_dev.h"
#include "systems/ptc_injector_pulsar.h"
#include "systems/ptc_updater_pulsar.h"
#include <iostream>

using namespace std;
using namespace Aperture;

int
main(int argc, char *argv[]) {
  typedef Config<2> Conf;
  sim_environment env(&argc, &argv);

  env.params().add("log_level", (int64_t)LogLevel::debug);
  double Omega = 0.16667;
  env.params().get_value("omega", Omega);

  // auto comm = env.register_system<domain_comm<Conf>>(env);
  auto grid = env.register_system<grid_sph_t<Conf>>(env);
  auto pusher = env.register_system<ptc_updater_pulsar<Conf>>(env, *grid);
  auto lorentz =
      env.register_system<compute_lorentz_factor_cu<Conf>>(env, *grid);
  // auto rad = env.register_system<ph_freepath_dev<Conf>>(env, *grid);
  auto solver = env.register_system<field_solver_sph_cu<Conf>>(env, *grid);
  auto injector = env.register_system<ptc_injector_pulsar<Conf>>(env, *grid);
  injector->add_injector(
      vec<Scalar>(grid->delta[0], 0.0), vec<Scalar>(grid->delta[0], M_PI), 2.0f, 1.0f,
      [] __device__(Scalar x1, Scalar x2, Scalar x3) {
        return math::sin(x2);
      });
  // injector->add_injector(
  //     vec<Scalar>(math::log(0.6 / Omega), 0.5 * M_PI - 0.2),
  //     vec<Scalar>(math::log(1.2 / Omega) - math::log(0.6 / Omega), 0.4), 0.001f,
  //     0.5f);

  auto bc = env.register_system<boundary_condition<Conf>>(env, *grid);
  auto exporter = env.register_system<data_exporter<Conf>>(env, *grid);

  env.init();

  double Bp = 10000.0;
  env.params().get_value("Bp", Bp);
  // Set initial condition
  // set_initial_condition(env, *grid, 0, 1.0, Bp);
  vector_field<Conf> *B0, *B;
  env.get_data("B0", &B0);
  env.get_data("B", &B);

  // Set dipole initial magnetic field
  B0->set_values(0, [Bp](Scalar x, Scalar theta, Scalar phi) {
    Scalar r = grid_sph_t<Conf>::radius(x);
    // return Bp / (r * r);
    return Bp * 2.0 * cos(theta) / cube(r);
  });
  B0->set_values(1, [Bp](Scalar x, Scalar theta, Scalar phi) {
    Scalar r = grid_sph_t<Conf>::radius(x);
    return Bp * sin(theta) / cube(r);
  });
  B->copy_from(*B0);

  // Fill the magnetosphere with some multiplicity
  pusher->fill_multiplicity(2, 1.0);

  // env.run();
  for (int step = env.get_step(); step < env.get_max_steps(); step++) {
    env.update();

    // if (step == 10000) {
    //   injector->add_injector(
    //       vec<Scalar>(math::log(1.0 / Omega), 0.5 * M_PI - 0.2),
    //       vec<Scalar>(math::log(1.5 / Omega) - math::log(1.0 / Omega), 0.4), 0.001f,
    //       0.5f);
    // }
  }
  return 0;
}
