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
    FillCircleApprox,
};


struct RecordedPaintCall
{
    RecordedPaintKind  kind         = RecordedPaintKind::FillRect;
    float              x            = 0.0f;
    float              y            = 0.0f;
    float              width        = 0.0f;
    float              height       = 0.0f;
    float              thickness    = 0.0f;     // OutlineRect only
    uint32_t           argb         = 0;
    uint32_t           argbSecond   = 0;        // FillGradientRect bottom
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
    void  FillCircleApprox  (float cxPx, float cyPx, float radiusPx, uint32_t argbColor) override;

private:
    std::vector<RecordedPaintCall>  m_calls;
};
