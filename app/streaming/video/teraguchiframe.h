#pragma once

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/hwcontext.h>
#include <libavutil/pixdesc.h>
}

// Hardware status must come from the created VideoToolbox session, not a
// requested flag or the decoder name. Reject before queueing for presentation.
inline bool teraguchiNativeFrameMatches(const AVFrame* frame, AVCodecID codec,
                                        int profile, int hardwareStatus,
                                        int expectedWidth, int expectedHeight)
{
    if (!frame || codec != AV_CODEC_ID_HEVC || profile != AV_PROFILE_HEVC_REXT ||
            hardwareStatus != 1 || frame->format != AV_PIX_FMT_VIDEOTOOLBOX ||
            !frame->data[3] || !frame->hw_frames_ctx ||
            frame->hw_frames_ctx->size < sizeof(AVHWFramesContext) ||
            expectedWidth <= 0 || expectedHeight <= 0 ||
            frame->width != expectedWidth || frame->height != expectedHeight ||
            frame->colorspace != AVCOL_SPC_RGB || frame->color_range != AVCOL_RANGE_JPEG) {
        return false;
    }
    const auto* context = reinterpret_cast<const AVHWFramesContext*>(frame->hw_frames_ctx->data);
    if (!context || context->format != AV_PIX_FMT_VIDEOTOOLBOX) return false;
    const auto* descriptor = av_pix_fmt_desc_get(context->sw_format);
    if (!descriptor || descriptor->nb_components != 3 ||
            descriptor->log2_chroma_w != 0 || descriptor->log2_chroma_h != 0) {
        return false;
    }
    for (int component = 0; component < descriptor->nb_components; ++component) {
        if (descriptor->comp[component].depth != 10) return false;
    }
    return true;
}
