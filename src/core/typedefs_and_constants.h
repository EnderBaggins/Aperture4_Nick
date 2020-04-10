#ifndef _TYPEDEFS_N_CONSTANTS_H_
#define _TYPEDEFS_N_CONSTANTS_H_

#include <limits>
#include <cstdint>
#include "enum_types.h"

namespace Aperture {

#ifndef USE_DOUBLE
typedef float Scalar;
typedef float Mom_t;
typedef float Pos_t;
#else
typedef double Scalar;
typedef double Mom_t;
typedef float Pos_t;
#endif

#ifdef CUDA_ENABLED
constexpr MemoryModel default_memory_model = MemoryModel::host_device;
#else
constexpr MemoryModel default_memory_model = MemoryModel::host_only;
#endif
constexpr uint32_t empty_cell = std::numeric_limits<uint32_t>::max();
constexpr float eps_float = 1.0e-8f;
constexpr double eps_double = 1.0e-12;

}  // namespace Aperture

#endif  // _TYPEDEFS_N_CONSTANTS_H_
