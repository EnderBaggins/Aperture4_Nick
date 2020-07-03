#ifndef _ENUM_TYPES_H_
#define _ENUM_TYPES_H_

#include <cstdint>
#include <string>

namespace Aperture {

enum class MemType : char {
  host_only = 0,
  host_device,
  device_managed,
  device_only,
};

/// Field staggering type
enum class FieldType : char { face_centered = 0, edge_centered = 1 };

/// Particle types
enum class PtcType : unsigned char { electron = 0, positron, ion };

// This defines the maximum number of bits in the particle flag to represent the
// particle type
constexpr int max_ptc_type_bits = 3;
constexpr int max_ptc_types = 1 << max_ptc_type_bits;

inline std::string ptc_type_name(int type) {
  std::string result;
  if (type == (int)PtcType::electron) {
    result = "e";
  } else if (type == (int)PtcType::positron) {
    result = "p";
  } else if (type == (int)PtcType::ion) {
    result = "i";
  } else if (type == (int)PtcType::ion + 1) {
    result = "ph";
  } else {
    result = "unknown";
  }
  return result;
}

enum class CommTags : char { left = 0, right };

enum class Zone : char { center = 13 };

enum class BoundaryPos : char {
  lower0,
  upper0,
  lower1,
  upper1,
  lower2,
  upper2
};

// Use util functions check_flag, set_flag, flag_or, clear_flag, and
// toggle_flag to interact with particle and photon flags. These are
// defined from lower bits.
enum class PtcFlag : uint32_t {
  nothing = 0,
  tracked = 1,
  ignore_force,
  ignore_current,
  ignore_EM,
  ignore_radiation,
  primary,
  secondary,
  annihilate,
  emit_photon,
  max = emit_photon,
  count = max + 1
};

// One can define new flags by starting after the maximum in `PtcFlag`. For example:
//
// enum class PtcFlagEx : uint32_t {
//   thermal1 = (uint32_t)PtcFlag::count
// };
//
// Since all the flag functions are insensitive of the actual type, one can use
// them to set an extended flag on an existing uint32_t flag.

enum class PhFlag : uint32_t { tracked = 1, ignore_pair_create };

}  // namespace Aperture

#endif  // _ENUM_TYPES_H_
