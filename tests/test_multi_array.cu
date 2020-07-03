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

#include "catch.hpp"
#include "core/constant_mem_func.h"
#include "core/constant_mem.h"
#include "core/multi_array.hpp"
#include "core/multi_array_exp.hpp"
#include "core/ndptr.hpp"
#include "utils/logger.h"
#include "utils/interpolation.hpp"
#include "utils/kernel_helper.hpp"
#include "utils/range.hpp"
#include "utils/timer.h"
#include <algorithm>
#include <random>
#include <thrust/tuple.h>

using namespace Aperture;

#ifdef CUDA_ENABLED

TEST_CASE("Invoking kernels on multi_array", "[multi_array][kernel]") {
  uint32_t N1 = 100, N2 = 300;
  auto ext = extent(N1, N2);
  auto array = make_multi_array<float>(ext, MemType::host_device);
  REQUIRE(array.host_allocated() == true);
  REQUIRE(array.dev_allocated() == true);

  kernel_launch(
      [] __device__(auto p, float value, auto ext) {
        for (auto idx : grid_stride_range(0u, ext.size())) {
          p[idx] = value;
        }
      },
      array.dev_ndptr(), 3.0f, ext);
  CudaSafeCall(cudaDeviceSynchronize());

  array.copy_to_host();

  for (auto idx : array.indices()) {
    REQUIRE(array[idx] == 3.0f);
  }
}

TEST_CASE("Different indexing on multi_array",
          "[multi_array][kernel]") {
  Logger::init(0, LogLevel::debug);
  uint32_t N1 = 32, N2 = 32;
  // Extent ext(1, N2, N1);
  auto ext = extent(N2, N1);
  // multi_array<float, idx_row_major_t<>> array(
  auto array = make_multi_array<float,
                                idx_zorder_t>(ext, MemType::device_managed);
  // auto array = make_multi_array<float, MemType::device_managed,
  // idx_row_major_t>(ext);

  // assign_idx_array<<<128, 512>>>(array.dev_ndptr(), ext);
  kernel_launch(
      [] __device__(auto p, auto ext) {
        for (auto i : grid_stride_range(0u, ext.size())) {
          auto idx = p.idx_at(i, ext);
          auto pos = idx.get_pos();
          p[i] = pos[0] * pos[1];
        }
      },
      array.dev_ndptr(), ext);
  CudaSafeCall(cudaDeviceSynchronize());

  for (auto idx : array.indices()) {
    auto pos = idx.get_pos();
    REQUIRE(array[idx] == Approx((float)pos[0] * pos[1]));
  }
}

