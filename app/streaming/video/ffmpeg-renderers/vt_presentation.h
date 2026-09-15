#pragma once

#include "streaming/plankpresentation.h"
#include <array>
#include <cstddef>

namespace PlankVT {

// Native window ownership stays with Session. Reject incomplete layouts before
// creating Metal views; a missing output must not become a single-output render.
inline bool validLayout(const PlankPresentationLayout& layout, SDL_Window* primary)
{
    if (!primary || layout.canvasSize.isEmpty() || layout.outputs.isEmpty() ||
            layout.outputs.size() > 2) return false;
    int primaryCount = 0;
    const QRect canvas(QPoint(0, 0), layout.canvasSize);
    for (int i = 0; i < layout.outputs.size(); ++i) {
        const auto& output = layout.outputs[i];
        if (!output.window || output.canvasRect.isEmpty() ||
                !canvas.contains(output.canvasRect) ||
                output.primary != (output.window == primary)) return false;
        if (output.primary) ++primaryCount;
        for (int j = 0; j < i; ++j) {
            if (layout.outputs[j].window == output.window ||
                    layout.outputs[j].canvasRect.intersects(output.canvasRect)) return false;
        }
    }
    if (primaryCount != 1) return false;
    if (layout.outputs.size() == 1) return layout.outputs.first().canvasRect == canvas;
    auto left = layout.outputs[0].canvasRect, right = layout.outputs[1].canvasRect;
    if (left.x() > right.x()) std::swap(left, right);
    return left.x() == 0 && left.y() == 0 && right.y() == 0 &&
            left.width() == right.x() && right.x() + right.width() == canvas.width() &&
            qMax(left.height(), right.height()) == canvas.height();
}

struct Quad {
    QRectF position; // Metal NDC, with y increasing upwards.
    QRectF texture;  // Source UV, with y increasing downwards.
    bool visible = false;
};

// Match float4 + float2 in the production Metal shader, including array stride.
struct alignas(16) Vertex {
    float position[4];
    float texCoord[2];
};
static_assert(sizeof(Vertex) == 32 && offsetof(Vertex, texCoord) == 16);

inline std::array<Vertex, 4> vertices(const Quad& quad)
{
    const float x = quad.position.x(), y = quad.position.y();
    const float right = x + quad.position.width(), top = y + quad.position.height();
    const float u = quad.texture.x(), v = quad.texture.y();
    const float uRight = u + quad.texture.width(), vBottom = v + quad.texture.height();
    return {{{{x, y, 0, 1}, {u, vBottom}},
             {{x, top, 0, 1}, {u, v}},
             {{right, y, 0, 1}, {uRight, vBottom}},
             {{right, top, 0, 1}, {uRight, v}}}};
}

// Use the same canvas intersection as mouse/pen mapping. Drawable density only
// changes rasterization; it must not change which source pixels an output sees.
inline Quad outputQuad(const QSize& stream, const QSize& canvas, const QRect& output)
{
    Quad quad;
    if (stream.isEmpty() || canvas.isEmpty() || output.isEmpty() ||
            !QRect(QPoint(0, 0), canvas).contains(output)) return quad;
    const auto slice = PlankPresentation::sliceForOutput(stream, canvas, output);
    if (!slice.visible) return quad;
    const auto& dst = slice.destinationRect;
    quad.position = QRectF(2.0 * dst.x() / output.width() - 1.0,
                          1.0 - 2.0 * (dst.y() + dst.height()) / output.height(),
                          2.0 * dst.width() / output.width(),
                          2.0 * dst.height() / output.height());
    const auto& src = slice.sourceRect;
    quad.texture = QRectF(src.x() / stream.width(), src.y() / stream.height(),
                         src.width() / stream.width(), src.height() / stream.height());
    quad.visible = true;
    return quad;
}

} // namespace PlankVT
