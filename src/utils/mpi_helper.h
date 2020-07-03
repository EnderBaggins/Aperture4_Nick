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

#ifndef _MPI_HELPER_H_
#define _MPI_HELPER_H_

#include <mpi.h>

namespace Aperture {

namespace MPI_Helper {

template <typename T>
MPI_Datatype get_mpi_datatype(const T& x);

void handle_mpi_error(int error_code, int rank);

}  // namespace MPI_Helper

}  // namespace Aperture

#endif  // _MPI_HELPER_H_
