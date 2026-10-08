#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  MockDxuiControl
//
//  Lightweight `IDxuiControl` subclass for DxuiPanel and DxuiFocus-
//  Manager tests. Tracks the count of Layout / Paint / OnMouse /
//  OnKey / OnFocusChanged / OnThemeChanged / Tick invocations.
//  Optionally consumes mouse / key events when `consumeMouse` /
//  `consumeKey` is set.
//
////////////////////////////////////////////////////////////////////////////////

class MockDxuiControl : public IDxuiControl
{
public:
    MockDxuiControl  () { SetFocusable (true); }
    ~MockDxuiControl() override = default;

    int   layoutCount        = 0;
    int   paintCount         = 0;
    int   mouseCount         = 0;
    int   keyCount           = 0;
    int   focusChangedCount  = 0;
    bool  lastFocused        = false;
    int   focusEnteredCount  = 0;
    bool  lastEnteredForward = false;
    int   themeChangedCount  = 0;
    int   tickCount          = 0;

    bool  consumeMouse       = false;
    bool  consumeKey         = false;

    // The painter's clip when last painted, and whether one was set.
    RECT  paintClip          = {};
    bool  wasPaintClipped    = false;

    // PaintAfterSiblings calls, and a log shared by several controls of each
    // Paint (false) and PaintAfterSiblings (true) in the order they ran.
    int                                                     afterCount = 0;
    std::vector<std::pair<const MockDxuiControl *, bool>> * paintLog   = nullptr;

    void  Layout         (const RECT          & boundsDip,
                          const DxuiDpiScaler & /*scaler*/) override
    {
        SetBounds (boundsDip);
        layoutCount++;
    }

    void  Paint          (IDxuiPainter      & painter,
                          IDxuiTextRenderer & /*text*/,
                          const IDxuiTheme  & /*theme*/) override
    {
        wasPaintClipped = painter.GetClipRect (paintClip);
        paintCount++;

        if (paintLog != nullptr)
        {
            paintLog->push_back ({ this, false });
        }
    }

    void  PaintAfterSiblings (IDxuiPainter      & /*painter*/,
                              IDxuiTextRenderer & /*text*/,
                              const IDxuiTheme  & /*theme*/) override
    {
        afterCount++;

        if (paintLog != nullptr)
        {
            paintLog->push_back ({ this, true });
        }
    }

    bool  OnMouse        (const DxuiMouseEvent & /*ev*/) override
    {
        mouseCount++;
        return consumeMouse;
    }

    bool  OnKey          (const DxuiKeyEvent & /*ev*/) override
    {
        keyCount++;
        return consumeKey;
    }

    void  OnFocusChanged (bool focused) override
    {
        focusChangedCount++;
        lastFocused = focused;
    }

    void  OnFocusEntered (bool forward) override
    {
        focusEnteredCount++;
        lastEnteredForward = forward;
    }

    void  OnThemeChanged() override                              { themeChangedCount++; }
    void  Tick           (int64_t /*nowMs*/) override             { tickCount++; }
};
