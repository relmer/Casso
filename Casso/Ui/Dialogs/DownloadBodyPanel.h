#pragma once

#include "Pch.h"
#include "Core/DxuiPanel.h"


struct DialogInputEvent;
struct DialogPaintContext;




////////////////////////////////////////////////////////////////////////////////
//
//  DownloadBodyPanel
//
//  Paint/input bridge for StartupDownloadDialog's asset rows. Renders them
//  through the caller's PaintBody callback and forwards mouse events to the
//  per-row checkboxes via HandleBodyInput. It draws through the concrete
//  DxuiPainter / DxuiTextRenderer / CassoTheme (the modal host's actual
//  types) that the legacy DialogPaintContext expects, which is why the
//  callbacks take a context rather than the interfaces.
//
////////////////////////////////////////////////////////////////////////////////

class DownloadBodyPanel : public DxuiPanel
{
public:
    void  SetPaintFn (std::function<void (DialogPaintContext &)>     fn) { m_paint = std::move (fn); }
    void  SetInputFn (std::function<void (const DialogInputEvent &)> fn) { m_input = std::move (fn); }

    void  Layout  (const RECT & boundsPx, const DxuiDpiScaler & scaler) override;
    void  Paint   (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;

    // DxuiListView-style local-coordinate translation is not needed here --
    // the row hit-testing in HandleBodyInput works in body-local px, so the
    // event is rebased against this panel's own bounds before dispatch.
    bool  OnMouse (const DxuiMouseEvent & ev) override;

private:
    std::function<void (DialogPaintContext &)>      m_paint;
    std::function<void (const DialogInputEvent &)>  m_input;
    float                                           m_dpiScale = 1.0f;
};
