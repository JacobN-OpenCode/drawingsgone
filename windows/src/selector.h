// DrawingsGone for Windows — drag a rectangle.
#pragma once

#include <functional>

#include "common.h"

// Shows a full-screen picker. The callback gets (chosen, rect-in-screen-coords).
void beginRegionSelection(HINSTANCE instance, std::function<void(bool, RECT)> finished);
