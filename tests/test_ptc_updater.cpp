#include "catch.hpp"
#include "systems/ptc_updater.h"
#include "framework/environment.hpp"
#include "framework/config.h"

using namespace Aperture;

TEST_CASE("Boris push in a uniform B field", "[boris][.]") {
  typedef Config<2> Conf;
  sim_environment env;
  env.params().add("log_level", 2l);
  env.params().add("N", std::vector<int64_t>({64, 64, 64}));
  env.params().add("guard", std::vector<int64_t>({2, 2, 2}));
  env.params().add("size", std::vector<double>({1.0, 1.0, 1.0}));
  env.params().add("lower", std::vector<double>({0.0, 0.0, 0.0}));

  auto comm = env.register_system<domain_comm<Conf>>(env);
  auto grid = env.register_system<grid_t<Conf>>(env, *comm);
  auto pusher = env.register_system<ptc_updater<Conf>>(env, *grid, *comm);

  env.init();

  std::shared_ptr<vector_field<Conf>> B;
  env.get_data("B", B);
  (*B)[2].assign(100.0);
  std::shared_ptr<particle_data_t<Conf::default_ptc>> ptc;
  env.get_data("particles", ptc);
}
