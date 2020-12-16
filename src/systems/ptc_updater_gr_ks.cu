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

#include "core/math.hpp"
#include "data/curand_states.h"
#include "framework/config.h"
#include "helpers/ptc_update_helper.hpp"
#include "ptc_updater_gr_ks.h"
#include "systems/physics/geodesic_ks.hpp"
#include "systems/physics/metric_kerr_schild.hpp"
#include "utils/kernel_helper.hpp"
#include "utils/timer.h"
#include "utils/util_functions.h"

namespace Aperture {

namespace {

HOST_DEVICE void
gr_ks_boris_update(Scalar a, const vec_t<Scalar, 3> &x, vec_t<Scalar, 3> &u,
                   const vec_t<Scalar, 3> &B, const vec_t<Scalar, 3> &D,
                   Scalar dt, Scalar e_over_m) {
  Scalar sth = math::sin(x[1]);
  Scalar cth = math::cos(x[1]);

  Scalar g_13 = Metric_KS::g_13(a, x[0], sth, cth);
  Scalar g_11 = Metric_KS::g_11(a, x[0], sth, cth);
  Scalar g_22 = Metric_KS::g_22(a, x[0], sth, cth);
  Scalar g_33 = Metric_KS::g_33(a, x[0], sth, cth);
  Scalar gu11 = Metric_KS::gu11(a, x[0], sth, cth);
  Scalar gu22 = Metric_KS::gu22(a, x[0], sth, cth);
  Scalar gu33 = Metric_KS::gu33(a, x[0], sth, cth);
  Scalar gu13 = Metric_KS::gu13(a, x[0], sth, cth);
  Scalar sqrtg = Metric_KS::sqrt_gamma(a, x[0], sth, cth);

  vec_t<Scalar, 3> D_l = 0.0f;
  D_l[0] = g_11 * D[0] + g_13 * D[2];
  D_l[1] = g_22 * D[1];
  D_l[2] = g_33 * D[2] + g_13 * D[0];
  D_l *= 0.5f * dt * e_over_m * Metric_KS::alpha(a, x[0], sth, cth);

  vec_t<Scalar, 3> u_minus = u + D_l;

  vec_t<Scalar, 3> t =
      B * 0.5f * dt * e_over_m / Metric_KS::u0(a, x[0], sth, cth, u_minus);
  Scalar t2 = g_11 * t[0] * t[0] + g_22 * t[1] * t[1] + g_33 * t[2] * t[2] +
              2.0f * g_13 * t[0] * t[2];
  Scalar s = 2.0f / (1.0f + t2);

  vec_t<Scalar, 3> u_prime = u_minus;
  u_prime[0] += sqrtg * (gu22 * u_minus[1] * t[2] -
                         (gu33 * u_minus[2] + gu13 * u_minus[0]) * t[1]);
  u_prime[1] += sqrtg * ((gu33 * u_minus[2] + gu13 * u_minus[0]) * t[0] -
                         (gu11 * u_minus[0] + gu13 * u_minus[2]) * t[2]);
  u_prime[2] += sqrtg * ((gu11 * u_minus[0] + gu13 * u_minus[2]) * t[1] -
                         gu22 * u_minus[1] * t[0]);

  u = u_minus + D_l;
  u[0] += sqrtg *
          (gu22 * u_prime[1] * t[2] -
           (gu33 * u_prime[2] + gu13 * u_prime[0]) * t[1]) *
          s;
  u[1] += sqrtg *
          ((gu33 * u_prime[2] + gu13 * u_prime[0]) * t[0] -
           (gu11 * u_prime[0] + gu13 * u_prime[2]) * t[2]) *
          s;
  u[2] += sqrtg *
          ((gu11 * u_prime[0] + gu13 * u_prime[2]) * t[1] -
           gu22 * u_prime[1] * t[0]) *
          s;
}

HOST_DEVICE void
gr_ks_geodesic_advance(Scalar a, Scalar dt, vec_t<Scalar, 3> &x,
                       vec_t<Scalar, 3> &u, bool is_photon = false,
                       int n_iter = 3) {
  vec_t<Scalar, 3> x0 = x, x1 = x;
  vec_t<Scalar, 3> u0 = u, u1 = u;

  for (int i = 0; i < n_iter; i++) {
    auto x_tmp = (x0 + x) * 0.5;
    auto u_tmp = (u0 + u) * 0.5;
    x1 = x0 + geodesic_ks_x_rhs(a, x_tmp, u_tmp, is_photon) * dt;
    u1 = u0 + geodesic_ks_u_rhs(a, x_tmp, u_tmp, is_photon) * dt;
    x = x1;
    u = u1;
  }
}

template <typename Conf>
void
process_j_rho(vector_field<Conf>& j,
              typename ptc_updater_cu<Conf>::rho_ptrs_t& rho_ptrs,
              int num_species, const grid_ks_t<Conf>& grid,
              typename Conf::value_t dt) {
  kernel_launch(
      [dt, num_species] __device__(auto j, auto rho, auto grid_ptrs) {
        auto& grid = dev_grid<Conf::dim, typename Conf::value_t>();
        auto ext = grid.extent();
        for (auto idx : grid_stride_range(Conf::begin(ext), Conf::end(ext))) {
          auto pos = get_pos(idx, ext);
          // if (grid.is_in_bound(pos)) {
          auto w = grid.delta[0] * grid.delta[1] / dt;
          j[0][idx] *= w / grid_ptrs.Ad[0][idx];
          j[1][idx] *= w / grid_ptrs.Ad[1][idx];
          j[2][idx] /= grid_ptrs.Ad[2][idx];
          for (int n = 0; n < num_species; n++) {
            rho[n][idx] /= grid_ptrs.Ad[2][idx];
          }
          // }
          typename Conf::value_t theta = grid.template pos<1>(pos[1], true);
          if (math::abs(theta) < 0.1 * grid.delta[1]) {
            // j[1][idx] = 0.0;
            j[2][idx] = 0.0;
          }
        }
      },
      j.get_ptrs(), rho_ptrs.dev_ptr(), grid.get_grid_ptrs());
  // TODO: Is this necessary?
  // auto a = grid.a;
  // kernel_launch([a, dt, num_species] __device__(auto j, auto rho, auto grid_ptrs) {
  //       auto& grid = dev_grid<Conf::dim, typename Conf::value_t>();
  //       auto ext = grid.extent();
  //       for (auto idx : grid_stride_range(Conf::begin(ext), Conf::end(ext))) {
  //         auto pos = get_pos(idx, ext);
  //         Scalar r = grid_ks_t<Conf>::radius(grid.template pos<0>(pos[0], false));
  //         Scalar th = grid_ks_t<Conf>::theta(grid.template pos<1>(pos[1], true));
  //         auto sth = math::sin(th);
  //         auto cth = math::cos(th);

  //         Scalar rho_total = 0.0f;
  //         for (int n = 0; n < num_species; n++) {
  //           rho_total += 0.5f * (rho[n][idx] + rho[n][idx.inc_x()]);
  //         }
  //         j[0][idx] -= Metric_KS::beta1(a, r, sth, cth) * rho_total;
  //       }
  //   }, j.get_ptrs(), rho_ptrs.dev_ptr(), grid.get_grid_ptrs());
  CudaSafeCall(cudaDeviceSynchronize());
  CudaCheckError();
}

template <typename Conf>
void
ptc_outflow(particle_data_t& ptc, const grid_ks_t<Conf>& grid,
            int damping_length) {
  auto ptc_num = ptc.number();
  kernel_launch(
      [ptc_num, damping_length] __device__(auto ptc, auto gp) {
        auto& grid = dev_grid<Conf::dim, typename Conf::value_t>();
        for (auto n : grid_stride_range(0, ptc_num)) {
          auto c = ptc.cell[n];
          if (c == empty_cell) continue;

          auto idx = typename Conf::idx_t(c, grid.extent());
          auto pos = idx.get_pos();
          auto flag = ptc.flag[n];
          if (check_flag(flag, PtcFlag::ignore_EM)) continue;
          if (pos[0] > grid.dims[0] - damping_length + 2) {
            flag |= flag_or(PtcFlag::ignore_EM);
            ptc.flag[n] = flag;
          }
        }
      },
      ptc.get_dev_ptrs(), grid.get_grid_ptrs());
  CudaSafeCall(cudaDeviceSynchronize());
  CudaCheckError();
}

}  // namespace

template <typename Conf>
ptc_updater_gr_ks_cu<Conf>::ptc_updater_gr_ks_cu(sim_environment &env,
                                                 const grid_ks_t<Conf> &grid,
                                                 const domain_comm<Conf> *comm)
    : ptc_updater_cu<Conf>(env, grid, comm), m_ks_grid(grid) {}

template <typename Conf>
void
ptc_updater_gr_ks_cu<Conf>::init() {
  ptc_updater_cu<Conf>::init();

  this->m_env.params().get_value("bh_spin", m_a);
  this->m_env.params().get_value("damping_length", m_damping_length);
}

template <typename Conf>
void
ptc_updater_gr_ks_cu<Conf>::register_data_components() {
  ptc_updater_cu<Conf>::register_data_components();
}

template <typename Conf>
void
ptc_updater_gr_ks_cu<Conf>::update_particles(double dt, uint32_t step) {
  value_t a = m_a;
  auto ptc_num = this->ptc->number();
  using spline_t = typename ptc_updater<Conf>::spline_t;
  using idx_t = typename Conf::idx_t;

  if (ptc_num > 0) {
    auto ptc_kernel = [a, ptc_num, dt, step] __device__(
                          auto ptc, auto B, auto D, auto J, auto Rho,
                          auto rho_interval) {
      auto &grid = dev_grid<Conf::dim, typename Conf::value_t>();
      auto ext = grid.extent();

      for (auto n : grid_stride_range(0, ptc_num)) {
        uint32_t cell = ptc.cell[n];
        if (cell == empty_cell) continue;

        auto idx = idx_t(cell, ext);
        auto pos = idx.get_pos();

        vec_t<value_t, 3> x(ptc.x1[n], ptc.x2[n], ptc.x3[n]);
        vec_t<value_t, 3> u(ptc.p1[n], ptc.p2[n], ptc.p3[n]);
        // value_t u0 = ptc.E[n];

        vec_t<value_t, 3> x_global = grid.pos_global(pos, x);
        x_global[0] = grid_ks_t<Conf>::radius(x_global[0]);
        x_global[1] = grid_ks_t<Conf>::theta(x_global[1]);

        auto flag = ptc.flag[n];
        int sp = get_ptc_type(flag);
        value_t q_over_m = dev_charges[sp] / dev_masses[sp];

        if (!check_flag(flag, PtcFlag::ignore_EM)) {
          auto interp = interpolator<spline_t, Conf::dim>{};
          vec_t<value_t, 3> Dp, Bp;
          Dp[0] = interp(D[0], x, idx, stagger_t(0b110));
          Dp[1] = interp(D[1], x, idx, stagger_t(0b101));
          Dp[2] = interp(D[2], x, idx, stagger_t(0b011));
          Bp[0] = interp(B[0], x, idx, stagger_t(0b001));
          Bp[1] = interp(B[1], x, idx, stagger_t(0b010));
          Bp[2] = interp(B[2], x, idx, stagger_t(0b100));
          gr_ks_boris_update(a, x_global, u, Bp, Dp, dt, q_over_m);
        }

        vec_t<value_t, 3> new_x = x_global;
        gr_ks_geodesic_advance(a, dt, new_x, u, false);
        new_x[0] = x[0] + (grid_ks_t<Conf>::from_radius(new_x[0]) -
                           grid_ks_t<Conf>::from_radius(x_global[0])) *
                              grid.inv_delta[0];
        new_x[1] = x[1] + (grid_ks_t<Conf>::from_theta(new_x[1]) -
                           grid_ks_t<Conf>::from_theta(x_global[1])) *
                              grid.inv_delta[1];
        vec_t<int, 2> dc = 0;
        dc[0] = math::floor(new_x[0]);
        dc[1] = math::floor(new_x[1]);
        pos[0] += dc[0];
        pos[1] += dc[1];
        ptc.x1[n] = new_x[0] - (Scalar)dc[0];
        ptc.x2[n] = new_x[1] - (Scalar)dc[1];
        ptc.x3[n] = new_x[2];
        ptc.cell[n] = idx_t(pos, ext).linear;

        ptc.p1[n] = u[0];
        ptc.p2[n] = u[1];
        ptc.p3[n] = u[2];
        ptc.E[n] = Metric_KS::u0(a, x_global[0], math::sin(x_global[1]),
                                 math::cos(x_global[1]), u);

        if (!check_flag(flag, PtcFlag::ignore_current)) {
          auto weight = dev_charges[sp] * ptc.weight[n];

          deposit_2d<spline_t>(x, new_x, dc, (new_x - x_global) / dt, J, Rho,
                               idx, weight, sp, true);
        }
      }
    };

    kernel_launch(ptc_kernel, this->ptc->get_dev_ptrs(),
                  this->B->get_const_ptrs(), this->E->get_const_ptrs(),
                  this->J->get_ptrs(), this->m_rho_ptrs.dev_ptr(),
                  this->m_rho_interval);
    process_j_rho(*(this->J), this->m_rho_ptrs, this->m_num_species, m_ks_grid, dt);

    if (this->m_comm == nullptr || this->m_comm->domain_info().is_boundary[1]) {
      ptc_outflow(*(this->ptc), m_ks_grid, m_damping_length);
    }

    CudaSafeCall(cudaDeviceSynchronize());
    CudaCheckError();
  }
}

template <typename Conf>
void
ptc_updater_gr_ks_cu<Conf>::move_photons_2d(value_t dt, uint32_t step) {
  value_t a = m_a;
  auto ph_num = this->ph->number();

  if (ph_num > 0) {
    auto photon_kernel = [a, ph_num, dt, step] __device__(auto ph, auto rho_ph,
                                                          auto data_interval) {
      auto &grid = dev_grid<Conf::dim, typename Conf::value_t>();
      auto ext = grid.extent();

      for (size_t n : grid_stride_range(0, ph_num)) {
        uint32_t cell = ph.cell[n];
        if (cell == empty_cell) continue;
      }
    };

    kernel_launch(photon_kernel, this->ph->get_dev_ptrs(),
                  this->rho_ph->dev_ndptr(), this->m_data_interval);
  }
}

template <typename Conf>
void
ptc_updater_gr_ks_cu<Conf>::fill_multiplicity(int mult, value_t weight) {
  ptc_updater_cu<Conf>::fill_multiplicity(mult, weight);
}

template class ptc_updater_gr_ks_cu<Config<2>>;

}  // namespace Aperture
