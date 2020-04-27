#ifndef _INITIAL_CONDITION_H_
#define _INITIAL_CONDITION_H_

#include "framework/environment.hpp"
#include "systems/grid_logsph.h"

namespace Aperture {

template <typename Conf>
void set_initial_condition(sim_environment& env,
                           const grid_logsph_t<Conf>& grid, int mult,
                           double weight, double Bp);

}

#endif  // _INITIAL_CONDITION_H_