#ifndef __PARTICLES_H_
#define __PARTICLES_H_

#include "particle_structs.h"
#include "core/buffer.hpp"
#include "utils/vec.hpp"
#include "systems/grid.h"
#include <vector>

namespace Aperture {

template <int N>
struct Grid;

template <typename BufferType>
class particles_base : public BufferType {
 public:
  typedef BufferType base_type;
  typedef particles_base<BufferType> self_type;
  typedef typename BufferType::single_type single_type;
  typedef typename BufferType::ptrs_type ptrs_type;

  particles_base(MemType model = default_mem_type);
  particles_base(size_t size, MemType model = default_mem_type);
  particles_base(const self_type& other) = delete;
  particles_base(self_type&& other) = default;
  ~particles_base() {}

  self_type& operator=(const self_type& other) = delete;
  self_type& operator=(self_type&& other) = default;

  void set_memtype(MemType memtype);
  MemType mem_type() const { return m_mem_type; }

  void resize(size_t size);

  void copy_from(const self_type& other, size_t num, size_t src_pos,
                 size_t dst_pos);

  void erase(size_t pos, size_t amount = 1);

  void init() { erase(0, m_size); }

  void sort_by_cell(size_t max_cell);
  void sort_by_cell_host(size_t max_cell);
  void sort_by_cell_dev(size_t max_cell);

  void append(const vec_t<Pos_t, 3>& x, const vec_t<Scalar, 3>& p,
              uint32_t cell, Scalar weight = 1.0, uint32_t flag = 0);

  void append_dev(const vec_t<Pos_t, 3>& x, const vec_t<Scalar, 3>& p,
                  uint32_t cell, Scalar weight = 1.0, uint32_t flag = 0);

  void copy_to_host();
  void copy_to_device();
  void copy_to_host(cudaStream_t stream);
  void copy_to_device(cudaStream_t stream);

  template <typename Conf>
  void copy_to_comm_buffers(std::vector<self_type>& buffers,
                            buffer<ptrs_type>& buf_ptrs,
                            const grid_t<Conf>& grid);

  size_t size() const { return m_size; }
  size_t number() const { return m_number; }

  void set_num(size_t num) {
    // Can't set a number larger than maximum size
    m_number = std::min(num, m_size);
  }

  void add_num(size_t num) { set_num(m_number + num); }

  typename BufferType::ptrs_type& get_host_ptrs() { return m_host_ptrs; }
  typename BufferType::ptrs_type& get_dev_ptrs() { return m_dev_ptrs; }
  buffer<uint32_t>& ptc_id() { return m_ptc_id; }

 private:
  size_t m_size = 0;
  size_t m_number = 0;
  MemType m_mem_type;
  buffer<uint32_t> m_ptc_id;

  // Temporary data for sorting particles on device
  buffer<size_t> m_index;
  buffer<double> m_tmp_data;
  buffer<int> m_zone_buffer_num;
  // Temporary data for sorting particles on host
  std::vector<size_t> m_partition;

  typename BufferType::ptrs_type m_host_ptrs;
  typename BufferType::ptrs_type m_dev_ptrs;

  void rearrange_arrays(const std::string& skip);
  void rearrange_arrays_host();
  void swap(size_t pos, single_type& p);
};

using particles_t = particles_base<ptc_buffer>;
using photons_t = particles_base<ph_buffer>;

}  // namespace Aperture

#endif

