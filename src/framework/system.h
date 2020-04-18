#ifndef __SYSTEM_H_
#define __SYSTEM_H_

#include <cstdint>
#include <set>
#include <string>

namespace Aperture {

class sim_environment;
class data_store_t;

class system_t {
 public:
  system_t(sim_environment& env) : m_env(env) {}

  virtual void init() {}
  virtual void register_dependencies() {}
  virtual void update(double, uint32_t) {}

 protected:
  sim_environment& m_env;
  std::set<std::string> m_dependencies;
};

}  // namespace Aperture

#endif
