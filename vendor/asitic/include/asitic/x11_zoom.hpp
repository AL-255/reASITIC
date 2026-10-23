#pragma once
#include "asitic/view_commands.hpp"
namespace asitic {
// Original 0x0807fb18. Screen-coordinate differences wrap as native 32-bit
// integers. Pan writes round to binary64, while their extended values remain
// live for the scale computation. No rectangle normalization or clamping.
void zoom_view_to_rectangle(int x0,int y0,int x1,int y1,NativeDispatchState&,ViewCommandState&);
}
