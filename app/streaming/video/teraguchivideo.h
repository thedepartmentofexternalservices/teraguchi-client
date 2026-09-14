#pragma once

#include <Limelight.h>
#include "decoderpolicy.h"

namespace TeraguchiVideo {

#ifdef TERAGUCHI_STRICT_VIDEO
inline constexpr bool Required = true;
#else
inline constexpr bool Required = false;
#endif

inline constexpr bool acceptsCapture(DecoderCaptureSource source)
{
    return !Required || source == DecoderCaptureSource::NativeX11_10Bit;
}

inline constexpr bool acceptsFormat(DecoderEncoderBackend backend,
                                    int format, bool identityGbr)
{
    return !Required || (backend == DecoderEncoderBackend::NvencDirect &&
                         format == VIDEO_FORMAT_H265_REXT10_444 && identityGbr);
}

inline constexpr bool acceptsStream(int format, int width, int height, int fps,
                                    int expectedWidth, int expectedHeight, int expectedFps)
{
    return !Required || (format == VIDEO_FORMAT_H265_REXT10_444 &&
                         width > 0 && height > 0 && fps > 0 &&
                         width == expectedWidth && height == expectedHeight &&
                         fps == expectedFps);
}

// This boundary is shared by capability probes, initial creation and resets.
// A successful initializer alone never overrides a hardware-only request.
template<class Decoder, class Parameters>
bool initializeDecoder(Decoder& decoder, Parameters& parameters)
{
    if (Required) {
        parameters.selectionMode = DecoderSelectionMode::ExactHardwareOnly;
    }
    const bool hardwareOnly = parameters.selectionMode == DecoderSelectionMode::ExactHardwareOnly;
    return decoder.initialize(&parameters) &&
            (!hardwareOnly || decoder.isHardwareAccelerated());
}

} // namespace TeraguchiVideo
