/*
 * Copyright (c) 2022 Alex Chen.
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

#include "domain_comm_impl.hpp"
#include "framework/config.h"
#include "systems/policies/exec_policy_host.hpp"

namespace Aperture {

template <typename Conf, template <class> class ExecPolicy>
void
domain_comm<Conf, ExecPolicy>::setup_devices() {}

// template class domain_comm<Config<1, Scalar>, exec_policy_host>;
// template class domain_comm<Config<2, Scalar>, exec_policy_host>;
// template class domain_comm<Config<3, Scalar>, exec_policy_host>;
INSTANTIATE_WITH_CONFIG(domain_comm, exec_policy_host);

}
