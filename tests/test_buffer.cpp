#include "catch.hpp"
#include "utils/buffer.h"

using namespace Aperture;

TEST_CASE("Host only buffer", "[buffer]") {
  uint32_t N = 1000;

  buffer_t<double> buf(N);

  for (int i = 0; i < buf.size(); i++) {
    buf[i] = i;
  }
  for (int i = 0; i < buf.size(); i++) {
    REQUIRE(buf[i] == Approx(i));
  }

  SECTION("Move assignment and constructor") {
    buffer_t<double> buf1 = std::move(buf);

    REQUIRE(buf.host_allocated() == false);
    REQUIRE(buf.host_ptr() == nullptr);
    REQUIRE(buf1.size() == N);

    buffer_t<double> buf2(std::move(buf1));

    REQUIRE(buf1.host_allocated() == false);
    REQUIRE(buf1.host_ptr() == nullptr);
    REQUIRE(buf2.size() == N);

    for (int i = 0; i < buf2.size(); i++) {
      REQUIRE(buf2[i] == Approx(i));
    }
  }
}

TEST_CASE("Host device buffer", "[buffer]") {
  uint32_t N = 1000;

  buffer_t<double, MemoryModel::host_device> buf(N);

  REQUIRE(buf.host_allocated() == true);
#ifdef CUDA_ENABLED
  REQUIRE(buf.dev_allocated() == true);
#endif

  auto ptr = buf.data();
  ptr[300] = 3.0;
  REQUIRE(ptr[300] == 3.0);

#ifdef CUDA_ENABLED
  // Test coping to device and back
  buf.copy_to_device();
  ptr[300] = 6.0;
  REQUIRE(ptr[300] == 6.0);
  buf.copy_to_host();
  REQUIRE(ptr[300] == 3.0);
#endif

  SECTION("Move assignment and constructor") {
    buffer_t<double, MemoryModel::host_device> buf1 = std::move(buf);

    REQUIRE(buf.host_allocated() == false);
#ifdef CUDA_ENABLED
    REQUIRE(buf.dev_allocated() == false);
#endif
    REQUIRE(buf.host_ptr() == nullptr);
    REQUIRE(buf.dev_ptr() == nullptr);
    REQUIRE(buf1.size() == N);
    REQUIRE(buf1.host_allocated() == true);

    buffer_t<double, MemoryModel::host_device> buf2(std::move(buf1));

    REQUIRE(buf1.host_allocated() == false);
    REQUIRE(buf1.host_ptr() == nullptr);
#ifdef CUDA_ENABLED
    REQUIRE(buf1.dev_ptr() == nullptr);
#endif
    REQUIRE(buf2.size() == N);
  }
}

TEST_CASE("Managed buffer", "[buffer]") {
  uint32_t N = 1000;

  buffer_t<double, MemoryModel::device_managed> buf(N);

  REQUIRE(buf.host_allocated() == false);
#ifdef CUDA_ENABLED
  REQUIRE(buf.dev_allocated() == true);

  for (int i = 0; i < buf.size(); i++) {
    buf[i] = i;
  }
  for (int i = 0; i < buf.size(); i++) {
    REQUIRE(buf[i] == Approx(i));
  }
#endif
}

#ifdef CUDA_ENABLED
TEST_CASE("Device only buffer", "[buffer]") {
  uint32_t N = 1000;

  buffer_t<double, MemoryModel::device_only> buf(N);

  REQUIRE(buf.host_allocated() == false);
  REQUIRE(buf.dev_allocated() == true);
}
#endif
