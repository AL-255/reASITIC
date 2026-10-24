#pragma once
#ifdef __EMSCRIPTEN__
#include "soft_real.hpp"
namespace reasitic { SoftReal runtime_sin(double);SoftReal runtime_cos(double); }
#else
namespace reasitic { long double runtime_sin(double);long double runtime_cos(double); }
#endif
