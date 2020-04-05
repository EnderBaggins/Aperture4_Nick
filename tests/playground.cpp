#include "core/particles.h"
#include "utils/logger.h"
#include "visit_struct/visit_struct.hpp"
#include <any>
#include <string>
#include <unordered_map>
#include <vector>

using namespace Aperture;

// The goal here is to define two systems. Each of them has some
// requirement in data components. Then build a `data` object that
// contain the required components.
//
struct Component1 {
  std::vector<float> v;
  float* ptr_v;

  void init() { ptr_v = v.data(); }
};

struct Component2 {
  std::vector<float> x;
  float* ptr_x;

  void init() { ptr_x = x.data(); }
};

struct Component3 {
  std::vector<uint32_t> cell;
};

// template <class ... Ts>
// struct Mixed : public Ts... {
//   void init() {
//     (Ts::init(), ...);
//   }
// };

template <template <typename> typename... Skills>
class X : public Skills<X<Skills...>>... {
 public:
  void basicMethod();
};

template <typename T, typename... Ts, typename Func>
void
variadic_loop(Func f, T& t, Ts&... ts) {
  f(t);
  variadic_loop(f, ts...);
}

template <typename T, typename Func>
void
variadic_loop(Func f, T& t) {
  f(t);
}

int
main(int argc, char* argv[]) {
  Logger::init(0, LogLevel::debug);
  uint32_t N = 1000;
  particles_t<MemoryModel::host_device> ptc(N);

  photons_t<> ph(N);

  return 0;
}
