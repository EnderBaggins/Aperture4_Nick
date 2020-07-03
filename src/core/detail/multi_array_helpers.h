/*
 * Copyright (c) 2020 Alex Chen.
 * This file is part of Aperture (https://github.com/fizban007/Aperture4.git).
 *
 * Aperture is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 *
 * Aperture is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _MULTI_ARRAY_HELPERS_H_
#define _MULTI_ARRAY_HELPERS_H_

#include "core/multi_array.hpp"
#include "data/fields.h"
#include "utils/stagger.h"

#ifndef CUDA_ENABLED
typedef int cudaStream_t;
#endif

namespace Aperture {

template <typename T, typename U, int Rank>
void resample(const multi_array<T, Rank>& from, multi_array<U, Rank>& to,
              const index_t<Rank>& offset_src,
              const index_t<Rank>& offest_dst,
              stagger_t st_src, stagger_t st_dst,
              int downsample = 1);

template <typename T, typename U, int Rank>
void resample_dev(const multi_array<T, Rank>& from, multi_array<U, Rank>& to,
                  const index_t<Rank>& offset_src,
                  const index_t<Rank>& offset_dst,
                  stagger_t st_src, stagger_t st_dst,
                  int downsample = 1, const cudaStream_t* stream = nullptr);

template <typename T, int Rank>
void add(multi_array<T, Rank>& dst, const multi_array<T, Rank>& src,
         const index_t<Rank>& dst_pos, const index_t<Rank>& src_pos,
         const extent_t<Rank>& ext, T scale = 1.0);

template <typename T, int Rank>
void add_dev(multi_array<T, Rank>& dst, const multi_array<T, Rank>& src,
             const index_t<Rank>& dst_pos, const index_t<Rank>& src_pos,
             const extent_t<Rank>& ext, T scale = 1.0, const cudaStream_t* stream = nullptr);

template <typename T, int Rank>
void copy(multi_array<T, Rank>& dst, const multi_array<T, Rank>& src,
          const index_t<Rank>& dst_pos, const index_t<Rank>& src_pos,
          const extent_t<Rank>& ext);

template <typename T, int Rank>
void copy_dev(multi_array<T, Rank>& dst, const multi_array<T, Rank>& src,
              const index_t<Rank>& dst_pos, const index_t<Rank>& src_pos,
              const extent_t<Rank>& ext, const cudaStream_t* stream = nullptr);

}

#endif  // _MULTI_ARRAY_HELPERS_H_
