#pragma once

#include "burner.h"

/* pEmu/libcross2d owns the sole PS5 native presentation path. Keep the FBNeo
 * video interface's shared state definitions without registering its separate
 * desktop SDL presenters. */
#undef BUILD_SDL2
