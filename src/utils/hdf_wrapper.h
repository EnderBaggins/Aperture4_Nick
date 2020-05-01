#ifndef _HDF_WRAPPER_H_
#define _HDF_WRAPPER_H_

#include "core/multi_array.hpp"
#include "utils/vec.hpp"
#include "hdf5.h"
#include <string>
#include <vector>

namespace Aperture {

enum class H5OpenMode {
  read_write,
  read_only,
  rw_parallel,
  read_parallel
};

enum class H5CreateMode { trunc, excl, trunc_parallel, excl_parallel };

class H5File {
 private:
  hid_t m_file_id = 0;
  bool m_is_open = false;
  bool m_is_parallel = false;

 public:
  H5File();
  H5File(hid_t file_id);
  H5File(const std::string& filename,
         H5OpenMode mode = H5OpenMode::read_only);
  H5File(const H5File& other) = delete;
  H5File(H5File&& other);
  ~H5File();

  H5File& operator=(const H5File&) = delete;
  H5File& operator=(H5File&& other);

  void open(const std::string& filename,
            H5OpenMode mode = H5OpenMode::read_only);
  void close();

  template <typename T, int Dim>
  void write(const multi_array<T, Dim>& array, const std::string& name);
  template <typename T>
  void write(const T* array, size_t size, const std::string& name);
  template <typename T>
  void write(T value, const std::string& name);
  template <typename T, int Dim>
  void write_parallel(const multi_array<T, Dim>& array,
                      const extent_t<Dim>& ext_total, const index_t<Dim>& idx_dst,
                      const extent_t<Dim>& ext, const index_t<Dim>& idx_src,
                      const std::string& name);
  template <typename T>
  void write_parallel(const T* array, size_t array_size,
                      size_t len_total, size_t idx_dst, size_t len,
                      size_t idx_src, const std::string& name);

  template <typename T, int Dim>
  multi_array<T, Dim> read_multi_array(const std::string& name);
  template <typename T>
  buffer<T> read_array(const std::string& name);
  template <typename T>
  std::vector<T> read_vector(const std::string& name);
  template <typename T>
  T read_scalar(const std::string& name);

  template <typename T, int Dim>
  void read_subset(multi_array<T, Dim>& array, const std::string& name,
                   const index_t<Dim>& idx_src, const extent_t<Dim>& ext,
                   const index_t<Dim>& idx_dst);

  template <typename T>
  void read_subset(T* array, size_t array_size, const std::string& name,
                   size_t idx_src, size_t len, size_t idx_dst);

  void set_parallel(bool p) { m_is_parallel = p; }
};

H5File hdf_create(const std::string& filename,
                  H5CreateMode mode = H5CreateMode::trunc);

template <typename T>
hid_t h5datatype();

}  // namespace Aperture

#include "hdf_wrapper_impl.hpp"

#endif  // _HDF_WRAPPER_H_
