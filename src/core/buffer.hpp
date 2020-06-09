#ifndef __BUFFER_H_
#define __BUFFER_H_

#include "buffer_impl.hpp"
#include "core/cuda_control.h"
#include "core/enum_types.h"
#include "core/typedefs_and_constants.h"
#include "utils/logger.h"
#include <cstdlib>
#include <initializer_list>
#include <type_traits>

#ifdef CUDA_ENABLED
#include <thrust/copy.h>
#include <thrust/device_ptr.h>
#include <thrust/fill.h>
#else
typedef int cudaStream_t;
#endif

namespace Aperture {

////////////////////////////////////////////////////////////////////////////////
/// A class for linear buffers that manages resources both on the host
/// and the device.
////////////////////////////////////////////////////////////////////////////////
template <typename T>
class buffer {
 protected:
  size_t m_size = 0;

  mutable T* m_data_h = nullptr;
  mutable T* m_data_d = nullptr;
  // mutable bool m_host_valid = true;
  // mutable bool m_dev_valid = true;
  bool m_host_allocated = false;
  bool m_dev_allocated = false;
  MemType m_type = default_mem_type;

  void alloc_mem(size_t size) {
    if (m_type == MemType::host_only || m_type == MemType::host_device) {
      // Allocate host
      m_data_h = new T[size];
      m_host_allocated = true;
    }
#ifdef CUDA_ENABLED
    if (m_type != MemType::host_only) {
      if (m_type == MemType::device_managed) {
        CudaSafeCall(cudaMallocManaged(&m_data_d, size * sizeof(T)));
        m_data_h = m_data_d;
      } else {
        CudaSafeCall(cudaMalloc(&m_data_d, size * sizeof(T)));
      }
      m_dev_allocated = true;
    }
#endif
    m_size = size;
    Logger::print_debug("Allocated {} bytes", size * sizeof(T));
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
  typedef buffer<T> self_type;

  buffer(MemType type = default_mem_type) : m_type(type) {}
  buffer(size_t size, MemType type = default_mem_type) : m_type(type) {
    alloc_mem(size);
  }
  buffer(const self_type& other) = delete;
  buffer(self_type&& other) { *this = std::move(other); }

  ~buffer() { free_mem(); }

  self_type& operator=(const self_type& other) = delete;
  self_type& operator=(self_type&& other) {
    m_type = other.m_type;
    m_size = other.m_size;
    other.m_size = 0;

    m_host_allocated = other.m_host_allocated;
    m_dev_allocated = other.m_dev_allocated;
    other.m_host_allocated = false;
    other.m_dev_allocated = false;

    m_data_h = other.m_data_h;
    m_data_d = other.m_data_d;
    other.m_data_h = nullptr;
    other.m_data_d = nullptr;

    return *this;
  }

  /// Subscript operator, const version. This is only defined for the host
  /// pointer, so if the buffer is device_only then this will give a
  /// segmentation fault.
  inline T operator[](size_t n) const {
    return m_data_h[n];
  }

  /// Subscript operator. This is only defined for the host pointer, so if the
  /// buffer is device_only then this will give a segmentation fault.
  inline T& operator[](size_t n) {
    return m_data_h[n];
  }

  /// Check the memory type of this buffer.
  MemType mem_type() const { return m_type; }

  /// Set the memory location. This should always be followed by a resize,
  /// otherwise the actual memory location may be inconsistent.
  void set_memtype(MemType type) { m_type = type; }

  /// Resize the buffer to a given size. Reallocate all memory.
  ///
  /// \param size New size of the buffer, in elements.
  void resize(size_t size) {
    if (m_host_allocated || m_dev_allocated) {
      free_mem();
    }
    alloc_mem(size);
    // m_host_valid = true;
    // m_dev_valid = true;
  }

  /// Assign a single value to part of the buffer, host version
  void assign_host(size_t start, size_t end, const T& value) {
    // Do not go further than the array size
    end = std::min(m_size, end);
    start = std::min(start, end);
    if (m_host_allocated) ptr_assign(m_data_h, start, end, value);
  }

  /// Assign a single value to part of the buffer, device version
  void assign_dev(size_t start, size_t end, const T& value) {
    // Do not go further than the array size
    end = std::min(m_size, end);
    start = std::min(start, end);
    if (m_dev_allocated) ptr_assign_dev(m_data_d, start, end, value);
  }

  /// Assign a single value to part of the buffer. Calls the host or device
  /// version depending on the memory location
  void assign(size_t start, size_t end, const T& value) {
    if (m_type == MemType::host_only) {
      assign_host(start, end, value);
    } else {
      assign_dev(start, end, value);
    }
  }

  /// Assign a value to the whole buffer. Calls the host or device version
  /// depending on the memory location
  void assign(const T& value) { assign(0, m_size, value); }

  /// Assign a value to the whole buffer. Host version
  void assign_host(const T& value) { assign_host(0, m_size, value); }

  /// Assign a value to the whole buffer. Device version
  void assign_dev(const T& value) { assign_dev(0, m_size, value); }

  ///  Copy a part from another buffer. Will do the copy on the host or device
  ///  side depending on the memory location. If the buffer is `host_only`, then
  ///  only copy on the host side. Otherwise, only copy on the device side.
  ///
  ///  \param other  The other buffer that we are copying from
  ///  \param num    Number of elements to copy
  ///  \param src_pos   Starting position in the other buffer
  ///  \param dst_pos  Starting position in this buffer (the target)
  void copy_from(const self_type& other, size_t num, size_t src_pos = 0,
                 size_t dst_pos = 0) {
    if (other.m_type == MemType::host_only ||
        m_type == MemType::host_only) {
      host_copy_from(other, num, src_pos, dst_pos);
    } else {
      dev_copy_from(other, num, src_pos, dst_pos);
    }
  }

  ///  Copy a part from another buffer through host memory.
  ///
  ///  \param other  The other buffer that we are copying from
  ///  \param num    Number of elements to copy
  ///  \param src_pos   Starting position in the other buffer
  ///  \param dst_pos  Starting position in this buffer (the target)
  void host_copy_from(const self_type& other, size_t num, size_t src_pos = 0,
                      size_t dst_pos = 0) {
    // Sanitize input
    if (dst_pos + num > m_size) num = m_size - dst_pos;
    if (src_pos + num > other.m_size) num = other.m_size - src_pos;
    if (m_host_allocated && other.m_host_allocated) {
      ptr_copy(other.m_data_h, m_data_h, num, src_pos, dst_pos);
    }
  }

  ///  Copy a part from another buffer through device memory
  ///
  ///  \param other  The other buffer that we are copying from
  ///  \param num    Number of elements to copy
  ///  \param src_pos   Starting position in the other buffer
  ///  \param dst_pos  Starting position in this buffer (the target)
  void dev_copy_from(const self_type& other, size_t num, size_t src_pos = 0,
                      size_t dst_pos = 0) {
    // Sanitize input
    if (dst_pos + num > m_size) num = m_size - dst_pos;
    if (src_pos + num > other.m_size) num = other.m_size - src_pos;
    if (m_dev_allocated && other.m_dev_allocated) {
      ptr_copy_dev(other.m_data_d, m_data_d, num, src_pos, dst_pos);
    }
  }

  ///  Copy from the whole other buffer. Will do the copy on the host or device
  ///  side depending on the memory location. If the buffer is `host_only`, then
  ///  only copy on the host side. Otherwise, only copy on the device side.
  ///
  ///  \param other  The other buffer that we are copying from
  void copy_from(const self_type& other) {
    copy_from(other, other.m_size, 0, 0);
  }

  ///  Copy from the whole other buffer through host memory
  ///
  ///  \param other  The other buffer that we are copying from
  void host_copy_from(const self_type& other) {
    host_copy_from(other, other.m_size, 0, 0);
  }

  ///  Copy from the whole other buffer through device memory
  ///
  ///  \param other  The other buffer that we are copying from
  void dev_copy_from(const self_type& other) {
    dev_copy_from(other, other.m_size, 0, 0);
  }

  ///  Place some values directly at and after @pos. Very useful for
  ///  initialization.
  void emplace(size_t pos, const std::initializer_list<T>& list) {
    if (m_type == MemType::device_only) return;
    for (auto& t : list) {
      if (pos >= m_size) break;
      m_data_h[pos] = t;
      pos += 1;
    }
  }

  /// Check if the buffer is allocated in host memory
  bool host_allocated() const { return m_host_allocated; }

  /// Check if the buffer is allocated in device memory
  bool dev_allocated() const { return m_dev_allocated; }

  /// Size of the allocated buffer in elements
  size_t size() const { return m_size; }

  /// Return the pointer to the data, const version. This is only for interface
  /// compatibility with std data structures. The user is encouraged to use
  /// host_ptr() or dev_ptr() directly depending on the use case.
  const T* data() const {
    if (m_type == MemType::host_only || m_type == MemType::host_device)
      return m_data_h;
    else
      return m_data_d;
  }

  /// Return the pointer to the data. This is only for interface
  /// compatibility with std data structures. The user is encouraged to use
  /// host_ptr() or dev_ptr() directly depending on the use case.
  T* data() {
    if (m_type == MemType::host_only || m_type == MemType::host_device)
      return m_data_h;
    else
      return m_data_d;
  }

  /// Return the pointer to the host memory, const version
  const T* host_ptr() const { return m_data_h; }

  /// Return the pointer to the host memory
  T* host_ptr() { return m_data_h; }

  /// Return the pointer to the device memory, const version
  const T* dev_ptr() const { return m_data_d; }

  /// Return the pointer to the device memory
  T* dev_ptr() { return m_data_d; }

  /// Copy from device to host. This will block host code execution.
  void copy_to_host() {
    if (m_type == MemType::host_device) {
#ifdef CUDA_ENABLED
      CudaSafeCall(cudaMemcpy(m_data_h, m_data_d, m_size * sizeof(T),
                              cudaMemcpyDeviceToHost));
#endif
    }
  }

  /// Copy from device to host, on a given stream. This will not block host code
  /// execution. To ensure data copy is complete, synchronize the stream
  /// manually.
  void copy_to_host(cudaStream_t stream) {
    if (m_type == MemType::host_device) {
#ifdef CUDA_ENABLED
      CudaSafeCall(cudaMemcpyAsync(m_data_h, m_data_d, m_size * sizeof(T),
                                   cudaMemcpyDeviceToHost, stream));
#endif
    }
  }

  /// Copy from host to device. This will block host code execution.
  void copy_to_device() {
    if (m_type == MemType::host_device) {
#ifdef CUDA_ENABLED
      CudaSafeCall(cudaMemcpy(m_data_d, m_data_h, m_size * sizeof(T),
                              cudaMemcpyHostToDevice));
#endif
    }
  }

  /// Copy from host to device, on a given stream. This will not block host code
  /// execution. To ensure data copy is complete, synchronize the stream
  /// manually.
  void copy_to_device(cudaStream_t stream) {
    if (m_type == MemType::host_device) {
#ifdef CUDA_ENABLED
      CudaSafeCall(cudaMemcpyAsync(m_data_d, m_data_h, m_size * sizeof(T),
                                   cudaMemcpyHostToDevice, stream));
#endif
    }
  }
};

}  // namespace Aperture

#endif  // __BUFFER_H_
