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

#include "grid_sph.h"
#include "framework/config.h"
#include "framework/environment.h"
#include "systems/domain_comm.h"

namespace Aperture {

double
l1(double r, double rs) {
  return r;
}

double
A2(double r, double rs) {
  return 0.5 * r * r;
}

double
V3(double r, double rs) {
  return r * r * r / 3.0;
}

template <typename Conf>
grid_sph_t<Conf>::~grid_sph_t() {}

template <typename Conf>
void
grid_sph_t<Conf>::compute_coef() {
  double r_g = 0.0;
  this->m_env.params().get_value("compactness", r_g);

  for (int j = 0; j < this->dims[1]; j++) {
    double x2 = this->pos(1, j, false);
    double x2s = this->pos(1, j, true);
    double th = theta(x2);
    double th_minus = theta(x2 - this->delta[1]);
    double ths = theta(x2s);
    double ths_plus = theta(x2s + this->delta[1]);
    for (int i = 0; i < this->dims[0]; i++) {
      double x1 = this->pos(0, i, false);
      double x1s = this->pos(0, i, true);
      double r_minus = radius(x1 - this->delta[0]);
      double r = radius(x1);
      double rs = radius(x1s);
      double rs_plus = radius(x1s + this->delta[0]);
      auto idx = typename Conf::idx_t({i, j}, this->extent());
      auto pos = idx.get_pos();
      this->m_le[0][idx] = l1(rs_plus, r_g) - l1(rs, r_g);
      this->m_le[1][idx] = rs * this->delta[1];
      this->m_le[2][idx] = rs * std::sin(x2s);
      this->m_lb[0][idx] = l1(r, r_g) - l1(r_minus, r_g);
      this->m_lb[1][idx] = r * this->delta[1];
      this->m_lb[2][idx] = r * std::sin(x2);

      this->m_Ae[0][idx] =
          r * r * (std::cos(x2 - this->delta[1]) - std::cos(x2));
      if (std::abs(x2s) < 0.1 * this->delta[1]) {
        this->m_Ae[0][idx] =
            r * r * 2.0 * (1.0 - std::cos(0.5 * this->delta[1]));
      } else if (std::abs(x2s - M_PI) < 0.1 * this->delta[1]) {
        this->m_Ae[0][idx] =
            r * r * 2.0 * (1.0 - std::cos(0.5 * this->delta[1]));
      }
      this->m_Ae[1][idx] = (A2(r, r_g) - A2(r_minus, r_g)) * std::sin(x2);
      // Avoid axis singularity
      // if (std::abs(x2s) < TINY || std::abs(x2s - CONST_PI)
      // < TINY)
      //   m_A2_e(i, j) = 0.5 * std::sin(TINY) *
      //                  (std::exp(2.0 * x1s) -
      //                   std::exp(2.0 * (x1s - this->delta[0])));

      this->m_Ae[2][idx] = (A2(r, r_g) - A2(r_minus, r_g)) * this->delta[1];

      this->m_Ab[0][idx] =
          rs * rs * (std::cos(x2s) - std::cos(x2s + this->delta[1]));
      if (std::abs(x2s) > 0.1 * this->delta[1] &&
          std::abs(x2s - M_PI) > 0.1 * this->delta[1])
        this->m_Ab[1][idx] = (A2(rs_plus, r_g) - A2(rs, r_g)) * std::sin(x2s);
      else
        this->m_Ab[1][idx] = TINY;
      this->m_Ab[2][idx] = (A2(rs_plus, r_g) - A2(rs, r_g)) * this->delta[1];

      this->m_dV[idx] = (V3(r, r_g) - V3(r_minus, r_g)) *
                        (std::cos(x2 - this->delta[1]) - std::cos(x2)) /
                        (this->delta[0] * this->delta[1]);

      if (std::abs(x2s) < 0.1 * this->delta[1] ||
          std::abs(x2s - M_PI) < 0.1 * this->delta[1]) {
        this->m_dV[idx] = (V3(r, r_g) - V3(r_minus, r_g)) * 2.0 *
                          (1.0 - std::cos(0.5 * this->delta[1])) /
                          (this->delta[0] * this->delta[1]);
        // if (i == 100)
        //   Logger::print_info("dV is {}", m_dV(i, j));
      }
    }
  }

  for (int i = 0; i < 3; i++) {
    this->m_le[i].copy_to_device();
    this->m_lb[i].copy_to_device();
    this->m_Ae[i].copy_to_device();
    this->m_Ab[i].copy_to_device();
  }
  this->m_dV.copy_to_device();
}

template class grid_sph_t<Config<2>>;

}  // namespace Aperture
