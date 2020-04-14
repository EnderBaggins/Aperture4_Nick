#ifndef __CONSANT_MEM_H_
#define __CONSANT_MEM_H_

#include <cstdint>
#include "core/enum_types.h"

namespace Aperture {

template <int Dim>
struct Grid;

struct sim_params;

#if defined(__CUDACC__)

extern __constant__ uint32_t morton2dLUT_dev[256];
extern __constant__ uint32_t morton3dLUT_dev[256];
// extern __constant__ sim_params dev_params;
extern __constant__ Grid<1> dev_grid_1d;
extern __constant__ Grid<2> dev_grid_2d;
extern __constant__ Grid<3> dev_grid_3d;
extern __constant__ float dev_charges[max_ptc_types];
extern __constant__ float dev_masses[max_ptc_types];

#endif

}  // namespace Aperture

#endif
