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

#ifndef __ENVIRONMENT_H_
#define __ENVIRONMENT_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>

#include "data.h"
#include "framework/params_store.h"
#include "gsl/pointers"
#include "system.h"
#include "utils/logger.h"

namespace cxxopts {
class Options;
class ParseResult;
}  // namespace cxxopts

namespace Aperture {

// class callback_handler_t {
//  public:
//   template <typename Func>
//   void register_callback(const std::string& name, const Func& func) {
//     // callback_map[name].push_back(func);
//     callback_map.insert({name, func});
//   }

//   template <typename... Args>
//   void invoke_callback(const std::string& name, Args&... args) const {
//     auto it = callback_map.find(name);
//     if (it == callback_map.end()) {
//       Logger::print_err("Failed to find callback '{}'", name);
//       return;
//     } else {
//       auto& any_p = it->second;
//       std::function<void(Args & ...)> f;
//       try {
//         f = boost::any_cast<std::function<void(Args & ...)>>(any_p);
//       } catch (const boost::bad_any_cast& e) {
//         Logger::print_err("Failed to cast callback '{}': {}", name,
//         e.what()); return;
//       }
//       f(args...);
//     }
//   }

//  private:
//   // std::unordered_map<std::string, std::vector<std::any>> callback_map;
//   std::unordered_map<std::string, boost::any> callback_map;
// };

// class data_store_t {
//  public:
//   template <typename Data>
//   void save(const std::string& name, const Data& data) {
//     store_map[name] = &data;
//   }

//   template <typename T>
//   const T* get(const std::string& name) const {
//     auto it = store_map.find(name);
//     if (it != store_map.end()) {
//       auto& any_p = it->second;
//       if (any_p != nullptr) {
//         return reinterpret_cast<const T*>(any_p);
//       }
//     }
//     return nullptr;
//   }

//  private:
//   std::unordered_map<std::string, const void*> store_map;
// };

////////////////////////////////////////////////////////////////////////////////
///  Environment class that keeps a registry of parameters, data components, and
///  systems.
////////////////////////////////////////////////////////////////////////////////
class sim_environment {
 private:
  // Registry for systems and data
  std::unordered_map<std::string, std::unique_ptr<data_t>> m_data_map;
  std::unordered_map<std::string, std::unique_ptr<system_t>> m_system_map;
  std::vector<std::string> m_system_order;
  std::vector<std::string> m_data_order;

  // Modules that manage callback, shared data pointers, and parameters
  // callback_handler_t m_callback_handler;
  // data_store_t m_shared_data;
  params_store m_params;

  // Information about commandline arguments
  std::unique_ptr<cxxopts::Options> m_options;
  std::unique_ptr<cxxopts::ParseResult> m_commandline_args;

  void parse_options(int argc, char** argv);

  // These are variables governing the lifetime of the simulation
  bool is_dry_run = false;
  double dt;
  double time;
  uint32_t step;
  uint32_t max_steps;
  uint32_t perf_interval;

 public:
  typedef std::unordered_map<std::string, std::unique_ptr<data_t>> data_map_t;

  sim_environment();
  sim_environment(int* argc, char*** argv);
  ~sim_environment();

  ////////////////////////////////////////////////////////////////////////////////
  ///  Register a system class with the environment. This will either construct
  ///  a `unique_ptr` of the given `System` and insert it into the registry, or
  ///  return a reference to an existing one. The parameters are simply
  ///  constructor parameters. System names are given by their static `name()`
  ///  method. There cannot be more than one system registered under the same
  ///  name.
  ///
  ///  \tparam System  Type of the system to be registered. This template
  ///  parameter
  ///                 has to be specified.
  ///  \tparam Args    Types of the parameters to be sent to the constructor
  ///  \param args    The parameters to be sent to the system constructor
  ////////////////////////////////////////////////////////////////////////////////
  template <typename System, typename... Args>
  auto register_system(Args&&... args) -> gsl::not_null<System*> {
    const std::string& name = System::name();
    // Check if the system has already been installed. If so, return it
    // directly
    auto it = m_system_map.find(name);
    if (it != m_system_map.end())
      return dynamic_cast<System*>(it->second.get());

    // Otherwise, make the system, and return the pointer
    std::unique_ptr<system_t> ptr =
        std::make_unique<System>(std::forward<Args>(args)...);
    ptr->register_data_components();
    m_system_map.insert({name, std::move(ptr)});
    m_system_order.push_back(name);
    return dynamic_cast<System*>(m_system_map[name].get());
  }

  ////////////////////////////////////////////////////////////////////////////////
  ///  Register a data class with the environment. Multiple instances of the
  ///  same data type can be registered, as long as they have different names.
  ///
  ///  \tparam Data   Type of the data component to be registered. This template
  ///                 parameter has to be specified.
  ///  \tparam Args   Types of the parameters to be sent to the constructor
  ///  \param args    The parameters to be sent to the data constructor
  ////////////////////////////////////////////////////////////////////////////////
  template <typename Data, typename... Args>
  auto register_data(const std::string& name, Args&&... args)
      -> gsl::not_null<Data*> {
    // Check if the data component has already been installed
    auto it = m_data_map.find(name);
    if (it != m_data_map.end())
      // return std::dynamic_pointer_cast<Data>(it->second);
      return dynamic_cast<Data*>(it->second.get());

    // Otherwise, make the data, and return the pointer
    auto ptr = std::make_unique<Data>(std::forward<Args>(args)...);
    m_data_map.insert({name, std::move(ptr)});
    m_data_order.push_back(name);
    return dynamic_cast<Data*>(m_data_map[name].get());
  }

