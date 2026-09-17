#pragma once
#ifdef __APPLE__
#include <ApplicationServices/ApplicationServices.h>
#endif

namespace MacInputAccess {
struct Status {
    bool supported = false;
    bool accessibility = false;
    bool inputMonitoring = false;
    bool ready() const { return supported && accessibility && inputMonitoring; }
};
// Shared with the real reserved-key tap. Reads only: no TCC request, event tap,
// global input monitor, or settings mutation occurs during this check.
inline Status query()
{
#ifdef __APPLE__
    return {true, bool(AXIsProcessTrusted()), CGPreflightListenEventAccess()};
#else
    return {};
#endif
}
}
