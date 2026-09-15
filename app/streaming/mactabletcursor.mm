#include "mactabletcursor.h"
#import <AppKit/AppKit.h>
#import <QuartzCore/QuartzCore.h>

@interface PlankTabletCursorView : NSView
@end
@implementation PlankTabletCursorView
- (BOOL)isFlipped { return YES; }
- (BOOL)isOpaque { return NO; }
- (BOOL)acceptsFirstResponder { return NO; }
- (NSView*)hitTest:(NSPoint)point { (void)point; return nil; }
@end

namespace {
NSView* contentView(SDL_Window* window)
{
    if (!window) return nil;
    auto* native = (__bridge NSWindow*)SDL_GetPointerProperty(
        SDL_GetWindowProperties(window), SDL_PROP_WINDOW_COCOA_WINDOW_POINTER, nullptr);
    return native.contentView;
}
}

class MacTabletCursor::Impl
{
public:
    PlankTabletCursorView* view = nil;
    CALayer* imageLayer = nil;
    QPoint hotspot;
    QPoint position;
    bool hasPosition = false;

    explicit Impl(NSView* parent)
    {
        view = [[PlankTabletCursorView alloc] initWithFrame:parent.bounds];
        view.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
        view.wantsLayer = YES;
        view.layer.masksToBounds = YES;
        view.hidden = YES;
        imageLayer = [[CALayer alloc] init];
        imageLayer.anchorPoint = CGPointZero;
        imageLayer.contentsGravity = kCAGravityResize;
        [view.layer addSublayer:imageLayer];
        [parent addSubview:view positioned:NSWindowAbove relativeTo:nil];
    }

    ~Impl()
    {
        [view removeFromSuperview];
        [imageLayer release];
        [view release];
    }

    void updatePosition()
    {
        [CATransaction begin];
        [CATransaction setDisableActions:YES];
        imageLayer.position = CGPointMake(position.x() - hotspot.x(), position.y() - hotspot.y());
        [CATransaction commit];
    }
};

MacTabletCursor::MacTabletCursor(std::unique_ptr<Impl> impl) : m_Impl(std::move(impl)) {}
MacTabletCursor::~MacTabletCursor() = default;

std::unique_ptr<MacTabletCursor> MacTabletCursor::create(SDL_Window* parentWindow)
{
    SDL_assert(SDL_IsMainThread());
    NSView* parent = contentView(parentWindow);
    if (!parent) return nullptr;
    return std::unique_ptr<MacTabletCursor>(new MacTabletCursor(std::make_unique<Impl>(parent)));
}

bool MacTabletCursor::isAttachedTo(SDL_Window* parentWindow) const
{
    return m_Impl->view.superview == contentView(parentWindow);
}

void MacTabletCursor::dispatchPending()
{
    SDL_assert(SDL_IsMainThread());
    NSView* parent = m_Impl->view.superview;
    // Renderer recreation can append a new opaque Metal view after this
    // overlay was attached. The parent still matches, but video now covers
    // the cursor. Restore sibling order without moving or focusing a window.
    if (parent && parent.subviews.lastObject != m_Impl->view) {
        [parent addSubview:m_Impl->view positioned:NSWindowAbove relativeTo:nil];
    }
    // Native layers commit with AppKit's run loop; cursor motion never waits
    // for a decoded frame and does not make a window key or order it front.
}

void MacTabletCursor::setImage(const QImage& source, int hotspotX, int hotspotY)
{
    SDL_assert(SDL_IsMainThread());
    if (source.isNull()) return;
    const QImage image = source.convertToFormat(QImage::Format_RGBA8888_Premultiplied);
    CFDataRef bytes = CFDataCreate(kCFAllocatorDefault, image.constBits(), image.sizeInBytes());
    if (!bytes) return;
    CGDataProviderRef provider = CGDataProviderCreateWithCFData(bytes);
    CGColorSpaceRef color = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
    CGImageRef native = CGImageCreate(image.width(), image.height(), 8, 32,
        image.bytesPerLine(), color, kCGImageAlphaPremultipliedLast | kCGBitmapByteOrder32Big,
        provider, nullptr, false, kCGRenderingIntentDefault);
    CGColorSpaceRelease(color);
    CGDataProviderRelease(provider);
    CFRelease(bytes);
    if (!native) return;
    [CATransaction begin];
    [CATransaction setDisableActions:YES];
    m_Impl->imageLayer.contents = (__bridge id)native;
    m_Impl->imageLayer.bounds = CGRectMake(0, 0, image.width(), image.height());
    [CATransaction commit];
    CGImageRelease(native);
    m_Impl->hotspot = QPoint(qBound(0, hotspotX, image.width() - 1),
                            qBound(0, hotspotY, image.height() - 1));
    m_Impl->updatePosition();
}

void MacTabletCursor::setPosition(int hotspotX, int hotspotY)
{
    SDL_assert(SDL_IsMainThread());
    m_Impl->position = QPoint(hotspotX, hotspotY);
    m_Impl->hasPosition = true;
    m_Impl->updatePosition();
}

void MacTabletCursor::setVisible(bool visible)
{
    SDL_assert(SDL_IsMainThread());
    m_Impl->view.hidden = !(visible && m_Impl->hasPosition && m_Impl->imageLayer.contents);
}
