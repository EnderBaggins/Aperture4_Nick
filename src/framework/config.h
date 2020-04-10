#ifndef __CONFIG_H_
#define __CONFIG_H_

#include "core/enum_types.h"
#include "core/multi_array.hpp"
#include "core/typedefs_and_constants.h"
#include "utils/index.hpp"
#include "utils/logger.h"

namespace Aperture {

class sim_environment;

template <int Dim, typename FloatT = Scalar,
          template <int> typename Index_t = idx_col_major_t,
          MemoryModel MemModel = default_memory_model>
class Config {
 public:
  static constexpr int dim = Dim;
  static constexpr MemoryModel default_mem_model = MemModel;
  typedef FloatT value_type;
  typedef Index_t<Dim> index_type;
  typedef multi_array<FloatT, Dim, MemModel, Index_t<Dim>>
      multi_array_t;

  template <typename... Args>
  static multi_array_t make_multi_array(Args... args) {
    return multi_array_t(args...);
  }

  Config(const sim_environment& env) {}
  Config(const Config& other) = delete;
  Config(Config&& other) = default;
  Config& operator=(const Config& other) = delete;
  Config& operator=(Config&& other) = default;

};

}  // namespace Aperture

#endif
