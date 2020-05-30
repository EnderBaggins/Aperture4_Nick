#ifndef _RT_MAGNETAR_H_
#define _RT_MAGNETAR_H_

#include "systems/radiative_transfer.h"

namespace Aperture {

template <typename Conf>
struct rt_magnetar_impl_t;

template <typename Conf>
using rt_magnetar = radiative_transfer_cu<Conf, rt_magnetar_impl_t<Conf>>;

}



#endif  // _RT_MAGNETAR_H_