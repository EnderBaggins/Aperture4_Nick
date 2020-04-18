#include "core/constant_mem_func.h"
#include "data/field_helpers.h"
#include "framework/config.h"
#include "ptc_updater.h"
#include "helpers/ptc_update_helper.hpp"
#include "utils/double_buffer.h"
#include "utils/interpolation.hpp"
#include "utils/kernel_helper.hpp"
#include "utils/range.hpp"
#include "utils/util_functions.h"

namespace Aperture {

template <typename Conf>
void
ptc_updater_cu<Conf>::init() {
  this->init_charge_mass();
  init_dev_charge_mass(this->m_charges, this->m_masses);

  this->Etmp = vector_field<Conf>(this->m_grid, MemType::device_only);
  this->Btmp = vector_field<Conf>(this->m_grid, MemType::device_only);

  m_rho_ptrs.set_memtype(MemType::host_device);
  m_rho_ptrs.resize(this->m_num_species);
  for (int i = 0; i < this->m_num_species; i++) {
    m_rho_ptrs[i] = this->Rho[i]->get_ptr();
  }
  m_rho_ptrs.copy_to_device();
}

template <typename Conf>
void
ptc_updater_cu<Conf>::register_dependencies() {
  size_t max_ptc_num = 1000000;
  this->m_env.params().get_value("max_ptc_num", max_ptc_num);
  // Prefer device_only, but can take other possibilities if data is already
  // there
  this->ptc = this->m_env.template register_data<particle_data_t>(
      "particles", max_ptc_num, MemType::device_only);

  this->E = this->m_env.template register_data<vector_field<Conf>>(
      "E", this->m_grid, field_type::edge_centered, MemType::host_device);
  this->B = this->m_env.template register_data<vector_field<Conf>>(
      "B", this->m_grid, field_type::face_centered, MemType::host_device);
  this->J = this->m_env.template register_data<vector_field<Conf>>(
      "J", this->m_grid, field_type::edge_centered, MemType::host_device);

  this->m_env.params().get_value("num_species", this->m_num_species);
  this->Rho.resize(this->m_num_species);
  for (int i = 0; i < this->m_num_species; i++) {
    this->Rho[i] = this->m_env.template register_data<scalar_field<Conf>>(
        std::string("Rho_") + ptc_type_name(i), this->m_grid,
        field_type::vert_centered, MemType::host_device);
  }
}

// template <typename Conf>
// void
// ptc_updater_cu<Conf>::update(double dt, uint32_t step) {
//   if (this->m_pusher == Pusher::boris) {
//     push<boris_pusher>(dt, true);
//   } else if (this->m_pusher == Pusher::vay) {
//     push<vay_pusher>(dt, true);
//   } else if (this->m_pusher == Pusher::higuera) {
//     push<higuera_pusher>(dt, true);
//   }

// }

template <typename Conf>
void
ptc_updater_cu<Conf>::push_default(double dt, bool resample_field) {
  // dispatch according to enum
  if (this->m_pusher == Pusher::boris) {
    push<boris_pusher>(dt, true);
  } else if (this->m_pusher == Pusher::vay) {
    push<vay_pusher>(dt, true);
  } else if (this->m_pusher == Pusher::higuera) {
    push<higuera_pusher>(dt, true);
  }
}

template <typename Conf>
template <typename P>
void
ptc_updater_cu<Conf>::push(double dt, bool resample_field) {
  // First interpolate E and B fields to vertices and store them in Etmp
  // and Btmp
  auto dbE = make_double_buffer(*(this->E), this->Etmp);
  auto dbB = make_double_buffer(*(this->B), this->Btmp);
  if (resample_field) {
    resample_dev(dbE.main()[0], dbE.alt()[0], this->m_grid.guards(),
                 this->E->stagger(0), this->Etmp.stagger(0));
    resample_dev(dbE.main()[1], dbE.alt()[1], this->m_grid.guards(),
                 this->E->stagger(1), this->Etmp.stagger(1));
    resample_dev(dbE.main()[2], dbE.alt()[2], this->m_grid.guards(),
                 this->E->stagger(2), this->Etmp.stagger(2));
    resample_dev(dbB.main()[0], dbB.alt()[0], this->m_grid.guards(),
                 this->B->stagger(0), this->Btmp.stagger(0));
    resample_dev(dbB.main()[1], dbB.alt()[1], this->m_grid.guards(),
                 this->B->stagger(1), this->Btmp.stagger(1));
    resample_dev(dbB.main()[2], dbB.alt()[2], this->m_grid.guards(),
                 this->B->stagger(2), this->Btmp.stagger(2));
    dbE.swap();
    dbB.swap();
  }

  auto num = this->ptc->number();
  auto ext = this->m_grid.extent();
  P pusher;

  auto pusher_kernel = [dt, num, ext] __device__(auto ptrs, auto E, auto B,
                                                 auto pusher) {
    for (auto n : grid_stride_range(0ul, num)) {
      uint32_t cell = ptrs.cell[n];
      if (cell == empty_cell) continue;
      auto idx = E[0].idx_at(cell, ext);
      // auto pos = idx.get_pos();

      auto interp = interpolator<bspline<1>, Conf::dim>{};
      auto flag = ptrs.flag[n];
      int sp = get_ptc_type(flag);

      Scalar qdt_over_2m = dt * 0.5f * dev_charges[sp] / dev_masses[sp];

      auto x = vec_t<Pos_t, 3>(ptrs.x1[n], ptrs.x2[n], ptrs.x3[n]);
      //  Grab E & M fields at the particle position
      Scalar E1 = interp(E[0], x, idx);
      Scalar E2 = interp(E[1], x, idx);
      Scalar E3 = interp(E[2], x, idx);
      Scalar B1 = interp(B[0], x, idx);
      Scalar B2 = interp(B[1], x, idx);
      Scalar B3 = interp(B[2], x, idx);

      //  Push particles
      Scalar p1 = ptrs.p1[n], p2 = ptrs.p2[n], p3 = ptrs.p3[n],
             gamma = ptrs.E[n];
      if (p1 != p1 || p2 != p2 || p3 != p3) {
        printf(
            "NaN detected in push! p1 is %f, p2 is %f, p3 is %f, gamma "
            "is %f\n",
            p1, p2, p3, gamma);
        asm("trap;");
      }

      if (!check_flag(flag, PtcFlag::ignore_EM)) {
        pusher(p1, p2, p3, gamma, E1, E2, E3, B1, B2, B3, qdt_over_2m,
               (Scalar)dt);
      }

      // if (dev_params.rad_cooling_on && sp != (int)ParticleType::ion) {
      //   sync_kill_perp(p1, p2, p3, gamma, B1, B2, B3, E1, E2, E3,
      //                  q_over_m);
      // }
      ptrs.p1[n] = p1;
      ptrs.p2[n] = p2;
      ptrs.p3[n] = p3;
      ptrs.E[n] = gamma;
    }
  };

  if (num > 0) {
    kernel_launch(pusher_kernel, this->ptc->dev_ptrs(), dbE.main().get_ptrs(),
                  dbB.main().get_ptrs(), pusher);
  }
}

template <typename Conf>
void
ptc_updater_cu<Conf>::move_deposit_1d(double dt, uint32_t step) {
  auto num = this->ptc->number();
  if (num > 0) {
    auto ext = this->m_grid.extent();

    kernel_launch(
        [ext, num, dt, step] __device__(auto ptc, auto J, auto Rho,
                                        auto data_interval) {
          using spline_t = typename base_class::spline_t;
          for (auto n : grid_stride_range(0, num)) {
            uint32_t cell = ptc.cell[n];
            if (cell == empty_cell) continue;

            auto idx = J[0].idx_at(cell, ext);
            auto pos = idx.get_pos();

            // step 1: Move particles
            auto x1 = ptc.x1[n], x2 = ptc.x2[n], x3 = ptc.x3[n];
            Scalar v1 = ptc.p1[n], v2 = ptc.p2[n], v3 = ptc.p3[n],
                   gamma = ptc.E[n];

            v1 /= gamma;
            v2 /= gamma;
            v3 /= gamma;

            auto new_x1 = x1 + (v1 * dt) * dev_grid_1d.inv_delta[0];
            int dc1 = std::floor(new_x1);
            pos[0] += dc1;
            ptc.x1[n] = new_x1 - (Pos_t)dc1;
            ptc.x2[n] = x2 + v2 * dt;
            ptc.x3[n] = x3 + v3 * dt;

            ptc.cell[n] = J[0].get_idx(pos, ext).linear;

            // step 2: Deposit current
            auto flag = ptc.flag[n];
            auto sp = get_ptc_type(flag);
            auto interp = spline_t{};
            if (check_flag(flag, PtcFlag::ignore_current)) continue;
            auto weight = dev_charges[sp] * ptc.weight[n];

            int i_0 = (dc1 == -1 ? -spline_t::radius : 1 - spline_t::radius);
            int i_1 = (dc1 == 1 ? spline_t::radius + 1 : spline_t::radius);
            Scalar djx = 0.0f;
            for (int i = i_0; i <= i_1; i++) {
              Scalar sx0 = interp(-x1 + i);
              Scalar sx1 = interp(-new_x1 + i);

              // j1 is movement in x1
              int offset = i + pos[0] - dc1;
              djx += sx1 - sx0;
              atomicAdd(&J[0][offset], -weight * djx);
              // Logger::print_debug("J0 is {}", (*J)[0][offset]);

              // j2 is simply v2 times rho at center
              Scalar val1 = 0.5f * (sx0 + sx1);
              atomicAdd(&J[1][offset], weight * v2 * val1);

              // j3 is simply v3 times rho at center
              atomicAdd(&J[2][offset], weight * v3 * val1);

              // rho is deposited at the final position
              if ((step + 1) % data_interval == 0) {
                atomicAdd(&Rho[sp][offset], weight * sx1);
              }
            }
          }
        },
        this->ptc->dev_ptrs(), this->J->get_ptrs(), m_rho_ptrs.dev_ptr(),
        this->m_data_interval);
  }
}

template <typename Conf>
void
ptc_updater_cu<Conf>::move_deposit_2d(double dt, uint32_t step) {
  auto num = this->ptc->number();
  if (num > 0) {
    auto ext = this->m_grid.extent();

    kernel_launch(
        [ext, num, dt, step] __device__(auto ptc, auto J, auto Rho,
                                        auto data_interval) {
          using spline_t = typename base_class::spline_t;
          for (auto n : grid_stride_range(0, num)) {
            uint32_t cell = ptc.cell[n];
            if (cell == empty_cell) continue;

            auto idx = J[0].idx_at(cell, ext);
            auto pos = idx.get_pos();

            // step 1: Move particles
            auto x1 = ptc.x1[n], x2 = ptc.x2[n], x3 = ptc.x3[n];
            Scalar v1 = ptc.p1[n], v2 = ptc.p2[n], v3 = ptc.p3[n],
                   gamma = ptc.E[n];

            v1 /= gamma;
            v2 /= gamma;
            v3 /= gamma;

            auto new_x1 = x1 + (v1 * dt) * dev_grid_2d.inv_delta[0];
            int dc1 = std::floor(new_x1);
            pos[0] += dc1;
            ptc.x1[n] = new_x1 - (Pos_t)dc1;

            auto new_x2 = x2 + (v2 * dt) * dev_grid_2d.inv_delta[1];
            int dc2 = std::floor(new_x2);
            pos[1] += dc2;
            ptc.x2[n] = new_x2 - (Pos_t)dc2;

            ptc.x3[n] = x3 + v3 * dt;

            ptc.cell[n] = J[0].get_idx(pos, ext).linear;

            // step 2: Deposit current
            auto flag = ptc.flag[n];
            auto sp = get_ptc_type(flag);
            auto interp = spline_t{};
            if (check_flag(flag, PtcFlag::ignore_current)) continue;
            auto weight = dev_charges[sp] * ptc.weight[n];

            int j_0 = (dc2 == -1 ? -spline_t::radius : 1 - spline_t::radius);
            int j_1 = (dc2 == 1 ? spline_t::radius + 1 : spline_t::radius);
            int i_0 = (dc1 == -1 ? -spline_t::radius : 1 - spline_t::radius);
            int i_1 = (dc1 == 1 ? spline_t::radius + 1 : spline_t::radius);

            Scalar djy[2 * spline_t::radius + 1] = {};
            for (int j = j_0; j <= j_1; j++) {
              Scalar sy0 = interp(-x2 + j);
              Scalar sy1 = interp(-new_x2 + j);

              Scalar djx = 0.0f;
              for (int i = i_0; i <= i_1; i++) {
                Scalar sx0 = interp(-x1 + i);
                Scalar sx1 = interp(-new_x1 + i);

                // j1 is movement in x1
                auto offset = idx.inc_x(i).inc_y(j);
                djx += movement2d(sy0, sy1, sx0, sx1);
                atomicAdd(&J[0][offset], -weight * djx);
                // Logger::print_debug("J0 is {}", (*J)[0][offset]);

                // j2 is movement in x2
                djy[i - i_0] += movement2d(sx0, sx1, sy0, sy1);
                atomicAdd(&J[1][offset], -weight * djy[i - i_0]);

                // j3 is simply v3 times rho at center
                atomicAdd(&J[2][offset], weight * v3 *
                          center2d(sx0, sx1, sy0, sy1));

                // rho is deposited at the final position
                if ((step + 1) % data_interval == 0) {
                  atomicAdd(&Rho[sp][offset], weight * sx1 * sy1);
                }
              }
            }
          }
        },
        this->ptc->dev_ptrs(), this->J->get_ptrs(), m_rho_ptrs.dev_ptr(),
        this->m_data_interval);
  }
}

template <typename Conf>
void
ptc_updater_cu<Conf>::move_deposit_3d(double dt, uint32_t step) {
  auto num = this->ptc->number();
  if (num > 0) {
    auto ext = this->m_grid.extent();

    kernel_launch(
        [ext, num, dt, step] __device__(auto ptc, auto J, auto Rho,
                                        auto data_interval) {
          using spline_t = typename base_class::spline_t;
          for (auto n : grid_stride_range(0, num)) {
            uint32_t cell = ptc.cell[n];
            if (cell == empty_cell) continue;

            auto idx = J[0].idx_at(cell, ext);
            auto pos = idx.get_pos();

            // step 1: Move particles
            auto x1 = ptc.x1[n], x2 = ptc.x2[n], x3 = ptc.x3[n];
            Scalar v1 = ptc.p1[n], v2 = ptc.p2[n], v3 = ptc.p3[n],
                   gamma = ptc.E[n];

            v1 /= gamma;
            v2 /= gamma;
            v3 /= gamma;

            auto new_x1 = x1 + (v1 * dt) * dev_grid_3d.inv_delta[0];
            int dc1 = std::floor(new_x1);
            pos[0] += dc1;
            ptc.x1[n] = new_x1 - (Pos_t)dc1;

            auto new_x2 = x2 + (v2 * dt) * dev_grid_3d.inv_delta[1];
            int dc2 = std::floor(new_x2);
            pos[1] += dc2;
            ptc.x2[n] = new_x2 - (Pos_t)dc2;

            auto new_x3 = x3 + (v3 * dt) * dev_grid_3d.inv_delta[2];
            int dc3 = std::floor(new_x3);
            pos[2] += dc3;
            ptc.x3[n] = new_x3 - (Pos_t)dc3;

            ptc.cell[n] = J[0].get_idx(pos, ext).linear;

            // step 2: Deposit current
            auto flag = ptc.flag[n];
            auto sp = get_ptc_type(flag);
            auto interp = spline_t{};
            if (check_flag(flag, PtcFlag::ignore_current)) continue;
            auto weight = dev_charges[sp] * ptc.weight[n];

            int k_0 = (dc3 == -1 ? -spline_t::radius : 1 - spline_t::radius);
            int k_1 = (dc3 == 1 ? spline_t::radius + 1 : spline_t::radius);
            int j_0 = (dc2 == -1 ? -spline_t::radius : 1 - spline_t::radius);
            int j_1 = (dc2 == 1 ? spline_t::radius + 1 : spline_t::radius);
            int i_0 = (dc1 == -1 ? -spline_t::radius : 1 - spline_t::radius);
            int i_1 = (dc1 == 1 ? spline_t::radius + 1 : spline_t::radius);

            Scalar djz[2 * spline_t::radius + 1][2 * spline_t::radius + 1] = {};
            for (int k = k_0; k <= k_1; k++) {
              Scalar sz0 = interp(-x3 + k);
              Scalar sz1 = interp(-new_x3 + k);

              Scalar djy[2 * spline_t::radius + 1] = {};
              for (int j = j_0; j <= j_1; j++) {
                Scalar sy0 = interp(-x2 + j);
                Scalar sy1 = interp(-new_x2 + j);

                Scalar djx = 0.0f;
                for (int i = i_0; i <= i_1; i++) {
                  Scalar sx0 = interp(-x1 + i);
                  Scalar sx1 = interp(-new_x1 + i);

                  // j1 is movement in x1
                  auto offset = idx.inc_x(i).inc_y(j).inc_z(k);
                  djx += movement3d(sy0, sy1, sz0, sz1, sx0, sx1);
                  atomicAdd(&J[0][offset], -weight * djx);
                  // Logger::print_debug("J0 is {}", (*J)[0][offset]);

                  // j2 is movement in x2
                  djy[i - i_0] += movement3d(sz0, sz1, sx0, sx1, sy0, sy1);
                  atomicAdd(&J[1][offset], -weight * djy[i - i_0]);

                  // j3 is movement in x3
                  djz[j - j_0][i - i_0] +=
                      movement3d(sx0, sx1, sy0, sy1, sz0, sz1);
                  atomicAdd(&J[2][offset], -weight * djz[j - j_0][i - i_0]);

                  // rho is deposited at the final position
                  if ((step + 1) % data_interval == 0) {
                    atomicAdd(&Rho[sp][offset], weight * sx1 * sy1 * sz1);
                  }
                }
              }
            }
          }
        },
        this->ptc->dev_ptrs(), this->J->get_ptrs(), m_rho_ptrs.dev_ptr(),
        this->m_data_interval);
  }
}

template class ptc_updater_cu<Config<1>>;
template class ptc_updater_cu<Config<2>>;
template class ptc_updater_cu<Config<3>>;

}  // namespace Aperture