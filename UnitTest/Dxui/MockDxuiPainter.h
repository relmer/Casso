#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  MockDxuiPainter
//
//  Recording `IDxuiPainter` implementation. Each painter call appends
//  a `RecordedPaintCall` to an internal vector. Tests inspect the log
//  via `Calls()` and clear it with `Reset()`.
//
//  No D3D device is created or required. Every method is allocation-
//  free on the steady state and returns void (matching the interface).
//
////////////////////////////////////////////////////////////////////////////////



enum class RecordedPaintKind
{
    FillRect,
    FillGradientRect,
    OutlineRect,
    OutlineRoundedRect,
    FillRoundedRect,
    FillCircle,
    FillConvexQuad,       // recorded as the quad's bounding box
    FillEllipse,          // recorded as the ellipse's bounding box
    DrawLine,             // recorded as the segment's bounding box
};


struct RecordedPaintCall
{
    RecordedPaintKind  kind         = RecordedPaintKind::FillRect;
    float              x            = 0.0f;
    float              y            = 0.0f;
    float              width        = 0.0f;
    float              height       = 0.0f;
    float              thickness    = 0.0f;     // the outline kinds only
    float              radius       = 0.0f;     // the rounded kinds only
    uint32_t           argb         = 0;
    uint32_t           argbSecond   = 0;        // FillGradientRect bottom
    bool               isClipped    = false;    // a clip was in force
    RECT               clip         = {};
};



class MockDxuiPainter : public IDxuiPainter
{
public:
    MockDxuiPainter  () = default;
    ~MockDxuiPainter() override = default;

    const std::vector<RecordedPaintCall> &  Calls() const { return m_calls; }
    void  Reset() { m_calls.clear(); }

    void  FillRect          (float xPx, float yPx, float widthPx, float heightPx, uint32_t argbColor) override;
    void  FillGradientRect  (float xPx, float yPx, float widthPx, float heightPx, uint32_t argbTop, uint32_t argbBottom) override;
    void  OutlineRect       (float xPx, float yPx, float widthPx, float heightPx, float thicknessPx, uint32_t argbColor) override;
    void  OutlineRoundedRect (float xPx, float yPx, float widthPx, float heightPx, float radiusPx, float thicknessPx, uint32_t argbColor) override;
    void  FillRoundedRect   (float xPx, float yPx, float widthPx, float heightPx, float radiusPx, uint32_t argbColor) override;
    void  FillCircle        (float cxPx, float cyPx, float radiusPx, uint32_t argbColor) override;
    void  FillConvexQuad    (float x0, float y0, float x1, float y1,
                             float x2, float y2, float x3, float y3, uint32_t argbColor) override;
    void  FillEllipse       (float cxPx, float cyPx, float radiusXPx, float radiusYPx, uint32_t argbColor) override;
    void  DrawLine          (float x0, float y0, float x1, float y1, float thicknessPx, uint32_t argbColor) override;

    // The clip in force, which each recorded call carries.
    void  SetClipRect       (const RECT * clipPx) override        { m_hasClip = (clipPx != nullptr); m_clip = m_hasClip ? *clipPx : RECT {}; }
    bool  GetClipRect       (RECT & clipPx) const override        { clipPx = m_clip; return m_hasClip; }

private:
    void  Record (RecordedPaintCall & call);

    std::vector<RecordedPaintCall>  m_calls;
    bool                            m_hasClip = false;
    RECT                            m_clip    = {};
};
