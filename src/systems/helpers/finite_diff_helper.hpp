#ifndef __FINITE_DIFF_HELPER_H_
#define __FINITE_DIFF_HELPER_H_

#include "core/cuda_control.h"
#include "core/grid.hpp"
#include "utils/stagger.h"
#include "utils/vec.hpp"

namespace Aperture {

template <int Dir, typename PtrType>
HD_INLINE typename PtrType::value_t
diff(const PtrType& p, const typename PtrType::idx_t& idx, stagger_t stagger) {
  return p[idx.template inc<Dir>(stagger[Dir])] -
         p[idx.template dec<Dir>(1 - stagger[Dir])];
}

template <int Dim>
struct finite_diff;

template <>
struct finite_diff<1> {
  template <typename VecType, typename Idx_t, typename Stagger>
  HD_INLINE static Scalar div(const VecType& f, const Idx_t& idx,
                              const Stagger& st, const Grid<1>& g) {
    return diff<0>(f[0], idx, st[0]) * g.inv_delta[0];
  }

  template <typename VecType, typename Idx_t, typename Stagger>
  HD_INLINE static Scalar curl0(const VecType& f, const Idx_t& idx,
                                const Stagger& st, const Grid<1>& g) {
    return 0.0;
  }

  template <typename VecType, typename Idx_t, typename Stagger>
  HD_INLINE static Scalar curl1(const VecType& f, const Idx_t& idx,
                                const Stagger& st, const Grid<1>& g) {
    return -diff<0>(f[2], idx, st[2]) * g.inv_delta[0];
  }

  template <typename VecType, typename Idx_t, typename Stagger>
  HD_INLINE static Scalar curl2(const VecType& f, const Idx_t& idx,
                                const Stagger& st, const Grid<1>& g) {
    return diff<0>(f[1], idx, st[1]) * g.inv_delta[0];
  }
};

template <>
struct finite_diff<2> {
  template <typename VecType, typename Idx_t, typename Stagger>
  HD_INLINE static Scalar div(const VecType& f, const Idx_t& idx,
                              const Stagger& st, const Grid<2>& g) {
    return diff<0>(f[0], idx, st[0]) * g.inv_delta[0] +
           diff<1>(f[1], idx, st[1]) * g.inv_delta[1];
  }

  template <typename VecType, typename Idx_t, typename Stagger>
  HD_INLINE static Scalar curl0(const VecType& f, const Idx_t& idx,
                                const Stagger& st, const Grid<2>& g) {
    return diff<1>(f[2], idx, st[2]) * g.inv_delta[1];
  }

  template <typename VecType, typename Idx_t, typename Stagger>
  HD_INLINE static Scalar curl1(const VecType& f, const Idx_t& idx,
                                const Stagger& st, const Grid<2>& g) {
    return -diff<0>(f[2], idx, st[2]) * g.inv_delta[0];
  }

  template <typename VecType, typename Idx_t, typename Stagger>
  HD_INLINE static Scalar curl2(const VecType& f, const Idx_t& idx,
                                const Stagger& st, const Grid<2>& g) {
    return diff<0>(f[1], idx, st[1]) * g.inv_delta[0] -
           diff<1>(f[0], idx, st[0]) * g.inv_delta[1];
  }
};

template <>
struct finite_diff<3> {
  template <typename VecType, typename Idx_t, typename Stagger>
  HD_INLINE static Scalar div(const VecType& f, const Idx_t& idx,
                              const Stagger& st, const Grid<3>& g) {
    return diff<0>(f[0], idx, st[0]) * g.inv_delta[0] +
           diff<1>(f[1], idx, st[1]) * g.inv_delta[1] +
           diff<2>(f[2], idx, st[2]) * g.inv_delta[2];
  }

  template <typename VecType, typename Idx_t, typename Stagger>
  HD_INLINE static Scalar curl0(const VecType& f, const Idx_t& idx,
                                const Stagger& st, const Grid<3>& g) {
    return diff<1>(f[2], idx, st[2]) * g.inv_delta[1] -
           diff<2>(f[1], idx, st[1]) * g.inv_delta[2];
  }

  template <typename VecType, typename Idx_t, typename Stagger>
  HD_INLINE static Scalar curl1(const VecType& f, const Idx_t& idx,
                                const Stagger& st, const Grid<3>& g) {
    return diff<2>(f[0], idx, st[0]) * g.inv_delta[2] -
           diff<0>(f[2], idx, st[2]) * g.inv_delta[0];
  }

  template <typename VecType, typename Idx_t, typename Stagger>
  HD_INLINE static Scalar curl2(const VecType& f, const Idx_t& idx,
                                const Stagger& st, const Grid<3>& g) {
    return diff<0>(f[1], idx, st[1]) * g.inv_delta[0] -
           diff<1>(f[0], idx, st[0]) * g.inv_delta[1];
  }
};

}  // namespace Aperture

#endif  // __FINITE_DIFF_HELPER_H_
