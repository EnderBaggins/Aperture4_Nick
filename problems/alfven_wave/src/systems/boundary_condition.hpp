#ifndef __BOUNDARY_CONDITION_H_
#define __BOUNDARY_CONDITION_H_

#include "data/fields.h"
#include "framework/environment.h"
#include "framework/system.h"
#include "systems/grid_curv.h"
#include <memory>

namespace Aperture {

template <typename Conf>
class boundary_condition : public system_t {
 protected:
  const grid_curv_t<Conf>& m_grid;
  typename Conf::value_t m_rpert1 = 5.0, m_rpert2 = 10.0;
  typename Conf::value_t m_tp_start, m_tp_end, m_nT, m_dw0;

  vector_field<Conf> *E, *B, *E0, *B0;

 public:
  static std::string name() { return "boundary_condition"; }

  boundary_condition(sim_environment& env, const grid_curv_t<Conf>& grid) :
      system_t(env), m_grid(grid) {}

  void init() override;
  void update(double dt, uint32_t step) override;

  void register_dependencies() {}
};

}

#endif // __BOUNDARY_CONDITION_H_
