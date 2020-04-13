#include "grid.h"
#include "core/constant_mem_func.h"
#include "core/domain_info.h"
#include "framework/config.h"
#include "framework/environment.hpp"
#include "framework/parse_params.hpp"
#include <exception>

namespace Aperture {

template <typename Conf>
grid_t<Conf>::grid_t(sim_environment& env,
                     const domain_info_t<Conf::dim>& domain_info)
    : system_t(env) {
  // Obtain grid parameters from the params store
  uint32_t vec_N[Conf::dim];
  get_from_store("N", vec_N, m_env.params());
  // auto vec_N = m_env.params().get<std::vector<int64_t>>("N");

  // uint32_t vec_N[Conf::dim] = {};
  get_from_store("guard", this->guard, m_env.params());
  get_from_store("size", this->sizes, m_env.params());
  get_from_store("lower", this->lower, m_env.params());

  // Initialize the grid parameters
  for (int i = 0; i < Conf::dim; i++) {
    this->delta[i] = this->sizes[i] / vec_N[i];
    this->inv_delta[i] = 1.0 / this->delta[i];
    // TODO: the z-order case is very weird. Is there a better way?
    if (Conf::is_zorder) {
      this->skirt[i] = 8;
      this->dims[i] = vec_N[i];
    } else {
      this->skirt[i] = this->guard[i];
      this->dims[i] = vec_N[i] + 2 * this->guard[i];
    }
    Logger::print_debug("Dim {} has size {}", i, this->dims[i]);
  }

  // Adjust the grid according to domain decomposition
  for (int d = 0; d < Conf::dim; d++) {
    this->dims[d] =
        this->reduced_dim(d) / domain_info.mpi_dims[d] + 2 * this->guard[d];
    this->sizes[d] /= domain_info.mpi_dims[d];
    this->lower[d] += domain_info.mpi_coord[d] * this->sizes[d];
    // TODO: In a non-uniform domain decomposition, the offset could
    // change, need a more robust way to count this
    this->offset[d] = domain_info.mpi_coord[d] * this->reduced_dim(d);
  }

  // Copy the grid parameters to gpu
#ifdef CUDA_ENABLED
  init_dev_grid<Conf::dim>(*this);
#endif
}

// template <typename Conf>
// void
// grid_t<Conf>::register_dependencies(sim_environment &env) {
//   // If we are initializing the communicator system, it should be done
//   // before initializing this one
//   depends_on("communicator");
// }

template class grid_t<Config<1>>;
template class grid_t<Config<2>>;
template class grid_t<Config<3>>;

}  // namespace Aperture