  ////////////////////////////////////////////////////////////////////////////////
  ///  Obtain a reference to the named system. If the system is not found
  ///  then a `nullptr` is returned.
  ///
  ///  \param name  Name of the system.
  ///  \return A raw pointer to the system
  ////////////////////////////////////////////////////////////////////////////////
  system_t* get_system(const std::string& name) {
    auto it = m_system_map.find(name);
    if (it != m_system_map.end()) {
      return it->second.get();
    } else {
      Logger::print_err("Failed to get system '{}'", name);
      return nullptr;
    }
  }

  ////////////////////////////////////////////////////////////////////////////////
  ///  Obtain a const raw pointer to the named system. If the system is not
  ///  found then a `nullptr` is returned.
  ///
  ///  \param name  Name of the system.
  ///  \return A const raw pointer to the system
  ////////////////////////////////////////////////////////////////////////////////
  const system_t* get_system(const std::string& name) const {
    auto it = m_system_map.find(name);
    if (it != m_system_map.end()) {
      return it->second.get();
    } else {
      Logger::print_err("Failed to get system '{}'", name);
      return nullptr;
    }
  }

  ////////////////////////////////////////////////////////////////////////////////
  ///  Obtain an optional raw pointer to the named data component. If the data
  ///  component is not found then a `nullptr` is returned.
  ///
  ///  \param name  Name of the data component.
  ///  \return A raw pointer to the data component
  ////////////////////////////////////////////////////////////////////////////////
  data_t* get_data_optional(const std::string& name) {
    auto it = m_data_map.find(name);
    if (it != m_data_map.end()) {
      return it->second.get();
    } else {
      Logger::print_info("Failed to get optional data component '{}'", name);
      return nullptr;
    }
  }

  ////////////////////////////////////////////////////////////////////////////////
  ///  Obtain a raw pointer to the named data component. If the data component
  ///  is not found then an exception is thrown.
  ///
  ///  \param name  Name of the data component.
  ///  \return A raw pointer to the data component
  ////////////////////////////////////////////////////////////////////////////////
  data_t* get_data(const std::string& name) {
    auto it = m_data_map.find(name);
    if (it != m_data_map.end()) {
      return it->second.get();
    } else {
      throw std::runtime_error("Data component not found: " + name);
    }
  }

  ////////////////////////////////////////////////////////////////////////////////
  ///  Obtain an optional const raw pointer to the named data component. If the
  ///  data component is not found then a `nullptr` is returned.
  ///
  ///  \param name  Name of the data component.
  ///  \param ptr   A const raw pointer to the data component, supplied to be
  ///  the output
  ////////////////////////////////////////////////////////////////////////////////
  template <typename T>
  void get_data(const std::string& name, T** ptr) {
    auto it = m_data_map.find(name);
    if (it != m_data_map.end()) {
      // ptr = std::dynamic_pointer_cast<T>(it->second);
      *ptr = dynamic_cast<T*>(it->second.get());
    } else {
      throw std::runtime_error("Data component not found: " + name);
    }
  }

  ////////////////////////////////////////////////////////////////////////////////
  ///  Obtain a const raw pointer to the named data component. If the data
  ///  component is not found then an exception is thrown.
  ///
  ///  \param name  Name of the data component.
  ///  \param ptr   A const raw pointer to the data component, supplied to be
  ///  the output
  ////////////////////////////////////////////////////////////////////////////////
  template <typename T>
  void get_data_optional(const std::string& name, T** ptr) {
    auto it = m_data_map.find(name);
    if (it != m_data_map.end()) {
      // ptr = std::dynamic_pointer_cast<T>(it->second);
      *ptr = dynamic_cast<T*>(it->second.get());
    } else {
      Logger::print_info("Failed to get optional data component '{}'", name);
      *ptr = nullptr;
    }
  }

  ////////////////////////////////////////////////////////////////////////////////
  ///  Initialize all systems in order defined. This calls all the `init()`
  ///  functions of the systems one by one.
  ////////////////////////////////////////////////////////////////////////////////
  void init();

  ////////////////////////////////////////////////////////////////////////////////
  ///  Start the main simulation loop. This enters a loop that calls the
  ///  `update()` function of each system in order at every time step.
  ////////////////////////////////////////////////////////////////////////////////
  void run();

  // data_store_t& shared_data() { return m_shared_data; }
  // const data_store_t& shared_data() const { return m_shared_data; }
  // callback_handler_t& callback_handler() { return m_callback_handler; }
  // const callback_handler_t& callback_handler() const { return
  // m_callback_handler; }
  params_store& params() { return m_params; }
  const params_store& params() const { return m_params; }
  const cxxopts::ParseResult* commandline_args() const {
    return m_commandline_args.get();
  }

  uint32_t get_step() const { return step; }
  void set_step(uint32_t s) { step = s; }
  double get_time() const { return time; }
  void set_time(double s) { step = s; }
  const data_map_t& data_map() { return m_data_map; }
};

}  // namespace Aperture

#endif
