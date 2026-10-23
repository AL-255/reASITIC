#pragma once
#include "asitic/x11_runtime.hpp"
namespace asitic {
// Original 0x0807ecf0 accepts a pointer to a 32-bit KeySym, rather than an
// XKeyEvent. The event loop performs key lookup before entering this routine.
// Ignored keys still calculate viewport endpoints. The KeySym read occurs
// between that arithmetic and the final viewport-height subtraction.
void handle_x11_keyboard(const unsigned*,GeometryShape*,X11State&,
    NativeDispatchState&,ViewCommandState&,const X11Runtime&);
}
