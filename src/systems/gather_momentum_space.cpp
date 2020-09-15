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

#include "gather_momentum_space.h"
#include "framework/config.h"
#include "framework/environment.h"
#include "framework/params_store.h"

namespace Aperture {

template <typename Conf>
void
gather_momentum_space<Conf>::register_data_components() {
  int downsample =
      this->m_env.params().template get_as<int64_t>("momentum_downsample", 16);
  int num_bins[3] = {256, 256, 256};
  this->m_env.params().get_array("momentum_num_bins", num_bins);
  float lim_lower[3] = {};
  this->m_env.params().get_array("momentum_lower", lim_lower);
  float lim_upper[3] = {};
  this->m_env.params().get_array("momentum_upper", lim_upper);
  this->m_env.params().get_value("fld_output_interval", m_data_interval);

  momentum = m_env.register_data<momentum_space<Conf>>("momentum", m_grid, downsample, num_bins,
                                                       lim_lower, lim_upper);
}

template <typename Conf>
void
gather_momentum_space<Conf>::init() {
  m_env.get_data("particles", &ptc);
}

template <typename Conf>
void
gather_momentum_space<Conf>::update(double dt, uint32_t step) {}

INSTANTIATE_WITH_CONFIG(gather_momentum_space);

}  // namespace Aperture
