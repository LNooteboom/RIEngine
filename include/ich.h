#ifndef ICHIGO_WRAPPER_H
#define ICHIGO_WRAPPER_H

#include <ecs.h>

#include "ichigo.h"

#ifdef __cplusplus
constexpr ClWrapper<IchigoVm, ICHIGO_VM> ICHIGO_VMS;
#endif

#endif