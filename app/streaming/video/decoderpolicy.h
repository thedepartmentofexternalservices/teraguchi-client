#pragma once

enum class DecoderSelectionMode
{
    PreferExactHardwareThenSoftware,
    ExactHardwareOnly,
};

enum class DecoderCaptureSource
{
    Nvfbc8Bit,
    NativeX11_10Bit,
    ScreenCaptureKit,
};

enum class DecoderEncoderBackend
{
    SoftwareCuda,
    NvencDirect,
    VideoToolbox,
};
