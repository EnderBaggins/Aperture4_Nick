#ifndef __BUFFER_H_
#define __BUFFER_H_

#include "cuda_control.h"
#include "utils/logger.h"
#include <cstdlib>
#include <type_traits>

namespace Aperture {

enum class MemoryModel : char {
  host_only = 0,
  host_device,
  device_managed,
  device_only,
};

template <typename T, MemoryModel Model = MemoryModel::host_only>
class buffer_t {
 private:
  size_t m_size = 0;

  mutable T* m_data_h = nullptr;
  mutable T* m_data_d = nullptr;
  mutable bool m_host_valid = true;
  mutable bool m_dev_valid = true;
  bool m_host_allocated = false;
  bool m_dev_allocated = false;

  void alloc_mem(size_t size) {
    if constexpr (Model == MemoryModel::host_only ||
                  Model == MemoryModel::host_device) {
      // Allocate host
      m_data_h = new T[size];
      m_host_allocated = true;
    }
#ifdef CUDA_ENABLED
    if constexpr (Model != MemoryModel::host_only) {
      if constexpr (Model == MemoryModel::device_managed) {
        CudaSafeCall(cudaMallocManaged(&m_data_d, size * sizeof(T)));
        m_data_h = m_data_d;
      } else {
        CudaSafeCall(cudaMalloc(&m_data_d, size * sizeof(T)));
      }
      m_dev_allocated = true;
    }
#endif
    m_size = size;
  }

  void free_mem() {
    if (m_host_allocated) {
      delete[] m_data_h;
      m_data_h = nullptr;
      m_host_allocated = false;
    }
#ifdef CUDA_ENABLED
    if (m_dev_allocated) {
      CudaSafeCall(cudaFree(m_data_d));
      m_data_d = nullptr;
      m_dev_allocated = false;
    }
#endif
  }

 public:
  buffer_t() {}
  buffer_t(size_t size) { alloc_mem(size); }

  ~buffer_t() { free_mem(); }

  template <MemoryModel M = Model>
  std::enable_if_t<M != MemoryModel::device_only, T>
  operator[](size_t n) const {
    return host_ptr()[n];
  }

  template <MemoryModel M = Model>
  std::enable_if_t<M != MemoryModel::device_only, T&>
  operator[](size_t n) {
    return host_ptr()[n];
  }

  bool host_allocated() { return m_host_allocated; }
  bool dev_allocated() { return m_dev_allocated; }
  size_t size() { return m_size; }

  const T* data() const {
    if (m_host_allocated)
      return host_ptr();
    else if (m_dev_allocated)
      return dev_ptr();
    else
      return nullptr;
  }

  T* data() {
    if (m_host_allocated)
      return host_ptr();
    else if (m_dev_allocated)
      return dev_ptr();
    else
      return nullptr;
  }

  template <MemoryModel M = Model>
  std::enable_if_t<M != MemoryModel::device_only, const T*>
  host_ptr() const {
    if (!m_host_valid && m_dev_valid) copy_to_host();
    return m_data_h;
  }

  template <MemoryModel M = Model>
  std::enable_if_t<M != MemoryModel::device_only, T*>
  host_ptr() {
    m_host_valid = true;
    m_dev_valid = false;
    return m_data_h;
  }

  template <MemoryModel M = Model>
  std::enable_if_t<M != MemoryModel::host_only, const T*>
  dev_ptr() const {
    if (!m_dev_valid && m_host_valid) copy_to_device();
    return m_data_d;
  }

  template <MemoryModel M = Model>
  std::enable_if_t<M != MemoryModel::host_only, T*>
  dev_ptr() {
    m_dev_valid = true;
    m_host_valid = false;
    return m_data_d;
  }

  void copy_to_host() {
    m_host_valid = true;
    if constexpr (Model == MemoryModel::host_device) {
#ifdef CUDA_ENABLED
      CudaSafeCall(cudaMemcpy(m_data_h, m_data_d, m_size * sizeof(T),
                              cudaMemcpyDeviceToHost));
#endif
    }
  }

  void copy_to_device() {
    m_dev_valid = true;
    if constexpr (Model == MemoryModel::host_device) {
#ifdef CUDA_ENABLED
      CudaSafeCall(cudaMemcpy(m_data_d, m_data_h, m_size * sizeof(T),
                              cudaMemcpyHostToDevice));
#endif
    }
  }
};

}  // namespace Aperture

#endif  // __BUFFER_H_
