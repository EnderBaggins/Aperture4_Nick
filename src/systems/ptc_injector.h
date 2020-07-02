#ifndef _PTC_INJECTOR_H_
#define _PTC_INJECTOR_H_

#include "core/enum_types.h"
#include "core/multi_array.hpp"
#include "data/fields.h"
#include "data/particle_data.h"
#include "framework/system.h"
#include "systems/grid.h"
#include <memory>

namespace Aperture {

class curand_states_t;

template <typename Conf>
class ptc_injector : public system_t {
 public:
  typedef typename Conf::value_t value_t;
  static std::string name() { return "ptc_injector"; }

  ptc_injector(sim_environment& env, const grid_t<Conf>& grid)
      : system_t(env), m_grid(grid) {}
  ptc_injector(sim_environment& env, const grid_t<Conf>& grid,
               const vec_t<value_t, Conf::dim>& lower,
               const extent_t<Conf::dim>& extent, value_t inj_rate,
               value_t inj_weight);
  virtual ~ptc_injector() {}

  virtual void init() override;
  virtual void update(double dt, uint32_t step) override;
  virtual void register_data_components() override;

 protected:
  const grid_t<Conf>& m_grid;

  particle_data_t* ptc;
  value_t m_inj_rate;
  value_t m_inj_weight;
  // vector_field<Conf>* B;

  // value_t m_target_sigma = 100.0;
};

template <typename Conf>
class ptc_injector_cu : public ptc_injector<Conf> {
 public:
  static std::string name() { return "ptc_injector"; }

  using ptc_injector<Conf>::ptc_injector;
  virtual ~ptc_injector_cu() {}

  virtual void init() override;
  virtual void update(double dt, uint32_t step) override;
  virtual void register_data_components() override;

 protected:
  curand_states_t* m_rand_states;
  multi_array<int, Conf::dim> m_num_per_cell;
  multi_array<int, Conf::dim> m_cum_num_per_cell;
  scalar_field<Conf>* m_sigma;
};

}  // namespace Aperture

#endif  // _PTC_INJECTOR_H_
