#include "radiative_transfer.h"
#include "framework/environment.h"
#include <exception>

namespace Aperture {

template <typename Conf>
radiative_transfer_cu<Conf>::radiative_transfer_cu(sim_environment& env,
                                             const grid_t<Conf>& grid,
                                             const domain_comm<Conf>* comm)
    : radiative_transfer<Conf>(env, grid, comm) {}

template <typename Conf>
void
radiative_transfer_cu<Conf>::init() {}

template <typename Conf>
void
radiative_transfer_cu<Conf>::register_dependencies() {
  this->m_env.get_data("particles", &(this->ptc));
  if (this->ptc == nullptr)
    throw std::runtime_error("Data component 'particles' is not found!");

  size_t max_ph_num = 10000;
  this->m_env.params().get_value("max_ph_num", max_ph_num);

  this->ph = this->m_env.template register_data<photon_data_t>("photons", max_ph_num,
                                          MemType::device_only);
  this->rho_ph = this->m_env.template register_data<scalar_field<Conf>>(
      "Rho_ph", this->m_grid, field_type::vert_centered, MemType::host_device);
}

template <typename Conf>
void
radiative_transfer_cu<Conf>::update(double dt, uint32_t step) {}

template <typename Conf>
void
radiative_transfer_cu<Conf>::move_photons(double dt) {
}

template <typename Conf>
void
radiative_transfer_cu<Conf>::emit_photons(double dt) {
}

template <typename Conf>
void
radiative_transfer_cu<Conf>::produce_pairs(double dt) {
}

}  // namespace Aperture
