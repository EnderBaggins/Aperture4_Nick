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

#include "core/cuda_control.h"
#include "core/multi_array_exp.hpp"
#include "core/ndsubset_dev.hpp"
#include "field_solver_gr_ks.h"
#include "framework/config.h"
#include "framework/environment.h"
#include "utils/kernel_helper.hpp"
#include "utils/timer.h"
// #include <cusparse.h>

namespace Aperture {

namespace {

template <typename Conf>
void
axis_boundary_e(vector_field<Conf> &D, const grid_ks_t<Conf> &grid) {
  typedef typename Conf::idx_t idx_t;
  kernel_launch(
      [] __device__(auto D) {
        auto &grid = dev_grid<Conf::dim>();
        auto ext = grid.extent();
        for (auto n0 : grid_stride_range(0, grid.dims[0])) {
          auto n1_0 = grid.guard[1];
          auto n1_pi = grid.dims[1] - grid.guard[1];
          if (abs(grid_ks_t<Conf>::theta(grid.template pos<1>(n1_0, true))) <
              0.1f * grid.delta[1]) {
            // At the theta = 0 axis

            // Set E_phi and B_theta to zero
            auto idx = idx_t(index_t<2>(n0, n1_0), ext);
            D[2][idx] = 0.0f;
            // e[1][idx] = 0.0;
            // D[1][idx.dec_y()] = D[1][idx];
            // D[0][idx.dec_y()] = D[0][idx];
            // D[0][idx] = 0.0f;
          }
          // printf("boundary pi at %f\n", grid.template pos<1>(n1_pi, true));
          if (abs(grid_ks_t<Conf>::theta(grid.template pos<1>(n1_pi, true)) -
                  M_PI) < 0.1f * grid.delta[1]) {
            // At the theta = pi axis
            auto idx = idx_t(index_t<2>(n0, n1_pi), ext);
            D[2][idx] = 0.0f;
            // e[1][idx] = 0.0;
            // D[1][idx] = D[1][idx.dec_y()];
            // D[0][idx] = D[0][idx.dec_y()];
            // D[0][idx] = 0.0f;
          }
        }
      },
      D.get_ptrs());
  CudaSafeCall(cudaDeviceSynchronize());
  CudaCheckError();
}

template <typename Conf>
void
axis_boundary_b(vector_field<Conf> &B, const grid_ks_t<Conf> &grid) {
  typedef typename Conf::idx_t idx_t;
  kernel_launch(
      [] __device__(auto B) {
        auto &grid = dev_grid<Conf::dim>();
        auto ext = grid.extent();
        for (auto n0 : grid_stride_range(0, grid.dims[0])) {
          for (int n1_0 = grid.guard[1]; n1_0 >= 0; n1_0--) {
            if (grid_ks_t<Conf>::theta(grid.template pos<1>(n1_0, true)) <
                0.1f * grid.delta[1]) {
              // At the theta = 0 axis

              // Set E_phi and B_theta to zero
              auto idx = idx_t(index_t<2>(n0, n1_0), ext);
              B[1][idx] = 0.0f;
              B[2][idx] = 0.0f;
              B[2][idx.dec_y()] = B[2][idx];
              // B[0][idx.dec_y()] = B[0][idx];
            }
          }
          for (int n1_pi = grid.dims[1] - grid.guard[1];
               n1_pi <= grid.dims[1] - 1; n1_pi++) {
            // printf("boundary pi at %f\n", grid.template pos<1>(n1_pi, true));
            if (abs(grid_ks_t<Conf>::theta(grid.template pos<1>(n1_pi, true)) -
                    M_PI) < 0.1f * grid.delta[1]) {
              // At the theta = pi axis
              auto idx = idx_t(index_t<2>(n0, n1_pi), ext);
              B[1][idx] = 0.0f;
              // B[1][idx.dec_y()] = 0.0f;
              // B[2][idx] = B[2][idx.dec_y()];
              B[2][idx] = 0.0f;
              B[2][idx.dec_y()] = 0.0f;
              // B[0][idx] = B[0][idx.dec_y()];
            }
          }
        }
      },
      B.get_ptrs());
  CudaSafeCall(cudaDeviceSynchronize());
  CudaCheckError();
}

template <typename Conf>
void
horizon_boundary(vector_field<Conf> &D, vector_field<Conf> &B,
                 const vector_field<Conf> &D0, const vector_field<Conf> &B0,
                 const grid_ks_t<Conf> &grid, int damping_length,
                 float damping_coef) {
  using value_t = typename Conf::value_t;
  kernel_launch(
      [damping_length, damping_coef] __device__(auto D, auto D0, auto B,
                                                auto B0, auto grid_ptrs) {
        auto &grid = dev_grid<Conf::dim>();
        auto ext = grid.extent();
        for (auto n1 : grid_stride_range(0, grid.dims[1])) {
          auto pos_ref = index_t<2>(damping_length, n1);
          auto idx_ref = Conf::idx(pos_ref, ext);
          for (int n0 = 0; n0 < damping_length; n0++) {
            auto pos = index_t<2>(n0, n1);
            auto idx = Conf::idx(pos, ext);
            value_t lambda =
                1.0f - damping_coef * cube((value_t)(damping_length - 1 - n0) /
                                           (damping_length - 1));

            // B[0][idx] *= lambda;
            // B[1][idx] *= lambda;
            // B[2][idx] *= lambda;
            // D[0][idx] *= lambda;
            // D[1][idx] *= lambda;
            // D[2][idx] *= lambda;

            // B[1][idx] = B[1][idx_ref];
            // B[2][idx] = B[2][idx_ref];
            D[0][idx] = D[0][idx_ref];
            B[0][idx] = B[0][idx_ref];

            // D[1][idx] = D[1][idx_ref];
            // D[2][idx] = D[2][idx_ref];

            // B[1][idx] = B0[1][idx];
            // B[2][idx] = B0[2][idx];
            // D[0][idx] = D0[0][idx];

            // B[0][idx] = B0[0][idx];
            // D[1][idx] = D0[1][idx];
            // D[2][idx] = D0[2][idx];

            B[1][idx] = 0.0f;
            B[2][idx] = 0.0f;
            // D[0][idx] = 0.0f;

            // B[0][idx] = 0.0f;
            D[1][idx] = 0.0f;
            D[2][idx] = 0.0f;
          }
        }
      },
      D.get_ptrs(), D0.get_ptrs(), B.get_ptrs(), B0.get_ptrs(),
      grid.get_grid_ptrs());
  CudaSafeCall(cudaDeviceSynchronize());
  CudaCheckError();
}

template <typename Conf>
void
inner_boundary(vector_field<Conf> &D, vector_field<Conf> &B,
               const grid_ks_t<Conf> &grid, int boundary_cell) {
  using value_t = typename Conf::value_t;
  using namespace Metric_KS;

  kernel_launch(
      [boundary_cell] __device__(auto D, auto B, auto grid_ptrs, auto a) {
        auto &grid = dev_grid<Conf::dim>();
        auto ext = grid.extent();
        for (auto n1 : grid_stride_range(0, grid.dims[1])) {
          int n0 = boundary_cell;
          auto pos = index_t<2>(n0, n1);
          auto idx = Conf::idx(pos, ext);
          // Dr and Br are continuous
          B[0][idx.dec_x()] = B[0][idx];
          D[0][idx.dec_x()] = D[0][idx];

          value_t r =
              grid_ks_t<Conf>::radius(grid.template pos<0>(pos[0], true));
          value_t r_p =
              grid_ks_t<Conf>::radius(grid.template pos<0>(pos[0] + 1, true));
          value_t th_s =
              grid_ks_t<Conf>::theta(grid.template pos<1>(pos[1], true));
          value_t th =
              grid_ks_t<Conf>::theta(grid.template pos<1>(pos[1], false));
          if (th_s < TINY) th_s = 0.01f * grid.delta[1];

          auto sth = math::sin(th_s);
          auto cth = math::cos(th_s);

          // First solve for Bth and Dph
          value_t coef_a = grid_ptrs.ag22dth_h[idx.dec_x()];
          value_t coef_b = grid_ptrs.gbetadth_h[idx.dec_x()] -
                           0.5f * grid_ptrs.gbetadth_h[idx];
          value_t coef_c = 0.5f * sq_gamma_beta(a, r, sth, cth);
          value_t coef_d = ag_33(a, r, sth, cth);

          value_t rhs_F = grid_ptrs.ag22dth_h[idx] * B[1][idx] +
                          0.5f * grid_ptrs.gbetadth_h[idx] * D[2][idx.inc_x()];
          value_t rhs_G =
              ag_33(a, r_p, sth, cth) * D[2][idx.inc_x()] +
              ag_13(a, r_p, sth, cth) * 0.5f * (D[0][idx] + D[0][idx.inc_x()]) +
              sq_gamma_beta(a, r_p, sth, cth) * 0.5f *
                  (B[1][idx] + B[1][idx.inc_x()]) -
              ag_13(a, r, sth, cth) * D[0][idx] -
              0.5f * sq_gamma_beta(a, r, sth, cth) * B[1][idx];

          value_t det = coef_b * coef_c - coef_a * coef_d;
          B[1][idx.dec_x()] = (coef_b * rhs_G - coef_d * rhs_F) / det;
          D[2][idx] = (coef_c * rhs_F - coef_a * rhs_G) / det;

          // value_t Hth1 = grid_ptrs.ag22dth_h[idx] * B[1][idx] +
          //       grid_ptrs.gbetadth_h[idx] * 0.5f *
          //           (D[2][idx.inc_x()] + D[2][idx]);
          // value_t Hth0 = grid_ptrs.ag22dth_h[idx.dec_x()] * B[1][idx.dec_x()]
          // +
          //       grid_ptrs.gbetadth_h[idx.dec_x()] * D[2][idx];

          // value_t Eph1 = ag_33(a, r_p, sth, cth) * D[2][idx.inc_x()] +
          //     ag_13(a, r_p, sth, cth) * 0.5f *
          //     (D[0][idx.inc_x()] + D[0][idx]) +
          //     0.5f * sq_gamma_beta(a, r_p, sth, cth) *
          //     (B[1][idx.inc_x()] + B[1][idx]);
          // value_t Eph0 =
          //     ag_33(a, r, sth, cth) * D[2][idx] +
          //     ag_13(a, r, sth, cth) * D[0][idx] +
          //     sq_gamma_beta(a, r, sth, cth) * 0.5f *
          //     (B[1][idx] + B[1][idx.dec_x()]);

          // if (pos[1] == 250) {
          //   printf("Hth1 is %f, Hth0 is %f, Eph1 is %f, Eph0 is %f\n", Hth1,
          //   Hth0, Eph1, Eph0);
          // }
          // if (Eph0 != Eph0 || Eph1 != Eph1) {
          //   printf("NaN in Eph at pos %d \n", pos[1]);
          //   asm("trap;");
          // }

          // Then solve for Bph and Dth
          r = grid_ks_t<Conf>::radius(grid.template pos<0>(pos[0], false));
          value_t r_m =
              grid_ks_t<Conf>::radius(grid.template pos<0>(pos[0] - 1, false));
          sth = math::sin(th);
          cth = math::cos(th);

          coef_a = ag_33(a, r_m, sth, cth);
          coef_b = -sq_gamma_beta(a, r_m, sth, cth) +
                   0.5f * sq_gamma_beta(a, r, sth, cth);
          coef_c = -0.5 * grid_ptrs.gbetadth_e[idx];
          coef_d = grid_ptrs.ag22dth_e[idx];

          rhs_F =
              ag_33(a, r, sth, cth) * B[2][idx] +
              ag_13(a, r, sth, cth) * 0.5f * (B[0][idx] + B[0][idx.inc_x()]) -
              sq_gamma_beta(a, r, sth, cth) * 0.5f * D[1][idx.inc_x()] -
              ag_13(a, r_m, sth, cth) * B[0][idx];
          rhs_G = grid_ptrs.ag22dth_e[idx.inc_x()] * D[1][idx.inc_x()] -
                  grid_ptrs.gbetadth_e[idx.inc_x()] * 0.5f *
                      (B[2][idx] + B[2][idx.inc_x()]) +
                  grid_ptrs.gbetadth_e[idx] * 0.5f * B[2][idx];

          det = coef_b * coef_c - coef_a * coef_d;
          B[2][idx.dec_x()] = (coef_b * rhs_G - coef_d * rhs_F) / det;
          D[1][idx] = (coef_c * rhs_F - coef_a * rhs_G) / det;

          // value_t Eth1 =
          //     grid_ptrs.ag22dth_e[idx.inc_x()] * D[1][idx.inc_x()] -
          //     grid_ptrs.gbetadth_e[idx.inc_x()] * 0.5f *
          //     (B[2][idx.inc_x()] + B[2][idx]);
          // value_t Eth0 =
          //     grid_ptrs.ag22dth_e[idx] * D[1][idx] -
          //     grid_ptrs.gbetadth_e[idx] * 0.5f *
          //     (B[2][idx] + B[2][idx.dec_x()]);

          // value_t Hph0 = ag_33(a, r_m, sth, cth) * B[2][idx.dec_x()] +
          //     ag_13(a, r_m, sth, cth) * B[0][idx] -
          //     sq_gamma_beta(a, r_m, sth, cth) * D[1][idx];

          // value_t Hph1 = ag_33(a, r, sth, cth) * B[2][idx] +
          //     ag_13(a, r, sth, cth) * 0.5f *
          //     (B[0][idx.inc_x()] + B[0][idx]) -
          //     sq_gamma_beta(a, r, sth, cth) * 0.5f *
          //     (D[1][idx.inc_x()] + D[1][idx]);

          // if (pos[1] == 250) {
          //   printf("Eth1 is %f, Eth0 is %f, Hph1 is %f, Hph0 is %f\n", Eth1,
          //   Eth0, Hph1, Hph0);
          // }
          // for (int j = 0; j < boundary_cell; j++) {
          //   pos = get_pos(j, n1);
          //   idx = Conf::idx(pos, ext);

          //   if (j < boundary_cell - 1) {

          //   }
          // }
        }
      },
      D.get_ptrs(), B.get_ptrs(), grid.get_grid_ptrs(), grid.a);
  CudaSafeCall(cudaDeviceSynchronize());
  CudaCheckError();
}

template <typename Conf>
void
compute_flux(scalar_field<Conf> &flux, const vector_field<Conf> &b,
             const grid_ks_t<Conf> &grid) {
  flux.init();
  auto ext = grid.extent();
  kernel_launch(
      [ext] __device__(auto flux, auto b, auto a, auto grid_ptrs) {
        auto &grid = dev_grid<Conf::dim>();
        for (auto n0 : grid_stride_range(0, grid.dims[0])) {
          auto r = grid_ks_t<Conf>::radius(grid.template pos<0>(n0, true));

          for (int n1 = grid.guard[1]; n1 < grid.dims[1] - grid.guard[1];
               n1++) {
            Scalar th = grid_ks_t<Conf>::theta(grid.template pos<1>(n1, false));
            Scalar th_p =
                grid_ks_t<Conf>::theta(grid.template pos<1>(n1 + 1, true));
            Scalar th_m =
                grid_ks_t<Conf>::theta(grid.template pos<1>(n1, true));
            auto dth = th_p - th_m;

            auto pos = index_t<Conf::dim>(n0, n1);
            auto idx = typename Conf::idx_t(pos, ext);

            flux[idx] = flux[idx.dec_y()] +
                        // b[0][idx] * Metric_KS::sqrt_gamma(a, r, th) * dth;
                        b[0][idx] * grid_ptrs.Ab[0][idx];
          }
        }
      },
      flux.dev_ndptr(), b.get_ptrs(), grid.a, grid.get_grid_ptrs());
  CudaSafeCall(cudaDeviceSynchronize());
  CudaCheckError();
}

template <typename Conf>
void
compute_divs(scalar_field<Conf> &divD, scalar_field<Conf> &divB,
             const vector_field<Conf> &D, const vector_field<Conf> &B,
             const grid_ks_t<Conf> &grid) {
  kernel_launch(
      [] __device__(auto div_e, auto e, auto div_b, auto b, auto grid_ptrs) {
        auto &grid = dev_grid<Conf::dim>();
        auto ext = grid.extent();
        for (auto idx : grid_stride_range(Conf::begin(ext), Conf::end(ext))) {
          auto pos = get_pos(idx, ext);
          if (grid.is_in_bound(pos)) {
            div_b[idx] = (b[0][idx.inc_x()] * grid_ptrs.Ab[0][idx.inc_x()] -
                          b[0][idx] * grid_ptrs.Ab[0][idx] +
                          b[1][idx.inc_y()] * grid_ptrs.Ab[1][idx.inc_y()] -
                          b[1][idx] * grid_ptrs.Ab[1][idx]) /
                         grid_ptrs.Ab[2][idx];

            div_e[idx] = (e[0][idx] * grid_ptrs.Ad[0][idx] -
                          e[0][idx.dec_x()] * grid_ptrs.Ad[0][idx.dec_x()] +
                          e[1][idx] * grid_ptrs.Ad[1][idx] -
                          e[1][idx.dec_y()] * grid_ptrs.Ad[1][idx.dec_y()]) /
                         grid_ptrs.Ad[2][idx];

            if (pos[0] == 3 && pos[1] == 200) {
              printf(
                  "divD is %f, eA0_p is %f, eA0_m is %f, eA1_p is %f, eA1_m is "
                  "%f\n",
                  div_e[idx] * grid_ptrs.Ad[2][idx],
                  e[0][idx] * grid_ptrs.Ad[0][idx],
                  e[0][idx.dec_x()] * grid_ptrs.Ad[0][idx.dec_x()],
                  e[1][idx] * grid_ptrs.Ad[1][idx],
                  e[1][idx.dec_y()] * grid_ptrs.Ad[1][idx.dec_y()]);
            }
          }
        }
      },
      divD[0].dev_ndptr(), D.get_const_ptrs(), divB[0].dev_ndptr(),
      B.get_const_ptrs(), grid.get_grid_ptrs());
}

}  // namespace

template <typename Conf>
field_solver_gr_ks_cu<Conf>::~field_solver_gr_ks_cu() {
  // sp_buffer.resize(0);
}

template <typename Conf>
void
field_solver_gr_ks_cu<Conf>::init() {
  field_solver<Conf>::init();

  this->m_env.params().get_value("bh_spin", m_a);
  Logger::print_info("bh_spin in field solver is {}", m_a);
  this->m_env.params().get_value("implicit_beta", this->m_beta);
  this->m_env.params().get_value("damping_length", m_damping_length);
  this->m_env.params().get_value("damping_coef", m_damping_coef);

  m_tmp_th_field.set_memtype(MemType::device_only);
  m_tmp_th_field.resize(this->m_grid.extent());
  m_tmp_prev_field.set_memtype(MemType::device_only);
  m_tmp_prev_field.resize(this->m_grid.extent());
}

template <typename Conf>
void
field_solver_gr_ks_cu<Conf>::register_data_components() {
  field_solver_cu<Conf>::register_data_components();

  flux = this->m_env.template register_data<scalar_field<Conf>>(
      "flux", this->m_grid, field_type::vert_centered);
}

// template <typename Conf>
// void
// field_solver_gr_ks_cu<Conf>::solve_tridiagonal() {
//   // Solve the assembled tri-diagonal system using cusparse
//   cusparseStatus_t status;
//   auto ext = this->m_grid.extent_less();
// #if USE_DOUBLE
//   status = cusparseDgtsv2(sp_handle, ext[0], ext[1], m_tri_dl.dev_ptr(),
//                           m_tri_d.dev_ptr(), m_tri_du.dev_ptr(),
//                           m_tmp_rhs.dev_ptr(), ext[0], sp_buffer.dev_ptr());
// #else
//   status = cusparseSgtsv2(sp_handle, ext[0], ext[1], m_tri_dl.dev_ptr(),
//                           m_tri_d.dev_ptr(), m_tri_du.dev_ptr(),
//                           m_tmp_rhs.dev_ptr(), ext[0], sp_buffer.dev_ptr());
// #endif
//   CudaSafeCall(cudaDeviceSynchronize());
//   if (status != CUSPARSE_STATUS_SUCCESS) {
//     Logger::print_err("cusparse failure during field update! Error code {}",
//                       status);
//   }
// }

template <typename Conf>
void
field_solver_gr_ks_cu<Conf>::update_Bth(vector_field<Conf> &B,
                                        const vector_field<Conf> &B0,
                                        const vector_field<Conf> &D,
                                        const vector_field<Conf> &D0,
                                        value_t dt) {
  m_tmp_prev_field.copy_from(B[1]);

  // Predictor-corrector approach to update Bth
  auto Bth_kernel = [dt] __device__(auto B, auto B0, auto D, auto D0,
                                    auto B1_0, auto B1_1, auto a, auto grid_ptrs) {
    using namespace Metric_KS;

    auto &grid = dev_grid<Conf::dim>();
    auto ext = grid.extent();
    auto extl = grid.extent_less();
    for (auto idx : grid_stride_range(Conf::begin(ext), Conf::end(ext))) {
      auto pos = get_pos(idx, ext);
      if (grid.is_in_bound(pos)) {
        value_t r =
            grid_ks_t<Conf>::radius(grid.template pos<0>(pos[0], false));
        value_t r_sp =
            grid_ks_t<Conf>::radius(grid.template pos<0>(pos[0] + 1, true));
        value_t r_sm =
            grid_ks_t<Conf>::radius(grid.template pos<0>(pos[0], true));

        value_t th = grid.template pos<1>(pos[1], true);
        if (math::abs(th) < TINY) th = 0.01f * grid.delta[1];

        value_t sth = math::sin(th);
        value_t cth = math::cos(th);
        value_t prefactor = dt / grid_ptrs.Ab[1][idx];

        auto Eph1 =
            ag_33(a, r_sp, sth, cth) * D[2][idx.inc_x()] +
            ag_13(a, r_sp, sth, cth) * 0.5f * (D[0][idx.inc_x()] + D[0][idx]) +
            0.25f * sq_gamma_beta(a, r_sp, sth, cth) *
                (B1_0[idx.inc_x()] + B1_0[idx] + B1_1[idx.inc_x()] + B1_1[idx]);

        auto Eph0 =
            ag_33(a, r_sm, sth, cth) * D[2][idx] +
            ag_13(a, r_sm, sth, cth) * 0.5f * (D[0][idx] + D[0][idx.dec_x()]) +
            0.25f * sq_gamma_beta(a, r_sm, sth, cth) *
                (B1_0[idx] + B1_0[idx.dec_x()] + B1_1[idx] + B1_1[idx.dec_x()]);

        B[1][idx] = B1_0[idx] - prefactor * (Eph0 - Eph1);

        // if (pos[0] == 6 && pos[1] == 200) {
        // printf(
        //     "Eph1 is %f, Eph0 is %f, dEphi is %f, B1 is %f, "
        //     "rhs is %f\n",
        //     Eph1, Eph0, (Eph0 - Eph1), B[1][idx],
        //     rhs[idx]);
        // printf("ag33 is %f, ag13 is %f, sqgb is %f\n",
        //        ag_33(a, r, sth, cth), ag_13(a, r, sth, cth),
        //        sq_gamma_beta(a, r, sth, cth));
        // }
        // if (pos[1] == 2 && pos[0] == 200)
        //   printf("rhs is %f, D0 is %f, B1 is %f\n", rhs[idx], D[0][idx],
        //   B[1][idx]);
      }
    }
  };
  kernel_launch(Bth_kernel, B.get_ptrs(), B0.get_const_ptrs(),
                D.get_const_ptrs(), D0.get_const_ptrs(), B[1].dev_ndptr_const(),
                B[1].dev_ndptr_const(), m_a, m_ks_grid.get_grid_ptrs());
  CudaSafeCall(cudaDeviceSynchronize());
  kernel_launch(Bth_kernel, B.get_ptrs(), B0.get_const_ptrs(),
                D.get_const_ptrs(), D0.get_const_ptrs(),
                m_tmp_prev_field.dev_ndptr_const(), B[1].dev_ndptr_const(),
                m_a, m_ks_grid.get_grid_ptrs());
  CudaSafeCall(cudaDeviceSynchronize());
  CudaCheckError();

  select_dev(m_tmp_th_field) = B[1] * 0.5f + m_tmp_prev_field * 0.5f;
}

template <typename Conf>
void
field_solver_gr_ks_cu<Conf>::update_Bph(vector_field<Conf> &B,
                                        const vector_field<Conf> &B0,
                                        const vector_field<Conf> &D,
                                        const vector_field<Conf> &D0,
                                        value_t dt) {
  m_tmp_prev_field.copy_from(B[2]);

  // Use a predictor-corrector step to update Bph too
  auto Bph_kernel = [dt] __device__(auto B, auto B0, auto D, auto D0, auto B2_0,
                                    auto B2_1, auto a, auto grid_ptrs) {
    using namespace Metric_KS;
    auto &grid = dev_grid<Conf::dim>();
    auto ext = grid.extent();
    auto extl = grid.extent_less();
    for (auto idx : grid_stride_range(Conf::begin(ext), Conf::end(ext))) {
      auto pos = get_pos(idx, ext);
      if (grid.is_in_bound(pos)) {
        value_t r =
            grid_ks_t<Conf>::radius(grid.template pos<0>(pos[0], false));
        value_t r_sp =
            grid_ks_t<Conf>::radius(grid.template pos<0>(pos[0] + 1, true));
        value_t r_sm =
            grid_ks_t<Conf>::radius(grid.template pos<0>(pos[0], true));
        // value_t dr = r_sp - r_sm;

        value_t th = grid.template pos<1>(pos[1], false);
        value_t th_sp = grid.template pos<1>(pos[1] + 1, true);
        value_t th_sm = grid.template pos<1>(pos[1], true);
        // value_t dth = th_sp - th_sm;
        if (th_sm < TINY) th_sm = 0.01f * grid.delta[1];

        value_t sth = math::sin(th);
        value_t cth = math::cos(th);
        value_t prefactor = dt / grid_ptrs.Ab[2][idx];

        auto Er1 = grid_ptrs.ag11dr_e[idx.inc_y()] * D[0][idx.inc_y()] +
                   grid_ptrs.ag13dr_e[idx.inc_y()] * 0.5f *
                       (D[2][idx.inc_y()] + D[2][idx.inc_y().inc_x()]);

        auto Er0 =
            grid_ptrs.ag11dr_e[idx] * D[0][idx] +
            grid_ptrs.ag13dr_e[idx] * 0.5f * (D[2][idx] + D[2][idx.inc_x()]);

        auto Eth1 =
            grid_ptrs.ag22dth_e[idx.inc_x()] * D[1][idx.inc_x()] -
            grid_ptrs.gbetadth_e[idx.inc_x()] * 0.25f *
                (B2_0[idx.inc_x()] + B2_0[idx] + B2_1[idx.inc_x()] + B2_1[idx]);

        auto Eth0 =
            grid_ptrs.ag22dth_e[idx] * D[1][idx] -
            grid_ptrs.gbetadth_e[idx] * 0.25f *
                (B2_0[idx] + B2_0[idx.dec_x()] + B2_1[idx] + B2_1[idx.dec_x()]);

        B[2][idx] = B2_0[idx] - prefactor * ((Er0 - Er1) + (Eth1 - Eth0));
      }
    }
  };
  kernel_launch(Bph_kernel, B.get_ptrs(), B0.get_const_ptrs(),
                D.get_const_ptrs(), D0.get_const_ptrs(), B[2].dev_ndptr_const(),
                B[2].dev_ndptr_const(), m_a, m_ks_grid.get_grid_ptrs());
  CudaSafeCall(cudaDeviceSynchronize());
  kernel_launch(Bph_kernel, B.get_ptrs(), B0.get_const_ptrs(),
                D.get_const_ptrs(), D0.get_const_ptrs(),
                m_tmp_prev_field.dev_ndptr_const(), B[2].dev_ndptr_const(), m_a,
                m_ks_grid.get_grid_ptrs());
  CudaSafeCall(cudaDeviceSynchronize());
  CudaCheckError();
}

template <typename Conf>
void
field_solver_gr_ks_cu<Conf>::update_Br(vector_field<Conf> &B,
                                       const vector_field<Conf> &B0,
                                       const vector_field<Conf> &D,
                                       const vector_field<Conf> &D0,
                                       value_t dt) {
  kernel_launch(
      [dt] __device__(auto B, auto B0, auto D, auto D0, auto tmp_field, auto a,
                      auto grid_ptrs) {
        using namespace Metric_KS;
        auto &grid = dev_grid<Conf::dim>();
        auto ext = grid.extent();
        for (auto idx : grid_stride_range(Conf::begin(ext), Conf::end(ext))) {
          auto pos = get_pos(idx, ext);
          if (grid.is_in_bound(pos)) {
            value_t r =
                grid_ks_t<Conf>::radius(grid.template pos<0>(pos[0], true));
            value_t r_sp =
                grid_ks_t<Conf>::radius(grid.template pos<0>(pos[0], false));
            value_t r_sm = grid_ks_t<Conf>::radius(
                grid.template pos<0>(pos[0] - 1, false));

            value_t th = grid.template pos<1>(pos[1], false);
            value_t th_sp = grid.template pos<1>(pos[1] + 1, true);
            value_t th_sm = grid.template pos<1>(pos[1], true);
            if (th_sm < TINY) th_sm = 0.01f * grid.delta[1];

            value_t prefactor = dt / grid_ptrs.Ab[0][idx];

            value_t sth = math::sin(th_sp);
            value_t cth = math::cos(th_sp);
            value_t Eph1 =
                ag_33(a, r, sth, cth) * D[2][idx.inc_y()] +
                ag_13(a, r, sth, cth) * 0.5f *
                    (D[0][idx.inc_y()] + D[0][idx.inc_y().dec_x()]) +
                sq_gamma_beta(a, r, sth, cth) * 0.5f *
                    (tmp_field[idx.inc_y()] + tmp_field[idx.inc_y().dec_x()]);

            sth = math::sin(th_sm);
            cth = math::cos(th_sm);
            value_t Eph0 =
                ag_33(a, r, sth, cth) * D[2][idx] +
                ag_13(a, r, sth, cth) * 0.5f * (D[0][idx] + D[0][idx.dec_x()]) +
                sq_gamma_beta(a, r, sth, cth) * 0.5f *
                    (tmp_field[idx] + tmp_field[idx.dec_x()]);

            // if (pos[0] == 6 && pos[1] == 200) {
            //   printf(
            //       "Eph1 is %f, Eph0 is %f, dEphi is %f, tmpf is %f, "
            //       "tmpf+ is %f\n",
            //       Eph1, Eph0, Eph1 - Eph0, tmp_field[idx],
            //       tmp_field[idx.inc_y()]);
            //   printf("ag33 is %f, ag13 is %f, sqgb is %f\n",
            //          ag_33(a, r, sth, cth), ag_13(a, r, sth, cth),
            //          sq_gamma_beta(a, r, sth, cth));
            // }

            B[0][idx] += -prefactor * (Eph1 - Eph0);
          }
        }
      },
      B.get_ptrs(), B0.get_const_ptrs(), D.get_const_ptrs(),
      D0.get_const_ptrs(), m_tmp_th_field.dev_ndptr_const(), m_a,
      m_ks_grid.get_grid_ptrs());
  CudaSafeCall(cudaDeviceSynchronize());
  CudaCheckError();
}

template <typename Conf>
void
field_solver_gr_ks_cu<Conf>::update_Dth(vector_field<Conf> &D,
                                        const vector_field<Conf> &D0,
                                        const vector_field<Conf> &B,
                                        const vector_field<Conf> &B0,
                                        const vector_field<Conf> &J,
                                        value_t dt) {
  m_tmp_prev_field.copy_from(D[1]);

  // Predictor-corrector approach to update Dth
  auto Dth_kernel = [dt] __device__(auto D, auto D0, auto B, auto B0, auto J,
                                    auto D1_0, auto D1_1, auto a,
                                    auto grid_ptrs) {
    using namespace Metric_KS;

    auto &grid = dev_grid<Conf::dim>();
    auto ext = grid.extent();
    auto extl = grid.extent_less();
    for (auto idx : grid_stride_range(Conf::begin(ext), Conf::end(ext))) {
      auto pos = get_pos(idx, ext);
      if (grid.is_in_bound(pos)) {
        value_t r = grid_ks_t<Conf>::radius(grid.template pos<0>(pos[0], true));
        value_t r_sp =
            grid_ks_t<Conf>::radius(grid.template pos<0>(pos[0], false));
        value_t r_sm =
            grid_ks_t<Conf>::radius(grid.template pos<0>(pos[0] - 1, false));
        value_t dr = r_sp - r_sm;

        value_t th = grid.template pos<1>(pos[1], false);

        value_t sth = math::sin(th);
        value_t cth = math::cos(th);
        value_t prefactor = dt / grid_ptrs.Ad[1][idx];

        auto Hph1 =
            ag_33(a, r_sp, sth, cth) * B[2][idx] +
            ag_13(a, r_sp, sth, cth) * 0.5f * (B[0][idx.inc_x()] + B[0][idx]) -
            sq_gamma_beta(a, r_sp, sth, cth) * 0.25f *
                (D1_0[idx.inc_x()] + D1_0[idx] + D1_1[idx.inc_x()] + D1_1[idx]);

        auto Hph0 =
            ag_33(a, r_sm, sth, cth) * B[2][idx.dec_x()] +
            ag_13(a, r_sm, sth, cth) * 0.5f * (B[0][idx] + B[0][idx.dec_x()]) -
            sq_gamma_beta(a, r_sm, sth, cth) * 0.25f *
                (D1_0[idx] + D1_0[idx.dec_x()] + D1_1[idx] + D1_1[idx.dec_x()]);

        // TODO: Fix boundary node problenm!
        if (pos[0] == grid.guard[0]) {
          Hph0 = Hph1;
        }

        D[1][idx] = D1_0[idx] + prefactor * (Hph0 - Hph1) - dt * J[1][idx];
      }
    }
  };
  kernel_launch(Dth_kernel, D.get_ptrs(), D0.get_const_ptrs(),
                B.get_const_ptrs(), B0.get_const_ptrs(), J.get_const_ptrs(),
                D[1].dev_ndptr_const(), D[1].dev_ndptr_const(), m_a,
                m_ks_grid.get_grid_ptrs());
  CudaSafeCall(cudaDeviceSynchronize());
  kernel_launch(Dth_kernel, D.get_ptrs(), D0.get_const_ptrs(),
                B.get_const_ptrs(), B0.get_const_ptrs(), J.get_const_ptrs(),
                m_tmp_prev_field.dev_ndptr_const(), D[1].dev_ndptr_const(), m_a,
                m_ks_grid.get_grid_ptrs());
  CudaSafeCall(cudaDeviceSynchronize());
  CudaCheckError();

  select_dev(m_tmp_th_field) = D[1] * 0.5f + m_tmp_prev_field * 0.5f;
}

template <typename Conf>
void
field_solver_gr_ks_cu<Conf>::update_Dph(vector_field<Conf> &D,
                                        const vector_field<Conf> &D0,
                                        const vector_field<Conf> &B,
                                        const vector_field<Conf> &B0,
                                        const vector_field<Conf> &J,
                                        value_t dt) {
  m_tmp_prev_field.copy_from(D[2]);

  // First assemble the right hand side and the diagonals of the tri-diagonal
  // equation
  auto Dph_kernel = [dt] __device__(auto D, auto B, auto J, auto D2_0,
                                    auto D2_1, auto a, auto grid_ptrs) {
    using namespace Metric_KS;

    auto &grid = dev_grid<Conf::dim>();
    auto ext = grid.extent();
    auto extl = grid.extent_less();
    for (auto idx : grid_stride_range(Conf::begin(ext), Conf::end(ext))) {
      auto pos = get_pos(idx, ext);
      if (grid.is_in_bound(pos)) {
        value_t r = grid_ks_t<Conf>::radius(grid.template pos<0>(pos[0], true));
        value_t r_sp =
            grid_ks_t<Conf>::radius(grid.template pos<0>(pos[0], false));
        value_t r_sm =
            grid_ks_t<Conf>::radius(grid.template pos<0>(pos[0] - 1, false));
        value_t dr = r_sp - r_sm;

        value_t th = grid.template pos<1>(pos[1], true);
        value_t th_sp = grid.template pos<1>(pos[1], false);
        value_t th_sm = grid.template pos<1>(pos[1] - 1, false);
        if (th < TINY) th = 0.01f * grid.delta[1];

        value_t sth = math::sin(th);
        value_t cth = math::cos(th);
        value_t prefactor = dt / grid_ptrs.Ad[2][idx];

        auto Hr0 = grid_ptrs.ag11dr_h[idx.dec_y()] * B[0][idx.dec_y()] +
                   grid_ptrs.ag13dr_h[idx.dec_y()] * 0.5f *
                       (B[2][idx.dec_y()] + B[2][idx.dec_y().dec_x()]);

        auto Hr1 =
            grid_ptrs.ag11dr_h[idx] * B[0][idx] +
            grid_ptrs.ag13dr_h[idx] * 0.5f * (B[2][idx] + B[2][idx.dec_x()]);

        auto Hth0 =
            grid_ptrs.ag22dth_h[idx.dec_x()] * B[1][idx.dec_x()] +
            grid_ptrs.gbetadth_h[idx.dec_x()] * 0.25f *
                (D2_0[idx] + D2_0[idx.dec_x()] + D2_1[idx] + D2_1[idx.dec_x()]);

        auto Hth1 =
            grid_ptrs.ag22dth_h[idx] * B[1][idx] +
            grid_ptrs.gbetadth_h[idx] * 0.25f *
                (D2_0[idx.inc_x()] + D2_0[idx] + D2_1[idx.inc_x()] + D2_1[idx]);

        // TODO: Fix boundary node problenm!
        if (pos[0] == grid.guard[0]) {
          Hth0 = Hth1;
        }

        D[2][idx] = D2_0[idx] + prefactor * ((Hr0 - Hr1) + (Hth1 - Hth0)) - dt * J[2][idx];

        if (pos[0] == 10 && pos[1] == 250) {
          printf("Hr0 is %f, Hr1 is %f, Hth0 is %f, Hth1 is %f, dDphi is %f\n",
                 Hr0, Hr1, Hth0, Hth1,
                 prefactor * ((Hr0 - Hr1) + (Hth1 - Hth0)));
        }
      }
    }
  };
  kernel_launch(Dph_kernel, D.get_ptrs(), B.get_const_ptrs(),
                J.get_const_ptrs(), D[2].dev_ndptr_const(),
                D[2].dev_ndptr_const(), m_a, m_ks_grid.get_grid_ptrs());
  CudaSafeCall(cudaDeviceSynchronize());
  kernel_launch(Dph_kernel, D.get_ptrs(), B.get_const_ptrs(),
                J.get_const_ptrs(), m_tmp_prev_field.dev_ndptr_const(),
                D[2].dev_ndptr_const(), m_a,
                m_ks_grid.get_grid_ptrs());
  CudaSafeCall(cudaDeviceSynchronize());
  CudaCheckError();
}

template <typename Conf>
void
field_solver_gr_ks_cu<Conf>::update_Dr(vector_field<Conf> &D,
                                       const vector_field<Conf> &D0,
                                       const vector_field<Conf> &B,
                                       const vector_field<Conf> &B0,
                                       const vector_field<Conf> &J,
                                       value_t dt) {
  kernel_launch(
      [dt] __device__(auto D, auto B, auto J, auto tmp_field, auto a,
                      auto grid_ptrs) {
        using namespace Metric_KS;

        auto &grid = dev_grid<Conf::dim>();
        auto ext = grid.extent();
        for (auto idx : grid_stride_range(Conf::begin(ext), Conf::end(ext))) {
          auto pos = get_pos(idx, ext);
          if (grid.is_in_bound(pos)) {
            value_t r =
                grid_ks_t<Conf>::radius(grid.template pos<0>(pos[0], false));
            value_t r_sp =
                grid_ks_t<Conf>::radius(grid.template pos<0>(pos[0] + 1, true));
            value_t r_sm =
                grid_ks_t<Conf>::radius(grid.template pos<0>(pos[0], true));

            value_t th = grid.template pos<1>(pos[1], true);
            value_t th_sp = grid.template pos<1>(pos[1], false);
            value_t th_sm = grid.template pos<1>(pos[1] - 1, false);
            // bool is_axis = false;
            if (th < TINY) {
              th = 1.0e-5;
              // is_axis = true;
            }

            value_t prefactor = dt / grid_ptrs.Ad[0][idx];

            value_t sth = math::sin(th_sp);
            value_t cth = math::cos(th_sp);
            auto Hph1 =
                ag_33(a, r, sth, cth) * B[2][idx] +
                ag_13(a, r, sth, cth) * 0.5f * (B[0][idx] + B[0][idx.inc_x()]) -
                sq_gamma_beta(a, r, sth, cth) * 0.5f *
                    (tmp_field[idx] + tmp_field[idx.inc_x()]);

            sth = math::sin(th_sm);
            cth = math::cos(th_sm);
            auto Hph0 =
                ag_33(a, r, sth, cth) * B[2][idx.dec_y()] +
                ag_13(a, r, sth, cth) * 0.5f *
                    (B[0][idx.dec_y()] + B[0][idx.dec_y().inc_x()]) -
                sq_gamma_beta(a, r, sth, cth) * 0.5f *
                    (tmp_field[idx.dec_y()] + tmp_field[idx.dec_y().inc_x()]);

            // if (is_axis) {
            //   Hph0 = -Hph1;
            // }

            D[0][idx] += prefactor * (Hph1 - Hph0) - dt * J[0][idx];

            if (D[0][idx] != D[0][idx]) {
              printf(
                  "NaN detected in Dr update! B2 is %f, B0 is %f, tmp_field is "
                  "%f\n",
                  B[2][idx.dec_y()], B[0][idx.dec_y()], tmp_field[idx.dec_y()]);
              asm("trap;");
            }
          }
        }
      },
      D.get_ptrs(), B.get_const_ptrs(), J.get_const_ptrs(),
      m_tmp_th_field.dev_ndptr_const(), m_a, m_ks_grid.get_grid_ptrs());
  CudaSafeCall(cudaDeviceSynchronize());
  CudaCheckError();
}

template <typename Conf>
void
field_solver_gr_ks_cu<Conf>::update(double dt, uint32_t step) {
  Logger::print_info("In GR KS solver! a is {}", m_a);

  if (this->m_update_b) {
    update_Bph(*(this->B), *(this->B0), *(this->E), *(this->E0), dt);
    update_Bth(*(this->B), *(this->B0), *(this->E), *(this->E0), dt);
    update_Br(*(this->B), *(this->B0), *(this->E), *(this->E0), dt);

    axis_boundary_b(*(this->B), m_ks_grid);
    // Communicate the new B values to guard cells
    if (this->m_comm != nullptr) this->m_comm->send_guard_cells(*(this->B));
  }

  if (this->m_update_e) {
    update_Dph(*(this->E), *(this->E0), *(this->B), *(this->B0), *(this->J),
               dt);
    update_Dth(*(this->E), *(this->E0), *(this->B), *(this->B0), *(this->J),
               dt);
    update_Dr(*(this->E), *(this->E0), *(this->B), *(this->B0), *(this->J), dt);

    axis_boundary_e(*(this->E), m_ks_grid);
    // Communicate the new E values to guard cells
    if (this->m_comm != nullptr) this->m_comm->send_guard_cells(*(this->E));
  }

  if (this->m_comm == nullptr || this->m_comm->domain_info().is_boundary[0]) {
    // horizon_boundary(*(this->E), *(this->B), *(this->E0), *(this->B0),
    //                  m_ks_grid, m_damping_length, m_damping_coef);
    // inner_boundary(*(this->E), *(this->B), m_ks_grid, m_damping_length);
  }

  compute_divs(*(this->divE), *(this->divB), *(this->E), *(this->B), m_ks_grid);

  this->Etotal->copy_from(*(this->E));
  // this->Etotal->add_by(*(this->E0));

  this->Btotal->copy_from(*(this->B));
  // this->Btotal->add_by(*(this->B0));

  if (step % this->m_data_interval == 0) {
    compute_flux(*flux, *(this->Btotal), m_ks_grid);
  }

  CudaSafeCall(cudaDeviceSynchronize());
}

template class field_solver_gr_ks_cu<Config<2>>;

}  // namespace Aperture
