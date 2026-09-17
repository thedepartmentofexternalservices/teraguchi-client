#pragma once

#include <QtGlobal>

// The host supplies cursor shape and post-driver position. Keep the native
// overlay separate from the OS pointer so tablet margins do not change input.
#ifdef Q_OS_MACOS
#include "mactabletcursor.h"
using PlankTabletCursor = MacTabletCursor;
#else
#include "plankwaylandcursor.h"
using PlankTabletCursor = PlankWaylandCursor;
#endif
