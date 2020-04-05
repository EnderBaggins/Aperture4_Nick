#ifndef _UTIL_FUNCTIONS_H_
#define _UTIL_FUNCTIONS_H_

#include "core/cuda_control.h"
#include "core/typedefs.h"
#include "core/enum_types.h"
#include <string>

namespace Aperture {

template <typename T>
HD_INLINE T
square(const T &val) {
  return val * val;
}

template <typename T>
HD_INLINE T
cube(const T &val) {
  return val * val * val;
}

template <typename T>
HD_INLINE int
sgn(T val) {
  return (T(0) < val) - (val < T(0));
}

template <typename Flag>
HD_INLINE bool
check_flag(uint32_t flag, Flag bit) {
  return (flag & (1 << static_cast<int>(bit))) != 0;
}

template <typename Flag>
HD_INLINE uint32_t
bit_or(Flag bit) {
  return (1 << static_cast<int>(bit));
}

template <typename Flag, typename... P>
HD_INLINE uint32_t
bit_or(Flag bit, P... bits) {
  return ((1 << static_cast<int>(bit)) | bit_or(bits...));
}

template <typename... Flag>
HD_INLINE void
set_flag(uint32_t& flag, Flag... bits) {
  flag |= bit_or(bits...);
}

template <typename... Flag>
HD_INLINE void
clear_flag(uint32_t& flag, Flag... bits) {
  flag &= ~static_cast<int>(bit_or(bits...));
}

template <typename... Flag>
HD_INLINE void
toggle_flag(uint32_t& flag, Flag... bits) {
  flag ^= static_cast<int>(bit_or(bits...));
}

// Get an integer representing particle type from a given flag
HD_INLINE int
get_ptc_type(uint32_t flag) {
  return (int)(flag >> 29);
}

// Generate a particle flag from a give particle type
HD_INLINE uint32_t
gen_ptc_type_flag(PtcType type) {
  return ((uint32_t)type << 29);
}

// Set a given flag such that it now represents given particle type
HD_INLINE uint32_t
set_ptc_type_flag(uint32_t flag, PtcType type) {
  return (flag & ((uint32_t)-1 >> 3)) | gen_ptc_type_flag(type);
}

}  // namespace Aperture

#endif  // _UTIL_FUNCTIONS_H_