TEST_CASE("Performance of different indexing schemes",
          "[multi_array][performance][kernel][.]") {
  init_morton(morton2dLUT, morton3dLUT);
  uint32_t N = 128;
  uint32_t N1 = N, N2 = N, N3 = N;
  std::default_random_engine g;
  std::uniform_real_distribution<float> dist(0.0, 1.0);
  std::uniform_int_distribution<uint32_t> cell_dist(0, N1 * N2 * N3);

  auto ext = extent(N1, N2, N3);
  // multi_array<float, idx_row_major_t<>> array(
  auto v1 = make_multi_array<float,
                             idx_col_major_t>(ext, MemType::host_device);
  auto v2 =
      make_multi_array<float, idx_zorder_t>(
          ext, MemType::host_device);

  for (auto idx : v1.indices()) {
    auto pos = idx.get_pos();
    v1[idx] = float(0.3 * pos[0] + 0.4 * pos[1] - pos[2]);
  }
  for (auto idx : v2.indices()) {
    auto pos = idx.get_pos();
    v2[idx] = float(0.3 * pos[0] + 0.4 * pos[1] - pos[2]);
  }
  for (auto idx : v1.indices()) {
    auto pos = idx.get_pos();
    REQUIRE(v1(pos[0], pos[1], pos[2]) == v2(pos[0], pos[1], pos[2]));
  }
  v1.copy_to_device();
  v2.copy_to_device();

  // Generate M random numbers
  int M = 1000000;
  buffer<float> xs(M, MemType::host_device);
  buffer<float> ys(M, MemType::host_device);
  buffer<float> zs(M, MemType::host_device);
  buffer<float> result1(M, MemType::host_device);
  buffer<float> result2(M, MemType::host_device);
  buffer<uint32_t> cells1(M, MemType::host_device);
  buffer<uint32_t> cells2(M, MemType::host_device);
  for (int n = 0; n < M; n++) {
    xs[n] = dist(g);
    ys[n] = dist(g);
    zs[n] = dist(g);
    cells1[n] = cell_dist(g);
    auto pos = v1.idx_at(cells1[n]).get_pos();
    auto idx = v2.get_idx(pos[0], pos[1], pos[2]);
    cells2[n] = idx.linear;
    result1[n] = 0.0f;
    result2[n] = 0.0f;
  }
  // std::sort(cells1.host_ptr(), cells1.host_ptr() + cells1.size());
  // std::sort(cells2.host_ptr(), cells2.host_ptr() + cells2.size());
  xs.copy_to_device();
  ys.copy_to_device();
  zs.copy_to_device();
  cells1.copy_to_device();
  cells2.copy_to_device();
  result1.copy_to_device();
  result2.copy_to_device();

  auto interp_kernel = [N1, N2, N3, M] __device__(
                           auto f, float* result, float* xs, float* ys,
                           float* zs, uint32_t* cells, auto ext) {
    for (uint32_t i : grid_stride_range(0, M)) {
      uint32_t cell = cells[i];
      auto idx = f.idx_at(cell, ext);
      auto pos = idx.get_pos();
      if (pos[0] < N1 - 1 && pos[1] < N2 - 1 && pos[2] < N3 - 1) {
        // result[i] = x;
        result[i] = lerp3(f, xs[i], ys[i], zs[i], idx);
      }
    }
  };

  cudaDeviceSynchronize();

  timer::stamp();
  kernel_launch(interp_kernel, v1.dev_ndptr_const(), result1.dev_ptr(),
                xs.dev_ptr(), ys.dev_ptr(), zs.dev_ptr(),
                cells1.dev_ptr(), ext);
  cudaDeviceSynchronize();
  timer::show_duration_since_stamp("normal indexing", "us");

  timer::stamp();
  kernel_launch(interp_kernel, v2.dev_ndptr_const(), result2.dev_ptr(),
                xs.dev_ptr(), ys.dev_ptr(), zs.dev_ptr(),
                cells2.dev_ptr(), ext);
  cudaDeviceSynchronize();
  timer::show_duration_since_stamp("morton indexing", "us");

  result1.copy_to_host();
  result2.copy_to_host();

  for (auto idx : range(0ul, result1.size())) {
    REQUIRE(result1[idx] == result2[idx]);
  }
}

TEST_CASE("Assign and copy on device", "[multi_array][kernel]") {
  auto v1 = make_multi_array<float>(extent(30, 30));
  auto v2 = make_multi_array<float>(extent(30, 30));

  v1.assign_dev(3.0f);
  v1.copy_to_host();
  for (auto idx : v1.indices()) {
    REQUIRE(v1[idx] == 3.0f);
  }
}

TEST_CASE("Add ndptr on device", "[multi_array][exp_template]") {
  auto ext = extent(30, 30);
  auto v1 = make_multi_array<float>(ext);
  auto v2 = make_multi_array<float>(ext);
  auto v3 = make_multi_array<float>(ext);

  v1.assign_dev(1.0f);
  v2.assign_dev(2.0f);

  kernel_launch({30, 30}, [ext]__device__(auto p1, auto p2, auto p3) {
      using idx_t = default_idx_t<2>;
      for (auto idx : grid_stride_range(idx_t(0, ext), idx_t(ext.size(), ext))) {
        p3[idx] = (p1 * p2)[idx];
      }
    }, v1.dev_ndptr_const(), v2.dev_ndptr_const(), v3.dev_ndptr());
  CudaSafeCall(cudaDeviceSynchronize());
  CudaCheckError();

  v3.copy_to_host();
  for (auto idx : v3.indices()) {
    REQUIRE(v3[idx] == 2.0f);
  }
}


#endif
